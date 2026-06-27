class DAFServerRoundTimer
{
	private static const int BROADCAST_INTERVAL_MS = 1000;
	private static int s_RoundEndTick;
	private static bool s_Running;

	static void Start(int roundMinutes)
	{
		if (roundMinutes <= 0)
		{
			Stop();
			return;
		}

		s_RoundEndTick = GetGame().GetTickTime() + (roundMinutes * 60);

		if (!s_Running)
		{
			s_Running = true;
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DAFServerRoundTimer.BroadcastTick, BROADCAST_INTERVAL_MS, true);
		}

		Broadcast();
	}

	static void Stop()
	{
		s_RoundEndTick = 0;

		if (s_Running)
		{
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(DAFServerRoundTimer.BroadcastTick);
			s_Running = false;
		}

		BroadcastSeconds(-1);
	}

	static void BroadcastTick()
	{
		int remaining = GetRemainingSeconds();
		BroadcastSeconds(remaining);

		if (remaining <= 0)
			Stop();
	}

	static void Broadcast()
	{
		BroadcastSeconds(GetRemainingSeconds());
	}

	static int GetRemainingSeconds()
	{
		if (s_RoundEndTick <= 0)
			return -1;

		int remaining = s_RoundEndTick - GetGame().GetTickTime();
		if (remaining < 0)
			return 0;

		return remaining;
	}

	static void BroadcastSeconds(int seconds)
	{
		array<Man> players = new array<Man>();
		GetGame().GetWorld().GetPlayerList(players);

		for (int i = 0; i < players.Count(); i++)
		{
			PlayerBase recipient = PlayerBase.Cast(players[i]);
			if (recipient && recipient.GetIdentity())
				recipient.RPCSingleParam(-74700009, new Param1<int>(seconds), true, recipient.GetIdentity());
		}
	}
}
