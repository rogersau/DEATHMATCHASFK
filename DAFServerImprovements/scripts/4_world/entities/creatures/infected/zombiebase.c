modded class ZombieBase
{
	override void EEInit()
	{
		super.EEInit();

		DAFServerInfectedCleanup.Register(this);
		DAFServerInfectedCleanup.CleanupIfNeeded("ZombieBase.EEInit");
	}

	override void EEKilled(Object killer)
	{
		DAFServerInfectedCleanup.Unregister(this);

		super.EEKilled(killer);
	}

	override void EEDelete(EntityAI parent)
	{
		DAFServerInfectedCleanup.Unregister(this);

		super.EEDelete(parent);
	}
}
