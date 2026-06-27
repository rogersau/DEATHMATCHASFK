class DAFDMRoundTimer
{
	private static const int BROADCAST_INTERVAL_MS = 1000;
	private static int s_RoundEndTick;
	private static bool s_Running;
	private static string s_RoundLabel;

	static void Start(int roundMinutes, string roundLabel)
	{
		s_RoundLabel = roundLabel;
		s_RoundEndTick = GetGame().GetTickTime() + (roundMinutes * 60);

		if (!s_Running)
		{
			s_Running = true;
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DAFDMRoundTimer.BroadcastTick, BROADCAST_INTERVAL_MS, true);
		}

		Broadcast();
	}

	static void Stop()
	{
		s_RoundEndTick = 0;
		s_RoundLabel = "";
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(DAFDMRoundTimer.BroadcastTick);
		s_Running = false;
		BroadcastSeconds(-1);
	}

	static void BroadcastTick()
	{
		int remaining = GetRemainingSeconds();
		BroadcastSeconds(remaining);
	}

	static void Broadcast()
	{
		BroadcastSeconds(GetRemainingSeconds());
	}

	static int GetRemainingSeconds()
	{
		if (s_RoundEndTick <= 0)
			return -1;

		return Math.Max(0, s_RoundEndTick - GetGame().GetTickTime());
	}

	static string GetRemainingText()
	{
		int seconds = GetRemainingSeconds();
		if (seconds < 0)
			return "no round is active";

		int minutes = seconds / 60;
		int remainder = seconds % 60;
		string remainderText = remainder.ToString();
		if (remainder < 10)
			remainderText = "0" + remainderText;

		return minutes.ToString() + ":" + remainderText;
	}

	static void BroadcastSeconds(int seconds)
	{
		array<Man> players = new array<Man>();
		GetGame().GetWorld().GetPlayerList(players);

		foreach (Man man: players)
		{
			PlayerBase recipient = PlayerBase.Cast(man);
			if (recipient && recipient.GetIdentity())
			{
				recipient.RPCSingleParam(-74700009, new Param1<int>(seconds), true, recipient.GetIdentity());
				recipient.RPCSingleParam(-74700010, new Param1<string>(s_RoundLabel), true, recipient.GetIdentity());
			}
		}
	}
}
