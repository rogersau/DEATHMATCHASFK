/**
 * DAFDMSurvivalNormalizer keeps deathmatch players in a clean survival state.
 *
 * Spawns already give full gear; this module makes sure food, water, body
 * temperature, and illnesses stay neutral too. It runs once on every player
 * spawn and then on a recurring 60-second tick for the lifetime of the round.
 */
class DAFDMSurvivalNormalizer
{
	private static const int TICK_INTERVAL_MS = 60000;
	private static bool s_TickStarted = false;

	/** Start the recurring normalization tick. Safe to call multiple times. */
	static void StartTick()
	{
		if (s_TickStarted)
			return;

		s_TickStarted = true;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DAFDMSurvivalNormalizer.Tick, TICK_INTERVAL_MS, true);
	}

	/** Stop the recurring tick. */
	static void StopTick()
	{
		if (!s_TickStarted)
			return;

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(DAFDMSurvivalNormalizer.Tick);
		s_TickStarted = false;
	}

	/** Normalize a single player. */
	static void NormalizePlayer(PlayerBase player)
	{
		if (!player)
			return;

		player.DAF_NormalizeSurvivalStats();
	}

	/** Normalize every alive, connected player. */
	static void NormalizeAllPlayers()
	{
		array<Man> players = new array<Man>();
		GetGame().GetPlayers(players);

		foreach (Man man: players)
		{
			PlayerBase player = PlayerBase.Cast(man);
			if (player && player.GetIdentity() && player.IsAlive())
				NormalizePlayer(player);
		}
	}

	static void Tick()
	{
		NormalizeAllPlayers();
	}
}
