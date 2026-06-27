class DAFDMLoadoutItemConfig
{
	string type;
	int quickbarSlot = -1;
}

class DAFDMLoadoutWeaponConfig
{
	string type;
	int quickbarSlot = -1;
	ref TStringArray attachments = new TStringArray();
	ref TStringArray loadedMagazines = new TStringArray();
	ref TStringArray spareMagazines = new TStringArray();
}

class DAFDMLoadoutEntryConfig
{
	string name;
	int weight = 1;
	ref DAFDMLoadoutWeaponConfig primary;
	ref DAFDMLoadoutWeaponConfig secondary;
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

		return picked;
	}

	private DAFDMLoadoutPoolConfig GetPool(string name)
	{
		foreach (DAFDMLoadoutPoolConfig pool: m_Pools)
		{
			if (pool && pool.name == name)
				return pool;
		}

		return null;
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
		m_Pools.Insert(MakePool("juicer", "AKM", "AK_PlasticHndgrd", "AK_PlasticBttstck", "PSO1Optic", "Mag_AKM_30Rnd", "Deagle", "Mag_Deagle_9rnd"));
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
}
