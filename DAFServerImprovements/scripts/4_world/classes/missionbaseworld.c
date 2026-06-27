modded class MissionBaseWorld
{
	override void OnRoundStart(DMArena arena)
	{
		super.OnRoundStart(arena);

		DAFServer_ResetRoundStats("MissionBaseWorld.OnRoundStart");
	}

	override void OnRoundEnd(DMArena arena)
	{
		super.OnRoundEnd(arena);

		DAFServer_ResetRoundStats("MissionBaseWorld.OnRoundEnd");
	}
}

void DAFServer_ResetRoundStats(string source)
{
	KillFeedHandle handle = GetKillFeedHandle();
	if (!handle)
	{
		Print("DAFServer: unable to reset round stats from " + source + " because KillFeedHandle is missing");
		return;
	}

	Print("DAFServer: resetting round stats from " + source);
	handle.ResetRoundStats();
	DAFServerDiscordStats.Reset();
}
