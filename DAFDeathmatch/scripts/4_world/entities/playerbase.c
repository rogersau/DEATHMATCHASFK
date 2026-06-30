modded class PlayerBase
{
	private bool m_DAFDM_BlockUnconsciousHandDrop;

	protected bool DAFDM_IsUnconsciousnessEnabled()
	{
		DAFDMSettings settings = DAFDMSettings.Instance();
		if (!settings)
			return true;

		return settings.enableUnconsciousness;
	}

	protected void DAFDM_FillShock()
	{
		SetHealth("", "Shock", GetMaxHealth("", "Shock"));
	}

	protected bool DAFDM_HasBrokenLegRisk()
	{
		if (GetBrokenLegs() != eBrokenLegs.NO_BROKEN_LEGS)
			return true;

		if (GetHealth("RightLeg", "Health") <= 1 || GetHealth("LeftLeg", "Health") <= 1)
			return true;

		return GetHealth("RightFoot", "Health") <= 1 || GetHealth("LeftFoot", "Health") <= 1;
	}

	protected void DAFDM_ProtectLegZoneFromGunBreak(string zone)
	{
		if (GetHealth(zone, "Health") <= 1)
			SetHealth(zone, "Health", 2);
	}

	protected void DAFDM_PreventGunshotBrokenLegs()
	{
		DAFDM_ProtectLegZoneFromGunBreak("RightLeg");
		DAFDM_ProtectLegZoneFromGunBreak("LeftLeg");
		DAFDM_ProtectLegZoneFromGunBreak("RightFoot");
		DAFDM_ProtectLegZoneFromGunBreak("LeftFoot");

		if (GetModifiersManager() && GetModifiersManager().IsModifierActive(eModifiers.MDF_BROKEN_LEGS))
			GetModifiersManager().DeactivateModifier(eModifiers.MDF_BROKEN_LEGS);

		SetBrokenLegs(eBrokenLegs.NO_BROKEN_LEGS);
	}

	override void OnUnconsciousStart()
	{
		if (!DAFDM_IsUnconsciousnessEnabled())
		{
			DAFDM_FillShock();
			return;
		}

		DAFDM_FillShock();
		m_DAFDM_BlockUnconsciousHandDrop = true;
		super.OnUnconsciousStart();
		m_DAFDM_BlockUnconsciousHandDrop = false;
	}

	override bool CanDropEntity(notnull EntityAI item)
	{
		if (m_DAFDM_BlockUnconsciousHandDrop && item == GetEntityInHands())
			return false;

		return super.CanDropEntity(item);
	}

	override bool EEOnDamageCalculated(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		DAFDeathmatch deathmatch = GetDAFDeathmatch();
		if (deathmatch && deathmatch.ShouldBlockWarmupInfectedDamage(source))
			return false;

		return super.EEOnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
	}

	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		bool hadBrokenLegRisk = DAFDM_HasBrokenLegRisk();

		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

		DAFDeathmatch deathmatch = GetDAFDeathmatch();
		if (deathmatch && GetGame().IsServer() && IsAlive())
		{
			PlayerBase attacker;
			if (source)
				attacker = PlayerBase.Cast(source.GetHierarchyRootPlayer());

			if (attacker)
				deathmatch.OnPlayerDamaged(this, attacker);
		}

		if (GetGame().IsServer() && IsAlive() && damageType == DamageType.FIRE_ARM && !hadBrokenLegRisk && DAFDM_HasBrokenLegRisk())
			DAFDM_PreventGunshotBrokenLegs();
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
