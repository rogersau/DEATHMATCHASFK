/**
 * Keeps fall-broken legs temporary without coupling that behavior to the
 * broader 60-second survival stat normalizer.
 */
class DAFDMBrokenLegRepair
{
	private static const int TICK_INTERVAL_MS = 15000;
	private static bool s_TickStarted = false;

	static void StartTick()
	{
		if (s_TickStarted)
			return;

		s_TickStarted = true;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DAFDMBrokenLegRepair.Tick, TICK_INTERVAL_MS, true);
	}

	static void StopTick()
	{
		if (!s_TickStarted)
			return;

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(DAFDMBrokenLegRepair.Tick);
		s_TickStarted = false;
	}

	static void RepairPlayer(PlayerBase player)
	{
		if (!player || !player.IsAlive() || player.GetBrokenLegs() == eBrokenLegs.NO_BROKEN_LEGS)
			return;

		player.SetHealth("RightLeg", "Health", player.GetMaxHealth("RightLeg", "Health"));
		player.SetHealth("RightFoot", "Health", player.GetMaxHealth("RightFoot", "Health"));
		player.SetHealth("LeftLeg", "Health", player.GetMaxHealth("LeftLeg", "Health"));
		player.SetHealth("LeftFoot", "Health", player.GetMaxHealth("LeftFoot", "Health"));

		if (player.GetModifiersManager() && player.GetModifiersManager().IsModifierActive(eModifiers.MDF_BROKEN_LEGS))
			player.GetModifiersManager().DeactivateModifier(eModifiers.MDF_BROKEN_LEGS);

		player.SetBrokenLegs(eBrokenLegs.NO_BROKEN_LEGS);
	}

	static void RepairAllPlayers()
	{
		array<Man> players = new array<Man>();
		GetGame().GetPlayers(players);

		foreach (Man man: players)
		{
			PlayerBase player = PlayerBase.Cast(man);
			if (player && player.GetIdentity())
				RepairPlayer(player);
		}
	}

	static void Tick()
	{
		RepairAllPlayers();
	}
}
