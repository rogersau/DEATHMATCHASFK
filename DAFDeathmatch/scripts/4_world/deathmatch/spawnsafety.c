/**
 * DAFDMSpawnSafety owns spawn selection that avoids unfair spawn moments.
 *
 * Extracted from the DAFDeathmatch god class. Previously this was a pure
 * evaluator smeared across ~230 lines of the manager (PickSpawnCandidate,
 * AnalyzeSpawn, IsLookingAtSpawn, IsEnemyForSpawn, ...) beside death drops,
 * loadouts, and arena walls. This module concentrates it behind one interface.
 *
 * It is a pure evaluator: given an arena, a spawning identity, and the live
 * player list, it scores every configured spawn and returns the best safe one
 * (or the least-bad fallback when every spawn is pressured). It has no state
 * of its own and mutates nothing on the server.
 *
 * Dependencies are passed in:
 *   - settings: the five spawn-safety tuning fields.
 *   - teams:    for team-aware enemy detection during TDM.
 *
 * CONTEXT.md "Spawn Safety And Team Modes": reject spawns too close to alive
 * enemies, reject spawns inside an enemy's forward view cone, and if every
 * authored spawn is pressured choose the least-bad spawn rather than failing.
 */
class DAFDMSpawnCandidate
{
	int index = -1;
	int totalSpawns = 0;
	vector position;
	float score = 0;
	bool rejected = false;
	bool fallback = false;
	string reason = "";
	int playerCount = 0;
	int enemyCount = 0;
	int friendlyCount = 0;
	int closePlayerCount = 0;
	int closeEnemyCount = 0;
	int viewThreatCount = 0;
	float nearestPlayerDistance = -1;
	float nearestEnemyDistance = -1;
	string nearestPlayerName = "none";
	string nearestEnemyName = "none";
}

class DAFDMSpawnSafety
{
	private ref DAFDMSettings m_Settings;
	private ref DAFDMTeams m_Teams;

	void DAFDMSpawnSafety(DAFDMSettings settings, DAFDMTeams teams)
	{
		m_Settings = settings;
		m_Teams = teams;
	}

	/**
	 * Score every configured spawn in the arena and return the best safe one.
	 * Returns null if the arena is null. When spawn safety is disabled or no
	 * spawns are configured, returns a random spawn.
	 * includeRandom adds jitter to the score so equally-safe spawns vary.
	 * ignorePlayerIds lists player ids to skip entirely (test dummies).
	 */
	DAFDMSpawnCandidate Pick(PlayerIdentity identity, PlayerBase currentPlayer, DAFDMArena arena, string gameMode, bool includeRandom, array<string> ignorePlayerIds)
	{
		if (!arena)
			return null;

		if (!m_Settings.enableSpawnSafety)
			return MakeRandomSpawnCandidate(arena, "spawn safety disabled");

		array<vector> spawns = arena.GetPlayerSpawns();
		if (!spawns || spawns.Count() == 0)
			return MakeRandomSpawnCandidate(arena, "no configured player spawns");

		float bestScore = -1000000;
		bool foundSafe = false;
		DAFDMSpawnCandidate bestCandidate;

		for (int i = 0; i < spawns.Count(); i++)
		{
			DAFDMSpawnCandidate candidate = AnalyzeSpawn(arena, spawns[i], i, spawns.Count(), identity, currentPlayer, gameMode, includeRandom, ignorePlayerIds);
			if (!candidate.rejected && (!foundSafe || candidate.score > bestScore))
			{
				bestCandidate = candidate;
				bestScore = candidate.score;
				foundSafe = true;
			}
			else if (!foundSafe && candidate.score > bestScore)
			{
				bestCandidate = candidate;
				bestScore = candidate.score;
			}
		}

		if (bestCandidate)
		{
			bestCandidate.fallback = !foundSafe;
			if (bestCandidate.fallback && bestCandidate.reason == "")
				bestCandidate.reason = "all spawns pressured; chose least-bad spawn";
			else if (!bestCandidate.fallback && bestCandidate.reason == "")
				bestCandidate.reason = "safe spawn selected";
		}

		return bestCandidate;
	}

	private DAFDMSpawnCandidate MakeRandomSpawnCandidate(DAFDMArena arena, string reason)
	{
		array<vector> spawns = arena.GetPlayerSpawns();
		if (!spawns || spawns.Count() == 0)
		{
			DAFDMSpawnCandidate centerCandidate = new DAFDMSpawnCandidate();
			centerCandidate.index = -1;
			centerCandidate.totalSpawns = 0;
			centerCandidate.position = arena.GetRandomPlayerSpawn();
			centerCandidate.reason = reason;
			return centerCandidate;
		}

		int index = Math.RandomInt(0, spawns.Count());
		DAFDMSpawnCandidate candidate = new DAFDMSpawnCandidate();
		candidate.index = index;
		candidate.totalSpawns = spawns.Count();
		candidate.position = DAFDMArena.SnapToGround(spawns[index]);
		candidate.reason = reason;
		return candidate;
	}

