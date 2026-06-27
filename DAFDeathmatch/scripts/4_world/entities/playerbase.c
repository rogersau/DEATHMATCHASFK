modded class PlayerBase
{
	override void EEItemIntoHands(EntityAI item)
	{
		super.EEItemIntoHands(item);

		DAFDeathmatch deathmatch = GetDAFDeathmatch();
		if (deathmatch)
			deathmatch.OnPlayerPickedUpItem(this, item);
	}

	override void EEKilled(Object killer)
	{
		PlayerBase killerPlayer;
		EntityAI killerEntity = EntityAI.Cast(killer);
		if (killerEntity)
			killerPlayer = PlayerBase.Cast(killerEntity.GetHierarchyRootPlayer());

		DAFDeathmatch deathmatch = GetDAFDeathmatch();
		if (deathmatch)
			deathmatch.OnPlayerKilled(this, killerPlayer, killerEntity, DAF_WasLastHitHeadshot());

		super.EEKilled(killer);
	}
}
