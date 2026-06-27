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

	void Reset()
	{
		m_Scores.Clear();
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

	private DAFDMScore Find(string id)
	{
		foreach (DAFDMScore score: m_Scores)
		{
			if (score && score.id == id)
				return score;
		}

		return null;
	}
}
