class DAFDMScore
{
	string id;
	string name;
	int kills;
	int deaths;

	void DAFDMScore(string playerId, string playerName)
	{
		id = playerId;
		name = playerName;
	}
}

class DAFDMScoreboard
{
	private ref array<ref DAFDMScore> m_Scores = new array<ref DAFDMScore>();
	private ref map<string, int> m_TeamScores = new map<string, int>();

	void Reset()
	{
		m_Scores.Clear();
		m_TeamScores.Clear();
		DAFRoundStats.Reset("DAFDeathmatch.Scoreboard.Reset");
	}

	void ResetPlayer(PlayerIdentity identity)
	{
		if (!identity)
			return;

		for (int i = m_Scores.Count() - 1; i >= 0; i--)
		{
			DAFDMScore score = m_Scores[i];
			if (score && score.id == identity.GetId())
				m_Scores.Remove(i);
		}

		DAFRoundStats.ResetPlayer(identity, "DAFDeathmatch.Scoreboard.ResetPlayer");
	}

	void AddKill(PlayerIdentity killer, PlayerIdentity victim)
	{
		if (killer)
		{
			DAFDMScore killerScore = Ensure(killer);
			killerScore.kills++;
		}

		if (victim)
		{
			DAFDMScore victimScore = Ensure(victim);
			victimScore.deaths++;
		}
	}

	void AddDeath(PlayerIdentity victim)
	{
		if (victim)
			Ensure(victim).deaths++;
	}

	void AddTeamKill(string team)
	{
		if (team == "")
			return;

		int score;
		m_TeamScores.Find(team, score);
		m_TeamScores.Set(team, score + 1);
	}

	DAFDMScore Ensure(PlayerIdentity identity)
	{
		foreach (DAFDMScore score: m_Scores)
		{
			if (score && score.id == identity.GetId())
				return score;
		}

		DAFDMScore newScore = new DAFDMScore(identity.GetId(), identity.GetName());
		m_Scores.Insert(newScore);
		return newScore;
	}

	int GetKills(PlayerIdentity identity)
	{
		if (!identity)
			return 0;

		DAFDMScore score = Find(identity.GetId());
		if (!score)
			return 0;

		return score.kills;
	}

	int GetDeaths(PlayerIdentity identity)
	{
		if (!identity)
			return 0;

		DAFDMScore score = Find(identity.GetId());
		if (!score)
			return 0;

		return score.deaths;
	}

	int GetTeamScore(string team)
	{
		if (team == "")
			return 0;

		int score;
		m_TeamScores.Find(team, score);
		return score;
	}

	private DAFDMScore Find(string id)
	{
		foreach (DAFDMScore score: m_Scores)
		{
			if (score && score.id == id)
				return score;
		}

		return null;
	}

	DAFDMScore GetTopPlayerByKills()
	{
		DAFDMScore best;
		foreach (DAFDMScore score: m_Scores)
		{
			if (!score)
				continue;

			if (!best || score.kills > best.kills)
				best = score;
		}

		return best;
	}

	DAFDMScore GetTopPlayerByKD()
	{
		DAFDMScore best;
		foreach (DAFDMScore score: m_Scores)
		{
			if (!score)
				continue;

			if (!best)
			{
				best = score;
				continue;
			}

			if (HasBetterKD(score, best))
				best = score;
		}

		return best;
	}

	bool HasBetterKD(DAFDMScore candidate, DAFDMScore current)
	{
		if (!candidate || !current)
			return false;

		if (candidate.kills <= 0)
			return false;

		bool candidateInfinite = candidate.deaths <= 0;
		bool currentInfinite = current.deaths <= 0;

		if (candidateInfinite && !currentInfinite)
			return true;

		if (!candidateInfinite && currentInfinite)
			return false;

		if (candidateInfinite && currentInfinite)
			return candidate.kills > current.kills;

		return (candidate.kills * current.deaths) > (current.kills * candidate.deaths);
	}

	array<ref DAFDMScore> GetScoresSnapshot()
	{
		array<ref DAFDMScore> snapshot = new array<ref DAFDMScore>();
		foreach (DAFDMScore score: m_Scores)
		{
			if (score)
				snapshot.Insert(score);
		}

		return snapshot;
	}

	array<ref DAFDMScore> GetSortedScoresSnapshot()
	{
		array<ref DAFDMScore> snapshot = GetScoresSnapshot();
		for (int i = 0; i < snapshot.Count() - 1; i++)
		{
			int bestIndex = i;
			for (int j = i + 1; j < snapshot.Count(); j++)
			{
				if (ShouldSortBefore(snapshot.Get(j), snapshot.Get(bestIndex)))
					bestIndex = j;
			}

			if (bestIndex != i)
			{
				DAFDMScore current = snapshot.Get(i);
				snapshot.Set(i, snapshot.Get(bestIndex));
				snapshot.Set(bestIndex, current);
			}
		}

		return snapshot;
	}

	bool ShouldSortBefore(DAFDMScore candidate, DAFDMScore current)
	{
		if (!candidate)
			return false;

		if (!current)
			return true;

		if (candidate.kills != current.kills)
			return candidate.kills > current.kills;

		if (candidate.deaths != current.deaths)
			return candidate.deaths < current.deaths;

		return false;
	}

	bool HasTeamScore(array<string> teamNames)
	{
		if (!teamNames)
			return false;

		foreach (string team: teamNames)
		{
			if (team != "" && GetTeamScore(team) > 0)
				return true;
		}

		return false;
	}
}