	private DAFDMSpawnCandidate AnalyzeSpawn(DAFDMArena arena, vector rawSpawn, int spawnIndex, int totalSpawns, PlayerIdentity identity, PlayerBase currentPlayer, string gameMode, bool includeRandom, array<string> ignorePlayerIds)
	{
		DAFDMSpawnCandidate candidate = new DAFDMSpawnCandidate();
		candidate.index = spawnIndex;
		candidate.totalSpawns = totalSpawns;
		candidate.position = DAFDMArena.SnapToGround(rawSpawn);
		candidate.nearestPlayerDistance = -1;
		candidate.nearestEnemyDistance = -1;

		array<Man> players = new array<Man>();
		GetGame().GetWorld().GetPlayerList(players);

		foreach (Man man: players)
		{
			PlayerBase other = PlayerBase.Cast(man);
			if (!other || other == currentPlayer || !other.IsAlive())
				continue;

			if (IsIgnored(other, ignorePlayerIds))
				continue;

			if (IsSameIdentity(identity, other.GetIdentity()))
				continue;

			candidate.playerCount++;
			float distance = vector.Distance(candidate.position, other.GetPosition());
			if (candidate.nearestPlayerDistance < 0 || distance < candidate.nearestPlayerDistance)
			{
				candidate.nearestPlayerDistance = distance;
				candidate.nearestPlayerName = GetPlayerDebugName(other);
			}

			if (m_Settings.spawnSafetyMinPlayerDistance > 0 && distance < m_Settings.spawnSafetyMinPlayerDistance)
			{
				candidate.closePlayerCount++;
				candidate.rejected = true;
			}

			if (!IsEnemyForSpawn(identity, other, gameMode))
			{
				candidate.friendlyCount++;
				candidate.score += Math.Min(distance, m_Settings.spawnSafetyEnemyViewDistance) * 0.2;
				continue;
			}

			candidate.enemyCount++;
			if (candidate.nearestEnemyDistance < 0 || distance < candidate.nearestEnemyDistance)
			{
				candidate.nearestEnemyDistance = distance;
				candidate.nearestEnemyName = GetPlayerDebugName(other);
			}

			candidate.score += Math.Min(distance, m_Settings.spawnSafetyEnemyViewDistance);
			if (m_Settings.spawnSafetyMinEnemyDistance > 0 && distance < m_Settings.spawnSafetyMinEnemyDistance)
			{
				candidate.score -= 10000;
				candidate.closeEnemyCount++;
				candidate.rejected = true;
			}

			if (IsLookingAtSpawn(other, candidate.position, distance))
			{
				candidate.score -= 5000;
				candidate.viewThreatCount++;
				candidate.rejected = true;
			}
		}

		if (candidate.rejected)
			candidate.reason = BuildSpawnRejectReason(candidate);

		if (includeRandom)
			candidate.score += Math.RandomFloat(0, 1);

		return candidate;
	}

	private string BuildSpawnRejectReason(DAFDMSpawnCandidate candidate)
	{
		string reason = "";
		if (candidate.closePlayerCount > 0)
			reason = "player too close";

		if (candidate.closeEnemyCount > 0)
		{
			if (reason != "")
				reason += ", ";
			reason += "enemy too close";
		}

		if (candidate.viewThreatCount > 0)
		{
			if (reason != "")
				reason += ", ";
			reason += "enemy looking at spawn";
		}

		if (reason == "")
			return "pressured";

		return reason;
	}

	private string GetPlayerDebugName(PlayerBase player)
	{
		if (!player || !player.GetIdentity())
			return "unknown";

		return player.GetIdentity().GetName();
	}

	private bool IsLookingAtSpawn(PlayerBase enemy, vector spawn, float distance)
	{
		if (!enemy || m_Settings.spawnSafetyEnemyViewDistance <= 0 || distance > m_Settings.spawnSafetyEnemyViewDistance)
			return false;

		if (distance < 0.1)
			return true;

		vector enemyDirection = enemy.GetDirection();
		enemyDirection[1] = 0;
		enemyDirection = enemyDirection.Normalized();

		vector toSpawn = vector.Direction(enemy.GetPosition(), spawn);
		toSpawn[1] = 0;
		toSpawn = toSpawn.Normalized();

		float threshold = Math.Cos(m_Settings.spawnSafetyEnemyViewAngleDegrees * Math.DEG2RAD);
		return vector.Dot(enemyDirection, toSpawn) >= threshold;
	}

	private bool IsSameIdentity(PlayerIdentity left, PlayerIdentity right)
	{
		if (!left || !right)
			return false;

		return left.GetId() == right.GetId();
	}

	private bool IsEnemyForSpawn(PlayerIdentity identity, PlayerBase other, string gameMode)
	{
		if (!DAFDMTeams.IsTeamMode(gameMode))
			return true;

		string ownTeam = m_Teams.GetTeam(identity, gameMode);
		string otherTeam = m_Teams.GetTeam(other.GetIdentity(), gameMode);
		if (ownTeam == "" || otherTeam == "")
			return true;

		return ownTeam != otherTeam;
	}

	private bool IsIgnored(PlayerBase player, array<string> ignorePlayerIds)
	{
		if (!player || !player.GetIdentity() || !ignorePlayerIds)
			return false;

		return ignorePlayerIds.Find(player.GetIdentity().GetId()) >= 0;
	}
}
