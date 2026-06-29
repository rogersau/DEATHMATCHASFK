class DAFDMVote
{
	string voteType;
	string target;
	string label;
	string starterId;
	string starterName;
	int expiresAt;
	ref map<string, bool> votes;

	void DAFDMVote()
	{
		votes = new map<string, bool>();
	}

	void SetVote(string playerId, bool yes)
	{
		votes.Set(playerId, yes);
	}

	bool HasVoted(string playerId)
	{
		bool existing;
		return votes.Find(playerId, existing);
	}

	int CountYes()
	{
		int count = 0;
		for (int i = 0; i < votes.Count(); i++)
		{
			if (votes.GetElement(i))
				count++;
		}

		return count;
	}

	int CountNo()
	{
		return votes.Count() - CountYes();
	}

	int CountTotal()
	{
		return votes.Count();
	}
}

class DAFDMVoteManager
{
	static const string TYPE_END_ROUND = "endround";
	static const string TYPE_ARENA = "arena";
	static const string TYPE_ROUND_TYPE = "roundtype";

	private ref DAFDMVote m_ActiveVote;

	bool HasActiveVote()
	{
		return m_ActiveVote != null;
	}

	bool StartVote(PlayerIdentity starter, string voteType, string target, string label, int durationSeconds)
	{
		if (!starter || m_ActiveVote)
			return false;

		m_ActiveVote = new DAFDMVote();
		m_ActiveVote.voteType = voteType;
		m_ActiveVote.target = target;
		m_ActiveVote.label = label;
		m_ActiveVote.starterId = starter.GetId();
		m_ActiveVote.starterName = starter.GetName();
		m_ActiveVote.expiresAt = GetGame().GetTime() + (durationSeconds * 1000);
		return true;
	}

	bool CastVote(PlayerIdentity identity, bool yes, int playerCount, out bool passed, out string voteType, out string target, out string label, out string message)
	{
		passed = false;
		voteType = "";
		target = "";
		label = "";
		message = "";

		if (!m_ActiveVote)
		{
			message = "No vote is active";
			return false;
		}

		if (!identity)
		{
			message = "Cannot vote without a player identity";
			return false;
		}

		string playerId = identity.GetId();
		if (m_ActiveVote.HasVoted(playerId))
		{
			message = "You have already voted";
			return false;
		}

		m_ActiveVote.SetVote(playerId, yes);
		message = BuildProgressMessage();

		if (HasCurrentMajority(playerCount))
			return FinishVote(playerCount, passed, voteType, target, label, message);

		return false;
	}

	bool CheckExpiredOrMajority(int playerCount, out bool passed, out string voteType, out string target, out string label, out string message)
	{
		passed = false;
		voteType = "";
		target = "";
		label = "";
		message = "";

		if (!m_ActiveVote)
			return false;

		if (HasCurrentMajority(playerCount))
			return FinishVote(playerCount, passed, voteType, target, label, message);

		if (GetGame().GetTime() < m_ActiveVote.expiresAt)
			return false;

		return FinishVote(playerCount, passed, voteType, target, label, message);
	}

	void Cancel(out string message)
	{
		message = "";
		if (!m_ActiveVote)
			return;

		message = "Vote cancelled: " + m_ActiveVote.label;
		m_ActiveVote = null;
	}

	string GetActiveSummary()
	{
		if (!m_ActiveVote)
			return "No active vote";

		int secondsLeft = Math.Max((m_ActiveVote.expiresAt - GetGame().GetTime()) / 1000, 0);
		return string.Format("Vote: %1 | Yes %2 / No %3 | %4s left", m_ActiveVote.label, m_ActiveVote.CountYes(), m_ActiveVote.CountNo(), secondsLeft);
	}

	private bool HasCurrentMajority(int playerCount)
	{
		if (!m_ActiveVote || playerCount < 1)
			return false;

		int required = (playerCount / 2) + 1;
		return m_ActiveVote.CountYes() >= required || m_ActiveVote.CountNo() >= required;
	}

	private bool FinishVote(int playerCount, out bool passed, out string voteType, out string target, out string label, out string message)
	{
		voteType = m_ActiveVote.voteType;
		target = m_ActiveVote.target;
		label = m_ActiveVote.label;

		int yes = m_ActiveVote.CountYes();
		int no = m_ActiveVote.CountNo();
		int total = m_ActiveVote.CountTotal();
		passed = total > (playerCount / 2) && yes > no;

		if (passed)
			message = string.Format("Vote passed: %1 (%2 yes / %3 no)", label, yes, no);
		else
			message = string.Format("Vote failed: %1 (%2 yes / %3 no)", label, yes, no);

		m_ActiveVote = null;
		return true;
	}

	private string BuildProgressMessage()
	{
		if (!m_ActiveVote)
			return "";

		return string.Format("Vote tally: %1 (%2 yes / %3 no)", m_ActiveVote.label, m_ActiveVote.CountYes(), m_ActiveVote.CountNo());
	}
}
