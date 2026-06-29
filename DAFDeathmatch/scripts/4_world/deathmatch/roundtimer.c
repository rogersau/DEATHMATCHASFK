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

	static void BroadcastTo(PlayerIdentity identity)
	{
		if (!identity)
			return;

		BroadcastSecondsTo(identity, GetRemainingSeconds());
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
		DAFRPC.SendRoundTimeSeconds(seconds);
		DAFRPC.SendRoundLabel(s_RoundLabel);
	}

	static void BroadcastSecondsTo(PlayerIdentity identity, int seconds)
	{
		DAFRPC.SendRoundTimeSecondsTo(identity, seconds);
		DAFRPC.SendRoundLabelTo(identity, s_RoundLabel);
	}
}
