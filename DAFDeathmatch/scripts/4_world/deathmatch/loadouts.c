class DAFDMLoadoutItemConfig
{
	string type;
	int quickbarSlot = -1;
}

class DAFDMLoadoutWeaponConfig
{
	string type;
	int quickbarSlot = -1;
	ref TStringArray fallbackTypes = new TStringArray();
	ref TStringArray attachments = new TStringArray();
	ref TStringArray loadedMagazines = new TStringArray();
	ref TStringArray spareMagazines = new TStringArray();
}

class DAFDMWeaponPoolConfig
{
	string name;
	int weight = 1;
	ref TStringArray variants = new TStringArray();
	ref array<ref TStringArray> attachments = new array<ref TStringArray>();
	ref TStringArray accessories = new TStringArray();
}

class DAFDMLoadoutEntryConfig
{
	string name;
	int weight = 1;
	ref DAFDMLoadoutWeaponConfig primary;
	ref DAFDMLoadoutWeaponConfig secondary;
	ref array<ref TStringArray> outfit = new array<ref TStringArray>();
	ref array<ref DAFDMWeaponPoolConfig> primaryWeapons = new array<ref DAFDMWeaponPoolConfig>();
	ref array<ref DAFDMWeaponPoolConfig> secondaryWeapons = new array<ref DAFDMWeaponPoolConfig>();
	ref array<ref DAFDMLoadoutItemConfig> items = new array<ref DAFDMLoadoutItemConfig>();
}

class DAFDMLoadoutPoolConfig
{
	string name;
	ref array<ref DAFDMLoadoutEntryConfig> entries = new array<ref DAFDMLoadoutEntryConfig>();
}

class DAFDMLoadoutRegistry
{
	private ref array<ref DAFDMLoadoutPoolConfig> m_Pools = new array<ref DAFDMLoadoutPoolConfig>();

	void Load()
	{
		if (!FileExist(DAFDMFilenames.DIRECTORY))
			MakeDirectory(DAFDMFilenames.DIRECTORY);

		if (FileExist(DAFDMFilenames.LOADOUTS_JSON))
		{
			JsonFileLoader<ref array<ref DAFDMLoadoutPoolConfig>>.JsonLoadFile(DAFDMFilenames.LOADOUTS_JSON, m_Pools);
		}
		else
		{
			FillDefaults();
			JsonFileLoader<ref array<ref DAFDMLoadoutPoolConfig>>.JsonSaveFile(DAFDMFilenames.LOADOUTS_JSON, m_Pools);
		}

		if (m_Pools.Count() == 0)
			FillDefaults();

		PrintFormat("DAFDeathmatch: loaded %1 loadout pools", m_Pools.Count());
	}

	DAFDMLoadoutEntryConfig PickEntry(string poolName, string lastEntryName)
	{
		DAFDMLoadoutPoolConfig pool = GetPool(poolName);
		if (!pool || pool.entries.Count() == 0)
			pool = GetPool("normal");

		if (!pool || pool.entries.Count() == 0)
			return null;

		DAFDMLoadoutEntryConfig picked = PickWeighted(pool);
		if (picked && picked.name == lastEntryName && pool.entries.Count() > 1)
			picked = PickWeighted(pool);

		ResolveWeaponPools(picked);
		return picked;
	}

	DAFDMLoadoutPoolConfig GetPool(string name)
	{
		foreach (DAFDMLoadoutPoolConfig pool: m_Pools)
		{
			if (pool && pool.name == name)
				return pool;
		}

		return null;
	}

	int Count()
	{
		return m_Pools.Count();
	}

	array<ref DAFDMLoadoutPoolConfig> GetAll()
	{
		return m_Pools;
	}

	private DAFDMLoadoutEntryConfig PickWeighted(DAFDMLoadoutPoolConfig pool)
	{
		int total = 0;
		foreach (DAFDMLoadoutEntryConfig entry: pool.entries)
		{
			if (entry)
				total += Math.Max(entry.weight, 1);
		}

		int roll = Math.RandomInt(0, total);
		int cursor = 0;
		foreach (DAFDMLoadoutEntryConfig candidate: pool.entries)
		{
			if (!candidate)
				continue;

			cursor += Math.Max(candidate.weight, 1);
			if (roll < cursor)
				return candidate;
		}

		return pool.entries[0];
	}

	private void FillDefaults()
	{
		m_Pools.Clear();
		m_Pools.Insert(MakePool("normal", "M4A1", "M4_RISHndgrd", "M4_MPBttstck", "ACOGOptic", "Mag_STANAG_30Rnd", "Glock19", "Mag_Glock_15Rnd"));
		m_Pools.Insert(MakePool("snipers", "Winchester70", "", "", "HuntingOptic", "Ammo_308Win", "Glock19", "Mag_Glock_15Rnd"));
		m_Pools.Insert(MakePool("freshies", "Mosin9130", "", "", "", "Ammo_762x54", "MKII", "Mag_MKII_10Rnd"));
		m_Pools.Insert(MakePool("juiced", "AKM", "AK_PlasticHndgrd", "AK_PlasticBttstck", "PSO1Optic", "Mag_AKM_30Rnd", "Deagle", "Mag_Deagle_9rnd"));
	}

