class DAFDMSeasonPlayer
{
	string id;
	string name;
	int points;
	int kills;
	int headshots;
	int uncons;
	int assists;

	void DAFDMSeasonPlayer(string playerId = "", string playerName = "")
	{
		id = playerId;
		name = playerName;
	}
}

class DAFDMSeasonDamageEntry
{
	string victimId;
	string attackerId;
	string attackerName;
	int lastDamageAt;

	void DAFDMSeasonDamageEntry(string victim, string attacker, string attackerDisplayName, int time)
	{
		victimId = victim;
		attackerId = attacker;
		attackerName = attackerDisplayName;
		lastDamageAt = time;
	}
}

class DAFDMSeasonStore
{
	int seasonStartedDayKey;
	int lastWeeklySummaryDayKey;
	ref array<ref DAFDMSeasonPlayer> players = new array<ref DAFDMSeasonPlayer>();
}

class DAFDMSeason
{
	private ref DAFDMSeasonStore m_Store = new DAFDMSeasonStore();
	private ref array<ref DAFDMSeasonDamageEntry> m_Damage = new array<ref DAFDMSeasonDamageEntry>();

	void Load()
	{
		if (!FileExist(DAFDMFilenames.DIRECTORY))
			MakeDirectory(DAFDMFilenames.DIRECTORY);

		if (FileExist(DAFDMFilenames.SEASON_JSON))
		{
			JsonFileLoader<ref DAFDMSeasonStore>.JsonLoadFile(DAFDMFilenames.SEASON_JSON, m_Store);
			if (!m_Store.players)
				m_Store.players = new array<ref DAFDMSeasonPlayer>();
			MigrateAssistCounts();
		}
		else
		{
			m_Store.seasonStartedDayKey = GetTodayKey();
			m_Store.lastWeeklySummaryDayKey = m_Store.seasonStartedDayKey;
			Save();
		}

		PrintFormat("DAFDeathmatch: season loaded. players=%1 points=%2", m_Store.players.Count(), GetTotalPoints());
	}

	void Save()
	{
		JsonFileLoader<ref DAFDMSeasonStore>.JsonSaveFile(DAFDMFilenames.SEASON_JSON, m_Store);
	}

	void EnsurePlayer(PlayerIdentity identity)
	{
		if (!identity)
			return;

		Ensure(identity.GetId(), identity.GetName());
		Save();
	}

	void RegisterDamage(PlayerBase victim, PlayerBase attacker, int assistWindowSeconds)
	{
		if (!victim || !attacker || victim == attacker || !victim.GetIdentity() || !attacker.GetIdentity())
			return;

		RegisterDamageById(victim.GetIdentity().GetId(), attacker.GetIdentity().GetId(), attacker.GetIdentity().GetName(), assistWindowSeconds);
	}

	int AwardKill(PlayerIdentity killer, bool headshot, DAFDMSettings settings)
	{
		if (!settings || !settings.enableSeasonScoring || !killer)
			return 0;

		DAFDMSeasonPlayer player = Ensure(killer.GetId(), killer.GetName());
		int awarded = settings.seasonKillPoints;
		player.points += awarded;
		player.kills++;
		if (headshot)
		{
			awarded += settings.seasonHeadshotBonusPoints;
			player.points += settings.seasonHeadshotBonusPoints;
			player.headshots++;
		}

		Save();
		PrintFormat("DAFDeathmatch: season kill points player=%1 points=%2 rank=%3 headshot=%4", player.id, player.points, GetRankById(player.id), headshot);
		return awarded;
	}

	void AwardUncons(PlayerIdentity victim, PlayerIdentity killer, DAFDMSettings settings, TStringArray unconIds)
	{
		if (!settings || !settings.enableSeasonScoring || !victim || !unconIds)
			return;

		string victimId = victim.GetId();
		string killerId = "";
		if (killer)
			killerId = killer.GetId();

		int now = GetGame().GetTime();
		bool changed = false;
		for (int i = m_Damage.Count() - 1; i >= 0; i--)
		{
			DAFDMSeasonDamageEntry entry = m_Damage[i];
			if (!entry || entry.victimId != victimId)
				continue;

			m_Damage.Remove(i);
			if (entry.attackerId == "" || entry.attackerId == killerId)
				continue;

			if ((now - entry.lastDamageAt) > settings.seasonAssistWindowSeconds * 1000)
				continue;

			DAFDMSeasonPlayer unconPlayer = Ensure(entry.attackerId, entry.attackerName);
			unconPlayer.points += settings.seasonAssistPoints;
			unconPlayer.uncons++;
			unconIds.Insert(unconPlayer.id);
			changed = true;
			PrintFormat("DAFDeathmatch: season uncon points player=%1 points=%2 rank=%3", unconPlayer.id, unconPlayer.points, GetRankById(unconPlayer.id));
		}

		if (changed)
			Save();
	}

	int GetPoints(PlayerIdentity identity)
	{
		if (!identity)
			return 0;

		DAFDMSeasonPlayer player = Find(identity.GetId());
		if (!player)
			return 0;

		return player.points;
	}

