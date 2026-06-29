/**
 * DAFDMLoadoutEngine owns player equipment mutation for a spawn.
 *
 * The manager chooses the current round type, pool, and game mode; this module
 * clears inventory, rolls the loadout, creates outfit/weapons/items, enforces
 * TDM visual identity, repairs attachments, and normalizes quickbar slots.
 */
class DAFDMLoadoutEngine
{
	private ref DAFDMSettings m_Settings;
	private ref DAFDMLoadoutRegistry m_Loadouts;
	private ref DAFDMTeams m_Teams;
	private ref map<string, string> m_LastLoadoutByPlayer = new map<string, string>();
	private ref map<string, string> m_LastPrimaryTypeByPlayer = new map<string, string>();
	private ref map<string, string> m_LastSecondaryTypeByPlayer = new map<string, string>();

	void DAFDMLoadoutEngine(DAFDMSettings settings, DAFDMLoadoutRegistry loadouts, DAFDMTeams teams)
	{
		m_Settings = settings;
		m_Loadouts = loadouts;
		m_Teams = teams;
	}

	void EquipPlayer(PlayerBase player, string poolName, string gameMode)
	{
		if (!player)
			return;

		ClearPlayerInventory(player);
		HumanInventory inventory = player.GetHumanInventory();
		DAFDMLoadoutEntryConfig loadout = PickLoadoutForPlayer(player, poolName);
		if (!loadout)
		{
			PrintFormat("DAFDeathmatch: no loadout found for pool %1", poolName);
			return;
		}

		CreateOutfit(inventory, loadout);
		ApplyTeamOutfit(player, gameMode);
		RepairPlayerAttachments(player);
		EntityAI primary = CreateLoadoutWeapon(player, inventory, loadout.primary, true);
		if (!primary)
			primary = CreateEmergencyPrimary(player, inventory);

		EntityAI secondary = CreateLoadoutWeapon(player, inventory, loadout.secondary, false);

		string playerId = "";
		if (player.GetIdentity())
			playerId = player.GetIdentity().GetId();

		m_LastPrimaryTypeByPlayer.Set(playerId, "");
		m_LastSecondaryTypeByPlayer.Set(playerId, "");
		if (primary)
			m_LastPrimaryTypeByPlayer.Set(playerId, primary.GetType());
		if (secondary)
			m_LastSecondaryTypeByPlayer.Set(playerId, secondary.GetType());

		foreach (DAFDMLoadoutItemConfig itemConfig: loadout.items)
		{
			if (!itemConfig || itemConfig.type == "")
				continue;

			EntityAI item = inventory.CreateInInventory(itemConfig.type);
			if (item && itemConfig.quickbarSlot >= 0)
				player.SetQuickBarEntityShortcut(item, itemConfig.quickbarSlot, true);
		}

		NormalizeLoadoutHotbar(player, primary, secondary, true);
		ScheduleLoadoutHotbarResync(player);
	}

	string GetLastLoadoutName(string playerId)
	{
		string loadoutName = "none";
		if (playerId != "")
			m_LastLoadoutByPlayer.Find(playerId, loadoutName);

		return loadoutName;
	}

	private void NormalizeLoadoutHotbar(PlayerBase player, EntityAI primary, EntityAI secondary, bool createMissingSurvivalItems)
	{
		if (!player)
			return;

		if (primary)
			player.SetQuickBarEntityShortcut(primary, 0, true);

		if (secondary)
			player.SetQuickBarEntityShortcut(secondary, 1, true);

		EntityAI knife = null;
		EntityAI morphine = null;
		EntityAI bandage = null;
		EntityAI saline = null;

		if (createMissingSurvivalItems)
		{
			knife = EnsureKnife(player);
			morphine = EnsureLoadoutItem(player, "Morphine", "morphine");
			bandage = EnsureLoadoutItem(player, "BandageDressing", "bandage");
			saline = EnsureLoadoutItem(player, "SalineBagIV", "saline");
		}
		else
		{
			knife = FindKnife(player);
			morphine = FindLoadoutItem(player, "Morphine");
			bandage = FindLoadoutItem(player, "BandageDressing");
			saline = FindLoadoutItem(player, "SalineBagIV");
		}

		if (knife)
			player.SetQuickBarEntityShortcut(knife, 2, true);

		if (morphine)
			player.SetQuickBarEntityShortcut(morphine, 3, true);

		if (bandage)
			player.SetQuickBarEntityShortcut(bandage, 4, true);

		if (saline)
			player.SetQuickBarEntityShortcut(saline, 5, true);
	}