	private void ResolveWeaponPools(DAFDMLoadoutEntryConfig entry)
	{
		if (!entry)
			return;

		if (entry.primaryWeapons && entry.primaryWeapons.Count() > 0)
			entry.primary = ResolveWeapon(PickWeightedWeapon(entry.primaryWeapons), 0);

		if (entry.secondaryWeapons && entry.secondaryWeapons.Count() > 0)
			entry.secondary = ResolveWeapon(PickWeightedWeapon(entry.secondaryWeapons), 1);
	}

	private DAFDMWeaponPoolConfig PickWeightedWeapon(array<ref DAFDMWeaponPoolConfig> weapons)
	{
		int total = 0;
		foreach (DAFDMWeaponPoolConfig weapon: weapons)
		{
			if (weapon && weapon.variants && weapon.variants.Count() > 0)
				total += Math.Max(weapon.weight, 1);
		}

		if (total <= 0)
			return null;

		int roll = Math.RandomInt(0, total);
		int cursor = 0;
		foreach (DAFDMWeaponPoolConfig candidate: weapons)
		{
			if (!candidate || !candidate.variants || candidate.variants.Count() == 0)
				continue;

			cursor += Math.Max(candidate.weight, 1);
			if (roll < cursor)
				return candidate;
		}

		return null;
	}

	private DAFDMLoadoutWeaponConfig ResolveWeapon(DAFDMWeaponPoolConfig poolWeapon, int quickbarSlot)
	{
		if (!poolWeapon || !poolWeapon.variants || poolWeapon.variants.Count() == 0)
			return null;

		DAFDMLoadoutWeaponConfig weapon = new DAFDMLoadoutWeaponConfig();
		weapon.type = poolWeapon.variants.GetRandomElement();
		weapon.quickbarSlot = quickbarSlot;
		foreach (string variant: poolWeapon.variants)
		{
			if (variant != weapon.type)
				weapon.fallbackTypes.Insert(variant);
		}

		foreach (TStringArray attachmentChoices: poolWeapon.attachments)
		{
			if (attachmentChoices && attachmentChoices.Count() > 0)
				weapon.attachments.Insert(attachmentChoices.GetRandomElement());
		}

		bool loaded = false;
		foreach (string accessory: poolWeapon.accessories)
		{
			if (accessory == "")
				continue;

			if (!loaded)
			{
				weapon.loadedMagazines.Insert(accessory);
				loaded = true;
			}
			else
			{
				weapon.spareMagazines.Insert(accessory);
			}
		}

		return weapon;
	}

	private DAFDMLoadoutPoolConfig MakePool(string poolName, string primaryType, string attachment1, string attachment2, string optic, string primaryMag, string secondaryType, string secondaryMag)
	{
		DAFDMLoadoutPoolConfig pool = new DAFDMLoadoutPoolConfig();
		pool.name = poolName;

		DAFDMLoadoutEntryConfig entry = new DAFDMLoadoutEntryConfig();
		entry.name = poolName + "-default";
		entry.weight = 1;
		entry.primary = MakeWeapon(primaryType, 0, attachment1, attachment2, optic, primaryMag);
		entry.secondary = MakeWeapon(secondaryType, 1, "", "", "", secondaryMag);
		entry.outfit.Insert(MakeChoices("BDUJacket"));
		entry.outfit.Insert(MakeChoices("BDUPants"));
		entry.outfit.Insert(MakeChoices("AthleticShoes_Black"));
		entry.outfit.Insert(MakeChoices("PressVest_Blue"));
		entry.items.Insert(MakeItem("BandageDressing", 2));
		entry.items.Insert(MakeItem("Morphine", 3));
		entry.items.Insert(MakeItem("SalineBagIV", 4));
		pool.entries.Insert(entry);
		return pool;
	}

	private DAFDMLoadoutWeaponConfig MakeWeapon(string type, int quickbarSlot, string attachment1, string attachment2, string attachment3, string magazine)
	{
		DAFDMLoadoutWeaponConfig weapon = new DAFDMLoadoutWeaponConfig();
		weapon.type = type;
		weapon.quickbarSlot = quickbarSlot;

		if (attachment1 != "")
			weapon.attachments.Insert(attachment1);
		if (attachment2 != "")
			weapon.attachments.Insert(attachment2);
		if (attachment3 != "")
			weapon.attachments.Insert(attachment3);

		if (magazine != "")
		{
			weapon.loadedMagazines.Insert(magazine);
			weapon.spareMagazines.Insert(magazine);
		}

		return weapon;
	}

	private DAFDMLoadoutItemConfig MakeItem(string type, int quickbarSlot)
	{
		DAFDMLoadoutItemConfig item = new DAFDMLoadoutItemConfig();
		item.type = type;
		item.quickbarSlot = quickbarSlot;
		return item;
	}

	private TStringArray MakeChoices(string type)
	{
		TStringArray choices = new TStringArray();
		choices.Insert(type);
		return choices;
	}
}