	int GetRank(PlayerIdentity identity)
	{
		if (!identity)
			return 0;

		return GetRankById(identity.GetId());
	}

	int GetRankById(string playerId)
	{
		DAFDMSeasonPlayer player = Find(playerId);
		if (!player)
			return 0;

		int rank = 1;
		foreach (DAFDMSeasonPlayer other: m_Store.players)
		{
			if (other && other.id != playerId && HasBetterRank(other, player))
				rank++;
		}

		return rank;
	}

	int GetPlayerCount()
	{
		return m_Store.players.Count();
	}

	void MaybePostWeeklySummary(DAFDMSettings settings, DAFDMDiscord discord)
	{
		if (!settings || !settings.enableDiscordSeasonSummary || !discord)
			return;

		int today = GetTodayKey();
		if (m_Store.lastWeeklySummaryDayKey <= 0)
		{
			m_Store.lastWeeklySummaryDayKey = today;
			Save();
			return;
		}

		if (today - m_Store.lastWeeklySummaryDayKey < settings.seasonSummaryIntervalDays)
			return;

		if (!HasScoredPlayers())
		{
			m_Store.lastWeeklySummaryDayKey = today;
			Save();
			return;
		}

		discord.PostSeasonSummary(settings, BuildSummary(settings.seasonSummaryTopPlayers));
		m_Store.lastWeeklySummaryDayKey = today;
		Save();
	}

	string BuildSummary(int maxPlayers)
	{
		if (maxPlayers < 1)
			maxPlayers = 10;

		string summary = "";
		TStringArray used = new TStringArray();
		for (int rank = 1; rank <= maxPlayers; rank++)
		{
			DAFDMSeasonPlayer next = PickNextBest(used);
			if (!next || next.points <= 0)
				break;

			used.Insert(next.id);
			summary += string.Format("%1. **%2** - **%3** pts (%4 K / %5 HS / %6 U)\\n", rank, next.name, next.points, next.kills, next.headshots, next.uncons);
		}

		if (summary == "")
			summary = "_No season points recorded yet._";

		return summary;
	}

	private void RegisterDamageById(string victimId, string attackerId, string attackerName, int assistWindowSeconds)
	{
		if (victimId == "" || attackerId == "")
			return;

		int now = GetGame().GetTime();
		int cutoff = now - (assistWindowSeconds * 1000);
		for (int i = m_Damage.Count() - 1; i >= 0; i--)
		{
			DAFDMSeasonDamageEntry entry = m_Damage[i];
			if (!entry || entry.lastDamageAt < cutoff)
			{
				m_Damage.Remove(i);
				continue;
			}

			if (entry.victimId == victimId && entry.attackerId == attackerId)
			{
				entry.attackerName = attackerName;
				entry.lastDamageAt = now;
				return;
			}
		}

		m_Damage.Insert(new DAFDMSeasonDamageEntry(victimId, attackerId, attackerName, now));
	}

	private DAFDMSeasonPlayer Ensure(string playerId, string playerName)
	{
		DAFDMSeasonPlayer existing = Find(playerId);
		if (existing)
		{
			if (playerName != "")
				existing.name = playerName;
			return existing;
		}

		DAFDMSeasonPlayer created = new DAFDMSeasonPlayer(playerId, playerName);
		m_Store.players.Insert(created);
		return created;
	}

	private DAFDMSeasonPlayer Find(string playerId)
	{
		if (playerId == "")
			return null;

		foreach (DAFDMSeasonPlayer player: m_Store.players)
		{
			if (player && player.id == playerId)
				return player;
		}

		return null;
	}

	private bool HasBetterRank(DAFDMSeasonPlayer left, DAFDMSeasonPlayer right)
	{
		if (!left || !right)
			return false;

		if (left.points != right.points)
			return left.points > right.points;

		if (left.kills != right.kills)
			return left.kills > right.kills;

		return left.headshots > right.headshots;
	}

	private DAFDMSeasonPlayer PickNextBest(TStringArray used)
	{
		DAFDMSeasonPlayer best;
		foreach (DAFDMSeasonPlayer candidate: m_Store.players)
		{
			if (!candidate || used.Find(candidate.id) >= 0)
				continue;

			if (!best || HasBetterRank(candidate, best))
				best = candidate;
		}

		return best;
	}

	private bool HasScoredPlayers()
	{
		foreach (DAFDMSeasonPlayer player: m_Store.players)
		{
			if (player && player.points > 0)
				return true;
		}

		return false;
	}

	private void MigrateAssistCounts()
	{
		bool changed = false;
		foreach (DAFDMSeasonPlayer player: m_Store.players)
		{
			if (player && player.uncons <= 0 && player.assists > 0)
			{
				player.uncons = player.assists;
				changed = true;
			}
		}

		if (changed)
			Save();
	}

	private int GetTotalPoints()
	{
		int total = 0;
		foreach (DAFDMSeasonPlayer player: m_Store.players)
		{
			if (player)
				total += player.points;
		}

		return total;
	}

	private int GetTodayKey()
	{
		int year;
		int month;
		int day;
		GetYearMonthDay(year, month, day);
		return (year * 372) + (month * 31) + day;
	}
}