	private void ScheduleLoadoutHotbarResync(PlayerBase player)
	{
		if (!player)
			return;

		PlayerIdentity identity = player.GetIdentity();
		if (!identity)
			return;

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.ResyncLoadoutHotbar, 500, false, identity);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.ResyncLoadoutHotbar, 1500, false, identity);
	}

	void ResyncLoadoutHotbar(PlayerIdentity identity)
	{
		if (!identity)
			return;

		PlayerBase player = PlayerBase.Cast(identity.GetPlayer());
		if (!player)
			return;

		string playerId = identity.GetId();
		string primaryType = "";
		string secondaryType = "";
		m_LastPrimaryTypeByPlayer.Find(playerId, primaryType);
		m_LastSecondaryTypeByPlayer.Find(playerId, secondaryType);

		EntityAI primary = null;
		if (primaryType != "")
			primary = FindInventoryItemByType(player, primaryType);

		if (!primary)
			primary = player.GetHumanInventory().GetEntityInHands();

		EntityAI secondary = null;
		if (secondaryType != "")
			secondary = FindInventoryItemByType(player, secondaryType);

		NormalizeLoadoutHotbar(player, primary, secondary, false);
	}

	private EntityAI EnsureKnife(PlayerBase player)
	{
		EntityAI existing = FindKnife(player);
		if (existing)
			return existing;

		TStringArray knifeTypes = new TStringArray();
		knifeTypes.Insert("CombatKnife");
		knifeTypes.Insert("HuntingKnife");
		knifeTypes.Insert("KitchenKnife");
		knifeTypes.Insert("SteakKnife");
		knifeTypes.Insert("StoneKnife");
		return CreateLoadoutItemAny(player, knifeTypes, "knife");
	}

	private EntityAI FindKnife(PlayerBase player)
	{
		TStringArray knifeTypes = new TStringArray();
		knifeTypes.Insert("CombatKnife");
		knifeTypes.Insert("HuntingKnife");
		knifeTypes.Insert("KitchenKnife");
		knifeTypes.Insert("SteakKnife");
		knifeTypes.Insert("StoneKnife");
		return FindInventoryItemAny(player, knifeTypes);
	}

	private EntityAI EnsureLoadoutItem(PlayerBase player, string type, string label)
	{
		EntityAI existing = FindLoadoutItem(player, type);
		if (existing)
			return existing;

		TStringArray types = new TStringArray();
		types.Insert(type);
		return CreateLoadoutItemAny(player, types, label);
	}

	private EntityAI FindLoadoutItem(PlayerBase player, string type)
	{
		TStringArray types = new TStringArray();
		types.Insert(type);
		return FindInventoryItemAny(player, types);
	}

	private EntityAI CreateLoadoutItemAny(PlayerBase player, TStringArray types, string label)
	{
		HumanInventory inventory = player.GetHumanInventory();
		foreach (string type: types)
		{
			EntityAI created = inventory.CreateInInventory(type);
			if (created)
				return created;
		}

		PrintFormat("DAFDeathmatch: failed to create hotbar %1 item", label);
		return null;
	}

	private EntityAI FindInventoryItemAny(PlayerBase player, TStringArray types)
	{
		if (!player || !types)
			return null;

		array<EntityAI> items = new array<EntityAI>();
		player.GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);

		foreach (EntityAI item: items)
		{
			if (ItemMatchesAnyType(item, types))
				return item;
		}

		EntityAI inHands = player.GetHumanInventory().GetEntityInHands();
		if (ItemMatchesAnyType(inHands, types))
			return inHands;

		return null;
	}

	private EntityAI FindInventoryItemByType(PlayerBase player, string type)
	{
		if (!player || type == "")
			return null;

		TStringArray types = new TStringArray();
		types.Insert(type);
		return FindInventoryItemAny(player, types);
	}

	private bool ItemMatchesAnyType(EntityAI item, TStringArray types)
	{
		if (!item || !types)
			return false;

		foreach (string type: types)
		{
			if (type != "" && item.IsKindOf(type))
				return true;
		}

		return false;
	}

	private void ApplyTeamOutfit(PlayerBase player, string gameMode)
	{
		if (!player || gameMode != "tdm" || !m_Settings.enforceTDMTeamOutfits)
			return;

		string team = m_Teams.GetTeam(player.GetIdentity(), gameMode);
		string jacket;
		string pants;
		string shoes;
		string mask;
		string armband;
		GetTeamOutfitTypes(team, jacket, pants, shoes, mask, armband);
		if (jacket == "" || pants == "" || shoes == "" || mask == "" || armband == "")
			return;

		ReplacePlayerAttachment(player, "Body", jacket);
		ReplacePlayerAttachment(player, "Legs", pants);
		ReplacePlayerAttachment(player, "Feet", shoes);
		ReplacePlayerAttachment(player, "Mask", mask);
		ReplacePlayerAttachment(player, "Armband", armband);
	}

	private void GetTeamOutfitTypes(string team, out string jacket, out string pants, out string shoes, out string mask, out string armband)
	{
		jacket = "";
		pants = "";
		shoes = "";
		mask = "";
		armband = "";

		team.ToLower();
		if (team == "red")
		{
			jacket = m_Settings.tdmRedJacket;
			pants = m_Settings.tdmRedPants;
			shoes = m_Settings.tdmRedShoes;
			mask = m_Settings.tdmRedMask;
			armband = m_Settings.tdmRedArmband;
			return;
		}

		if (team == "blue")
		{
			jacket = m_Settings.tdmBlueJacket;
			pants = m_Settings.tdmBluePants;
			shoes = m_Settings.tdmBlueShoes;
			mask = m_Settings.tdmBlueMask;
			armband = m_Settings.tdmBlueArmband;
		}
	}

	private void ReplacePlayerAttachment(PlayerBase player, string slotName, string itemType)
	{
		if (!player || itemType == "")
			return;

		EntityAI existing = player.FindAttachmentBySlotName(slotName);
		if (existing)
			GetGame().ObjectDelete(existing);

		EntityAI created = player.GetInventory().CreateAttachment(itemType);
		if (!created)
			PrintFormat("DAFDeathmatch: failed to create TDM team outfit item slot=%1 type=%2", slotName, itemType);
		else if (slotName == "Mask")
			player.AdjustBandana(created, slotName);
	}

	private void RepairPlayerAttachments(PlayerBase player)
	{
		if (!player)
			return;

		for (int i = 0; i < player.GetInventory().AttachmentCount(); i++)
		{
			EntityAI attachment = player.GetInventory().GetAttachmentFromIndex(i);
			if (attachment)
				RepairEntityDamage(attachment);
		}
	}

	private void RepairEntityDamage(EntityAI item)
	{
		if (!item)
			return;

		item.SetHealth("", "Health", item.GetMaxHealth("", "Health"));

		array<string> zones = new array<string>();
		item.GetDamageZones(zones);
		foreach (string zone: zones)
		{
			item.SetHealth(zone, "Health", item.GetMaxHealth(zone, "Health"));
		}
	}

	private DAFDMLoadoutEntryConfig PickLoadoutForPlayer(PlayerBase player, string poolName)
	{
		string id = "";
		if (player.GetIdentity())
			id = player.GetIdentity().GetId();

		string lastEntry;
		m_LastLoadoutByPlayer.Find(id, lastEntry);
		DAFDMLoadoutEntryConfig loadout = m_Loadouts.PickEntry(poolName, lastEntry);
		if (loadout)
			m_LastLoadoutByPlayer.Set(id, loadout.name);

		return loadout;
	}

	private void CreateOutfit(HumanInventory inventory, DAFDMLoadoutEntryConfig loadout)
	{
		if (!inventory || !loadout)
			return;

		foreach (TStringArray choices: loadout.outfit)
		{
			if (!choices || choices.Count() == 0)
				continue;

			string itemType = choices.GetRandomElement();
			if (itemType != "")
				inventory.CreateInInventory(itemType);
		}
	}

	private EntityAI CreateLoadoutWeapon(PlayerBase player, HumanInventory inventory, DAFDMLoadoutWeaponConfig config, bool inHands)
	{
		if (!inventory || !config)
			return null;

		EntityAI weapon = TryCreateLoadoutWeapon(inventory, config.type, inHands);
		if (!weapon && config.fallbackTypes)
		{
			foreach (string fallbackType: config.fallbackTypes)
			{
				weapon = TryCreateLoadoutWeapon(inventory, fallbackType, inHands);
				if (weapon)
					break;
			}
		}

		if (!weapon)
		{
			PrintFormat("DAFDeathmatch: failed to create loadout weapon %1 with %2 fallbacks", config.type, config.fallbackTypes.Count());
			return null;
		}

		foreach (string attachmentType: config.attachments)
		{
			EntityAI attachment = weapon.GetInventory().CreateAttachment(attachmentType);
			if (attachment)
				attachment.GetInventory().CreateAttachment("Battery9V");
		}

		Weapon_Base weaponBase = Weapon_Base.Cast(weapon);
		foreach (string loadedMagazineType: config.loadedMagazines)
		{
			if (weaponBase)
				weaponBase.SpawnAmmo(loadedMagazineType, Weapon_Base.SAMF_DEFAULT);
		}

		DAFWeaponFireModeHelper.SetPreferredFireMode(weaponBase);

		foreach (string spareMagazineType: config.spareMagazines)
		{
			inventory.CreateInInventory(spareMagazineType);
		}

		if (config.quickbarSlot >= 0)
			player.SetQuickBarEntityShortcut(weapon, config.quickbarSlot, true);

		return weapon;
	}

	private EntityAI CreateEmergencyPrimary(PlayerBase player, HumanInventory inventory)
	{
		if (!player || !inventory)
			return null;

		TStringArray fallbackTypes = new TStringArray();
		fallbackTypes.Insert("Winchester70");
		fallbackTypes.Insert("Mosin9130");
		fallbackTypes.Insert("M4A1");

		foreach (string type: fallbackTypes)
		{
			EntityAI weapon = TryCreateLoadoutWeapon(inventory, type, true);
			if (!weapon)
				continue;

			Weapon_Base weaponBase = Weapon_Base.Cast(weapon);
			if (weaponBase)
			{
				if (type == "Mosin9130")
					weaponBase.SpawnAmmo("Ammo_762x54", Weapon_Base.SAMF_DEFAULT);
				else if (type == "M4A1")
					weaponBase.SpawnAmmo("Mag_STANAG_30Rnd", Weapon_Base.SAMF_DEFAULT);
				else
					weaponBase.SpawnAmmo("Ammo_308Win", Weapon_Base.SAMF_DEFAULT);

				DAFWeaponFireModeHelper.SetPreferredFireMode(weaponBase);
			}

			player.SetQuickBarEntityShortcut(weapon, 0, true);
			PrintFormat("DAFDeathmatch: created emergency primary weapon %1", weapon.GetType());
			return weapon;
		}

		Print("DAFDeathmatch: failed to create emergency primary weapon");
		return null;
	}

	private void ClearPlayerInventory(PlayerBase player)
	{
		if (!player)
			return;

		EntityAI inHands = player.GetHumanInventory().GetEntityInHands();
		if (inHands)
		{
			player.GetHumanInventory().DropEntity(InventoryMode.SERVER, player, inHands);
			GetGame().ObjectDelete(inHands);
		}

		array<EntityAI> items = new array<EntityAI>();
		player.GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);
		for (int i = items.Count() - 1; i >= 0; i--)
		{
			EntityAI item = items[i];
			if (item && item != inHands)
				GetGame().ObjectDelete(item);
		}
	}

	private EntityAI TryCreateLoadoutWeapon(HumanInventory inventory, string type, bool inHands)
	{
		if (!inventory || type == "")
			return null;

		if (inHands)
			return inventory.CreateInHands(type);

		return inventory.CreateInInventory(type);
	}
}
