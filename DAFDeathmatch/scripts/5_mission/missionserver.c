modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();

		if (!g_DAFDeathmatch)
		{
			g_DAFDeathmatch = new DAFDeathmatch();
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(g_DAFDeathmatch.Start, 5000, false);
		}
	}

	override void OnEvent(EventType eventTypeId, Param params)
	{
		super.OnEvent(eventTypeId, params);

		if (g_DAFDeathmatch)
			g_DAFDeathmatch.OnEvent(eventTypeId, params);
	}

	override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
	{
		super.InvokeOnConnect(player, identity);

		if (g_DAFDeathmatch)
		{
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(g_DAFDeathmatch.OnPlayerConnected, 1000, false, player);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DAFDMRoundTimer.BroadcastTo, 1000, false, identity);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DAFDMRoundTimer.BroadcastTo, 3000, false, identity);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DAFDMRoundTimer.BroadcastTo, 6000, false, identity);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DAFDMRoundTimer.BroadcastTo, 10000, false, identity);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DAFDMRoundTimer.BroadcastTo, 15000, false, identity);
		}
	}
}
