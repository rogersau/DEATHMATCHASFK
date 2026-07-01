class DAF_ItemDamageSnapshot
{
	EntityAI m_Item;
	float m_GlobalHealth;
	ref array<string> m_Zones;
	ref array<float> m_ZoneHealth;

	void DAF_ItemDamageSnapshot(EntityAI item)
	{
		m_Item = item;
		m_GlobalHealth = item.GetHealth("", "Health");
		m_Zones = new array<string>();
		m_ZoneHealth = new array<float>();

		item.GetDamageZones(m_Zones);
		foreach (string zone: m_Zones)
		{
			m_ZoneHealth.Insert(item.GetHealth(zone, "Health"));
		}
	}

	void Restore()
	{
		if (!m_Item)
			return;

		m_Item.SetHealth("", "Health", m_GlobalHealth);

		for (int i = 0; i < m_Zones.Count(); i++)
		{
			m_Item.SetHealth(m_Zones[i], "Health", m_ZoneHealth[i]);
		}
	}
}

modded class PlayerBase extends ManBase
{
	protected const int DAF_FULL_AUTO_SWEEP_DELAY_MS = 500;
	protected const bool DAF_ENABLE_WEAPON_FIRE_MODE_CHANGES = false;

	private static ref KillFeedWrapper s_KillFeed;
	private static ref KillFeedHandle s_KillHandle;
	private static int s_DAF_PendingRoundSeconds = -1;
	private static string s_DAF_PendingRoundLabel = "";
	private static int s_DAF_PendingPlayerCount = -1;
	private static string s_DAF_PendingPlayerCountStatus = "";
	private static int s_DAF_PendingRoundKills = 0;
	private static int s_DAF_PendingRoundDeaths = 0;
	private static int s_DAF_PendingSeasonPoints = 0;
	private static int s_DAF_PendingSeasonRank = 0;
	private static bool s_DAFDM_ManualRespawnAllowed = false;
	protected ref array<ref DAF_ItemDamageSnapshot> m_DAF_ItemDamageSnapshots;
	private bool m_DAF_IsFallDeath;
	private bool m_DAF_IsHeadshot;

	void PlayerBase()
	{
		if (GetGame().IsClient())
		{
			if (!s_KillFeed)
			{
				s_KillFeed = new KillFeedWrapper();
				DAF_ApplyPendingRoundHud();
			}

			if (!s_KillHandle)
				s_KillHandle = new KillFeedHandle();
		}
		else if (GetGame().IsServer())
		{
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.SendKillFeedPlayerCount, 2000, false);
		}
	}

	void SendKillFeedPlayerCount()
	{
		KillFeedHandle handle = GetKillFeedHandle();
		if (handle)
			handle.SendPlayerCount();
	}

	protected bool DAF_IsFeetAttachment(notnull EntityAI item)
	{
		InventoryLocation location = new InventoryLocation();
		if (!item.GetInventory().GetCurrentInventoryLocation(location))
			return false;

		if (location.GetType() != InventoryLocationType.ATTACHMENT)
			return false;

		if (location.GetParent() != this)
			return false;

		return location.GetSlot() == InventorySlots.GetSlotIdFromString("Feet");
	}

	protected bool DAF_IsSalineBagIV(EntityAI item)
	{
		return item && item.IsKindOf("SalineBagIV");
	}

	protected bool DAF_ShouldProtectInventoryDamage()
	{
		return GetGame() && GetGame().IsServer();
	}

	protected bool DAF_IsInfectedDamageSource(EntityAI source)
	{
		return source && source.IsInherited(DayZInfected);
	}

	bool DAF_WasLastHitHeadshot()
	{
		return m_DAF_IsHeadshot;
	}

	protected void DAF_SnapshotCarriedItemDamage()
	{
		if (!DAF_ShouldProtectInventoryDamage())
			return;

		m_DAF_ItemDamageSnapshots = new array<ref DAF_ItemDamageSnapshot>();

		array<EntityAI> items = new array<EntityAI>();
		GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);

		foreach (EntityAI item: items)
		{
			if (item)
				m_DAF_ItemDamageSnapshots.Insert(new DAF_ItemDamageSnapshot(item));
		}

		EntityAI inHands = GetHumanInventory().GetEntityInHands();
		if (inHands)
			m_DAF_ItemDamageSnapshots.Insert(new DAF_ItemDamageSnapshot(inHands));
	}

	protected void DAF_RestoreCarriedItemDamage()
	{
		if (!m_DAF_ItemDamageSnapshots)
			return;

		foreach (DAF_ItemDamageSnapshot snapshot: m_DAF_ItemDamageSnapshots)
		{
			if (snapshot)
				snapshot.Restore();
		}

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.DAF_ClearCarriedItemDamageSnapshots, 1500, false);
	}

	void DAF_ClearCarriedItemDamageSnapshots()
	{
		if (m_DAF_ItemDamageSnapshots)
			m_DAF_ItemDamageSnapshots.Clear();
	}

	void DAF_FullHealFromSaline()
	{
		SetHealth("", "Health", GetMaxHealth("", "Health"));
		SetHealth("", "Blood", GetMaxHealth("", "Blood"));
		SetHealth("", "Shock", GetMaxHealth("", "Shock"));

		SetHealth("RightLeg", "Health", GetMaxHealth("RightLeg", "Health"));
		SetHealth("RightFoot", "Health", GetMaxHealth("RightFoot", "Health"));
		SetHealth("LeftLeg", "Health", GetMaxHealth("LeftLeg", "Health"));
		SetHealth("LeftFoot", "Health", GetMaxHealth("LeftFoot", "Health"));

		if (GetBleedingManagerServer())
			GetBleedingManagerServer().RemoveAllSources();

		if (GetModifiersManager() && GetModifiersManager().IsModifierActive(eModifiers.MDF_BROKEN_LEGS))
			GetModifiersManager().DeactivateModifier(eModifiers.MDF_BROKEN_LEGS);

		SetBrokenLegs(eBrokenLegs.NO_BROKEN_LEGS);
	}

	void DAF_NormalizeSurvivalStats()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;

		GetStatEnergy().Set(GetStatEnergy().GetMax());
		GetStatWater().Set(GetStatWater().GetMax());
		GetStatHeatComfort().Set(0.0);

		if (m_PlayerStomach)
			m_PlayerStomach.ClearContents();

		RemoveAllAgents();

		ModifiersManager mods = GetModifiersManager();
		if (!mods)
		{
			SetSynchDirty();
			return;
		}

		mods.DeactivateModifier(eModifiers.MDF_BURNING);
		mods.DeactivateModifier(eModifiers.MDF_FEVER);
		mods.DeactivateModifier(eModifiers.MDF_COMMON_COLD);
		mods.DeactivateModifier(eModifiers.MDF_POISONING);
		mods.DeactivateModifier(eModifiers.MDF_INFLUENZA);
		mods.DeactivateModifier(eModifiers.MDF_CHOLERA);
		mods.DeactivateModifier(eModifiers.MDF_SALMONELLA);
		mods.DeactivateModifier(eModifiers.MDF_VOMITSTUFFED);
		mods.DeactivateModifier(eModifiers.MDF_WETNESS);
		mods.DeactivateModifier(eModifiers.MDF_TOXICITY);
		mods.DeactivateModifier(eModifiers.MDF_BREATH_VAPOUR);
		mods.DeactivateModifier(eModifiers.MDF_WOUND_INFECTION);
		mods.DeactivateModifier(eModifiers.MDF_WOUND_INFECTION1);
		mods.DeactivateModifier(eModifiers.MDF_WOUND_INFECTION2);
		mods.DeactivateModifier(eModifiers.MDF_CONTAMINATION1);
		mods.DeactivateModifier(eModifiers.MDF_CONTAMINATION2);
		mods.DeactivateModifier(eModifiers.MDF_CONTAMINATION3);
		mods.DeactivateModifier(eModifiers.MDF_HEAVYMETAL1);
		mods.DeactivateModifier(eModifiers.MDF_HEAVYMETAL2);
		mods.DeactivateModifier(eModifiers.MDF_HEAVYMETAL3);
		mods.DeactivateModifier(eModifiers.MDF_PNEUMONIA);
		mods.DeactivateModifier(eModifiers.MDF_DROWNING);
		mods.DeactivateModifier(eModifiers.MDF_HEART_ATTACK);
		mods.DeactivateModifier(eModifiers.MDF_UNCONSCIOUSNESS);
		mods.DeactivateModifier(eModifiers.MDF_BLINDED);
		mods.DeactivateModifier(eModifiers.MDF_AREAEXPOSURE);
		mods.DeactivateModifier(eModifiers.MDF_FLIES);

		SetSynchDirty();
	}

	protected void DAF_DeleteSalineBagIV(EntityAI item)
	{
		if (DAF_IsSalineBagIV(item))
			item.Delete();
	}

	protected void DAF_DeleteSalineOnDeath()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;

		array<EntityAI> items = new array<EntityAI>();
		GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);

		foreach (EntityAI item: items)
		{
			DAF_DeleteSalineBagIV(item);
		}

		DAF_DeleteSalineBagIV(GetHumanInventory().GetEntityInHands());
	}

	protected bool DAF_SetWeaponPreferredFireMode(notnull Weapon_Base weapon)
	{
		bool changed = false;

		for (int muzzle = 0; muzzle < weapon.GetMuzzleCount(); muzzle++)
		{
			int originalMode = weapon.GetCurrentMode(muzzle);
			int modeCount = weapon.GetMuzzleModeCount(muzzle);
			int preferredMode = -1;
			int preferredBurstSize = 1;

			for (int mode = 0; mode < modeCount; mode++)
			{
				weapon.SetCurrentMode(muzzle, mode);

				if (weapon.GetCurrentModeAutoFire(muzzle))
				{
					preferredMode = mode;
					break;
				}

				int burstSize = weapon.GetCurrentModeBurstSize(muzzle);
				if (burstSize > preferredBurstSize)
				{
					preferredMode = mode;
					preferredBurstSize = burstSize;
				}
			}

			if (preferredMode != -1)
			{
				if (originalMode != preferredMode)
				{
					weapon.SetCurrentMode(muzzle, preferredMode);
					changed = true;
				}
			}
			else
			{
				weapon.SetCurrentMode(muzzle, originalMode);
			}
		}

		if (changed)
			weapon.Synchronize();

		return changed;
	}

	protected void DAF_SetEntityFullAuto(EntityAI item)
	{
		if (!DAF_ENABLE_WEAPON_FIRE_MODE_CHANGES)
			return;

		Weapon_Base weapon = Weapon_Base.Cast(item);
		if (weapon)
			DAF_SetWeaponPreferredFireMode(weapon);
	}

	protected void DAF_SetLoadoutWeaponsFullAuto()
	{
		array<EntityAI> items = new array<EntityAI>();
		GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);

		foreach (EntityAI item: items)
		{
			DAF_SetEntityFullAuto(item);
		}

		DAF_SetEntityFullAuto(GetHumanInventory().GetEntityInHands());
	}

	protected void DAF_QueueFullAutoSweep()
	{
		if (!DAF_ENABLE_WEAPON_FIRE_MODE_CHANGES)
			return;

		if (!GetGame() || GetGame().IsClient())
			return;

		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(DAF_SetLoadoutWeaponsFullAuto, DAF_FULL_AUTO_SWEEP_DELAY_MS, false);
	}

	override void Init()
	{
		super.Init();
	}

	override void OnPlayerLoaded()
	{
		super.OnPlayerLoaded();
		if (GetGame().IsClient())
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(DAF_RecreateKillFeedHud, 250, false);

		DAF_QueueFullAutoSweep();
	}

	override void OnConnect()
	{
		super.OnConnect();
		if (GetGame().IsClient())
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(DAF_RecreateKillFeedHud, 250, false);

		DAF_QueueFullAutoSweep();
	}

	override void EEItemAttached(EntityAI item, string slot_name)
	{
		super.EEItemAttached(item, slot_name);
		DAF_SetEntityFullAuto(item);
	}

	override void EEItemIntoHands(EntityAI item)
	{
		super.EEItemIntoHands(item);
		DAF_SetEntityFullAuto(item);
	}

	override bool EEOnDamageCalculated(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		if (DAF_IsInfectedDamageSource(source))
			return false;

		DAF_SnapshotCarriedItemDamage();
		return super.EEOnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
	}

	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		if (ammo == "FallDamage" && !IsAlive())
			m_DAF_IsFallDeath = true;

		m_DAF_IsHeadshot = dmgZone == "Head";

		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
		DAF_RestoreCarriedItemDamage();
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.DAF_RestoreCarriedItemDamage, 100, false);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.DAF_RestoreCarriedItemDamage, 500, false);
	}

	override void EEKilled(Object killer)
	{
		DAF_DeleteSalineOnDeath();

		EntityAI weapon;
		EntityAI killerEntity = EntityAI.Cast(killer);
		PlayerBase target;
		if (killerEntity)
			target = PlayerBase.Cast(killerEntity.GetHierarchyRootPlayer());

		DeathType deathType = DeathType.UNKNOWN;
		bool wasHeadshot = m_DAF_IsHeadshot;
		bool wasFallDeath = m_DAF_IsFallDeath;

		m_DAF_IsHeadshot = false;
		m_DAF_IsFallDeath = false;

		if (killer && killer == this)
		{
			if (GetHealth("", "Blood") < PlayerConstants.BLOOD_THRESHOLD_FATAL)
				deathType = DeathType.BLEEDING;
			else if (wasFallDeath)
				deathType = DeathType.FALL;
			else if (GetStatWater().Get() <= PlayerConstants.LOW_WATER_THRESHOLD || GetStatEnergy().Get() <= PlayerConstants.LOW_ENERGY_THRESHOLD)
				deathType = DeathType.STARVING;
			else
				deathType = DeathType.SUICIDE;
		}
		else if (killer && killer.IsInherited(ZombieBase))
			deathType = DeathType.ZOMBIE;
		else if (killer && killer.IsInherited(AnimalBase))
			deathType = DeathType.ANIMAL;
		else if (target && target != this)
		{
			if (killer != target)
				weapon = killerEntity;
			deathType = DeathType.PVP;
		}

		KillFeedHandle handle = GetKillFeedHandle();
		if (handle)
			handle.OnPlayerKilled(deathType, this, target, weapon, wasHeadshot);

		super.EEKilled(killer);
	}

	override bool CanDropEntity(notnull EntityAI item)
	{
		if (DAF_IsFeetAttachment(item) || DAF_IsSalineBagIV(item))
			return false;

		return super.CanDropEntity(item);
	}

	override bool CanReleaseAttachment(EntityAI attachment)
	{
		if (attachment && DAF_IsFeetAttachment(attachment))
			return false;

		return super.CanReleaseAttachment(attachment);
	}

	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		super.OnRPC(sender, rpc_type, ctx);

		if (GetGame().IsClient())
			DAF_HandleClientRPC(rpc_type, ctx);
	}

	void DAF_HandleClientRPC(int rpc_type, ParamsReadContext ctx)
	{
		if (rpc_type == DAFRPC.RPC_KILLFEED_ITEM)
		{
			Param7<string, string, string, int, string, int, int> data;
			if (!ctx.Read(data))
				return;

			if (s_KillFeed)
				s_KillFeed.AddItem(data);
		}
		else if (rpc_type == DAFRPC.RPC_PLAYER_COUNT)
		{
			Param1<int> playerCount;
			if (!ctx.Read(playerCount))
				return;

			if (s_KillFeed)
				s_KillFeed.SetPlayerCount(playerCount.param1);
		}
		else if (rpc_type == DAFRPC.RPC_PLAYER_COUNT_STATUS)
		{
			Param2<int, string> playerCountStatus;
			if (!ctx.Read(playerCountStatus))
				return;

			if (s_KillFeed)
				s_KillFeed.SetPlayerCountStatus(playerCountStatus.param1, playerCountStatus.param2);
		}
		else if (rpc_type == DAFRPC.RPC_RESPAWN_CURSOR_FIX)
		{
			Param1<bool> respawnData;
			if (!ctx.Read(respawnData))
				return;

			GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.DAF_FixRespawnCursor, 100, false);
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.DAF_FixRespawnCursor, 750, false);
			GetGame().GetCallQueue(CALL_CATEGORY_GUI).CallLater(this.DAF_FixRespawnCursor, 1500, false);
		}
		else if (rpc_type == DAFRPC.RPC_KILLFEED_RESET)
		{
			Param1<bool> resetData;
			if (!ctx.Read(resetData))
				return;

			if (s_KillFeed)
			{
				s_KillFeed.ClearItems();
				s_KillFeed.SetRoundStats(0, 0, 0, 0);
			}

			s_DAF_PendingRoundKills = 0;
			s_DAF_PendingRoundDeaths = 0;
			s_DAF_PendingSeasonPoints = 0;
			s_DAF_PendingSeasonRank = 0;
		}
		else if (rpc_type == DAFRPC.RPC_ROUND_TIME_SECONDS)
		{
			Param1<int> roundTimeData;
			if (!ctx.Read(roundTimeData))
				return;

			s_DAF_PendingRoundSeconds = roundTimeData.param1;
			if (s_KillFeed)
				s_KillFeed.SetRoundTimeRemaining(roundTimeData.param1);
		}
		else if (rpc_type == DAFRPC.RPC_ROUND_LABEL)
		{
			Param1<string> roundLabelData;
			if (!ctx.Read(roundLabelData))
				return;

			s_DAF_PendingRoundLabel = roundLabelData.param1;
			if (s_KillFeed)
				s_KillFeed.SetRoundLabel(roundLabelData.param1);
		}
		else if (rpc_type == DAFRPC.RPC_ROUND_HUD_STATE)
		{
			Param5<int, string, int, string, bool> stateData;
			if (!ctx.Read(stateData))
				return;

			s_DAF_PendingRoundSeconds = stateData.param1;
			s_DAF_PendingRoundLabel = stateData.param2;
			s_DAF_PendingPlayerCount = stateData.param3;
			s_DAF_PendingPlayerCountStatus = stateData.param4;
			s_DAFDM_ManualRespawnAllowed = stateData.param5;

			DAF_ApplyPendingRoundHud();
		}
		else if (rpc_type == DAFRPC.RPC_ROUND_STATS)
		{
			Param4<int, int, int, int> statsData;
			if (!ctx.Read(statsData))
				return;

			s_DAF_PendingRoundKills = statsData.param1;
			s_DAF_PendingRoundDeaths = statsData.param2;
			if (statsData.param3 >= 0)
				s_DAF_PendingSeasonPoints = statsData.param3;
			if (statsData.param4 >= 0)
				s_DAF_PendingSeasonRank = statsData.param4;

			if (s_KillFeed)
				s_KillFeed.SetRoundStats(s_DAF_PendingRoundKills, s_DAF_PendingRoundDeaths, s_DAF_PendingSeasonPoints, s_DAF_PendingSeasonRank);
		}
		else if (rpc_type == DAFRPC.RPC_SEASON_POINTS_POPUP)
		{
			Param1<string> popupData;
			if (!ctx.Read(popupData))
				return;

			DAFSeasonPointsPopup.Show(popupData.param1);
		}
		else if (rpc_type == DAFRPC.RPC_LEADERBOARD_SNAPSHOT)
		{
			Param1<TStringArray> leaderboardData;
			if (!ctx.Read(leaderboardData))
				return;

			DAFDMLeaderboardCache.SetSnapshot(leaderboardData.param1);
		}
	}

	static void DAF_ApplyPendingRoundHud()
	{
		if (!s_KillFeed)
			return;

		s_KillFeed.SetRoundLabel(s_DAF_PendingRoundLabel);
		s_KillFeed.SetRoundTimeRemaining(s_DAF_PendingRoundSeconds);

		if (s_DAF_PendingPlayerCount >= 0)
			s_KillFeed.SetPlayerCountStatus(s_DAF_PendingPlayerCount, s_DAF_PendingPlayerCountStatus);

		s_KillFeed.SetRoundStats(s_DAF_PendingRoundKills, s_DAF_PendingRoundDeaths, s_DAF_PendingSeasonPoints, s_DAF_PendingSeasonRank);
	}

	bool DAFDM_IsManualRespawnAllowed()
	{
		return s_DAFDM_ManualRespawnAllowed;
	}

	static void DAF_RecreateKillFeedHud()
	{
		if (s_KillFeed)
			s_KillFeed.Destroy();

		s_KillFeed = new KillFeedWrapper();
		DAF_ApplyPendingRoundHud();
	}

	void DAF_FixRespawnCursor()
	{
		GetGame().GetUIManager().ShowCursor(false);
		GetGame().GetInput().ChangeGameFocus(-1);
	}
}
