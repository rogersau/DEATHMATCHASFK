modded class PlayerBase
{
	override bool EEOnDamageCalculated(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		DAFDeathmatch deathmatch = GetDAFDeathmatch();
		if (deathmatch && deathmatch.ShouldBlockWarmupInfectedDamage(source))
			return false;

		return super.EEOnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
	}

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
