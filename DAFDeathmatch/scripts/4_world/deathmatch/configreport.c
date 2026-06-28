class DAFDMConfigReport
{
	static const float DUPLICATE_SPAWN_DISTANCE = 1.0;
	static const float CLOSE_SPAWN_DISTANCE = 6.0;

	static void PrintReport(DAFDMSettings settings, DAFDMArenaRegistry arenas, DAFDMLoadoutRegistry loadouts, string source)
	{
		int warningCount = Validate(settings, arenas, loadouts);

		PrintFormat("DAFDeathmatch: config report (%1): roundTypes=%2 weights=%3 arenas=%4 enabledArenas=%5 excludedArenas=%6 loadoutPools=%7 warnings=%8", source, CountRoundTypes(settings), BuildRoundWeightSummary(settings), CountArenas(arenas), CountEnabledArenas(settings, arenas), CountExcludedArenas(settings), CountLoadoutPools(loadouts), warningCount);
		PrintFormat("DAFDeathmatch: config flags (%1): autoRespawn=%2 playerRespawnCommand=%3 adminTestCommands=%4 spawnSafety=%5 tdmOutfitEnforcement=%6 lowPopWarmup=%7 weaponJamDisabled=1", source, settings.autoRespawn, settings.enablePlayerRespawnCommand, settings.enableAdminTestCommands, settings.enableSpawnSafety, settings.enforceTDMTeamOutfits, settings.enableLowPopWarmup);
	}

	static int Validate(DAFDMSettings settings, DAFDMArenaRegistry arenas, DAFDMLoadoutRegistry loadouts)
	{
		int warnings = 0;
		warnings += ValidateSettings(settings, arenas, loadouts);
		warnings += ValidateArenas(settings, arenas);
		warnings += ValidateLoadouts(loadouts);
		return warnings;
	}

	private static int ValidateSettings(DAFDMSettings settings, DAFDMArenaRegistry arenas, DAFDMLoadoutRegistry loadouts)
	{
		if (!settings)
			return Warn("settings object is missing");

		int warnings = 0;
		bool hasTDMRound = false;

		if (!settings.roundTypes || settings.roundTypes.Count() == 0)
			warnings += Warn("no round types configured");

		foreach (DAFDMRoundTypeConfig roundType: settings.roundTypes)
		{
			if (!roundType)
			{
				warnings += Warn("round type entry is null");
				continue;
			}

			if (roundType.name == "")
				warnings += Warn("round type has an empty name");

			if (roundType.weight <= 0)
				warnings += Warn(string.Format("round type '%1' has non-positive weight %2", roundType.name, roundType.weight));

			string poolName = roundType.loadoutPool;
			if (poolName == "")
				poolName = "normal";

			if (!loadouts || !loadouts.GetPool(poolName))
				warnings += Warn(string.Format("round type '%1' points to missing loadout pool '%2'", roundType.name, poolName));

			if (roundType.arenaNames)
			{
				foreach (string arenaName: roundType.arenaNames)
				{
					if (arenaName != "" && arenas && !arenas.GetByName(arenaName))
						warnings += Warn(string.Format("round type '%1' references missing arena '%2'", roundType.name, arenaName));
				}
			}

			string mode = roundType.gameMode;
			mode.ToLower();
			if (mode == "tdm")
				hasTDMRound = true;
			else if (mode != "" && mode != "ffa")
				warnings += Warn(string.Format("round type '%1' has unknown gameMode '%2'", roundType.name, roundType.gameMode));
		}

		if (settings.arenaRotation)
		{
			foreach (string rotationArenaName: settings.arenaRotation)
			{
				if (rotationArenaName != "" && arenas && !arenas.GetByName(rotationArenaName))
					warnings += Warn(string.Format("arenaRotation references missing arena '%1'", rotationArenaName));
			}
		}

		if (settings.excludedArenas)
		{
			foreach (string excludedArenaName: settings.excludedArenas)
			{
				if (excludedArenaName != "" && arenas && !arenas.GetByName(excludedArenaName))
					warnings += Warn(string.Format("excludedArenas references missing arena '%1'", excludedArenaName));
			}
		}

		if (hasTDMRound)
			warnings += ValidateTDMSettings(settings);

		warnings += ValidateWarmupSettings(settings);

		return warnings;
	}

	private static int ValidateWarmupSettings(DAFDMSettings settings)
	{
		int warnings = 0;
		if (!settings.enableLowPopWarmup)
			return warnings;

		if (settings.warmupInfectedTargetCount > 0)
			warnings += WarnMissingClass(settings.warmupInfectedType, "low-pop warmup infected type");

		if (settings.warmupPlayerThreshold < 1)
			warnings += Warn("low-pop warmup threshold is below one real player");

		if (settings.warmupInfectedMaxSpawnDistance < settings.warmupInfectedMinSpawnDistance)
			warnings += Warn("low-pop warmup infected max spawn distance is below min spawn distance");

		return warnings;
	}

	private static int ValidateTDMSettings(DAFDMSettings settings)
	{
		int warnings = 0;

		if (!settings.teamNames || settings.teamNames.Count() < 2)
			warnings += Warn("TDM is configured but fewer than two team names are available");

		if (settings.enforceTDMTeamOutfits)
		{
			warnings += WarnMissingClass(settings.tdmRedJacket, "TDM red jacket");
			warnings += WarnMissingClass(settings.tdmRedPants, "TDM red pants");
			warnings += WarnMissingClass(settings.tdmRedShoes, "TDM red shoes");
			warnings += WarnMissingClass(settings.tdmRedMask, "TDM red mask");
			warnings += WarnMissingClass(settings.tdmRedArmband, "TDM red armband");
			warnings += WarnMissingClass(settings.tdmBlueJacket, "TDM blue jacket");
			warnings += WarnMissingClass(settings.tdmBluePants, "TDM blue pants");
			warnings += WarnMissingClass(settings.tdmBlueShoes, "TDM blue shoes");
			warnings += WarnMissingClass(settings.tdmBlueMask, "TDM blue mask");
			warnings += WarnMissingClass(settings.tdmBlueArmband, "TDM blue armband");
		}

		if (settings.preassignedTeams)
		{
			foreach (DAFDMTeamAssignmentConfig assignment: settings.preassignedTeams)
			{
				if (!assignment)
					continue;

				if (assignment.playerId == "")
					warnings += Warn("TDM preassigned team entry has an empty playerId");

				string assignedTeam = assignment.team;
				assignedTeam.ToLower();
				if (assignedTeam == "" || !settings.teamNames || settings.teamNames.Find(assignedTeam) < 0)
					warnings += Warn(string.Format("TDM preassigned player '%1' uses unknown team '%2'", assignment.playerId, assignment.team));
			}
		}

		return warnings;
	}

	private static int ValidateArenas(DAFDMSettings settings, DAFDMArenaRegistry arenas)
	{
		if (!arenas)
			return Warn("arena registry is missing");

		int warnings = 0;
		TStringArray seenNames = new TStringArray();
		array<ref DAFDMArena> allArenas = arenas.GetAll();

		if (!allArenas || allArenas.Count() == 0)
			return Warn("no arenas loaded");

		foreach (DAFDMArena arena: allArenas)
		{
			if (!arena)
			{
				warnings += Warn("arena entry is null");
				continue;
			}

			string name = arena.GetName();
			if (name == "")
				warnings += Warn("arena has an empty name");
			else if (seenNames.Find(name) >= 0)
				warnings += Warn(string.Format("duplicate arena name '%1'", name));
			else
				seenNames.Insert(name);

			array<vector> spawns = arena.GetPlayerSpawns();
			if (!spawns || spawns.Count() < 2)
				warnings += Warn(string.Format("arena '%1' has too few player spawns (%2)", name, CountSpawns(spawns)));

			warnings += ValidateArenaSpawnDistances(name, spawns);
		}

		if (CountEnabledArenas(settings, arenas) == 0)
			warnings += Warn("all arenas are excluded from active rotation");

		return warnings;
	}

	private static int ValidateArenaSpawnDistances(string arenaName, array<vector> spawns)
	{
		if (!spawns)
			return 0;

		int duplicateCount = 0;
		int closeCount = 0;
		int firstDuplicateA = -1;
		int firstDuplicateB = -1;
		float firstDuplicateDistance = 0;
		int firstCloseA = -1;
		int firstCloseB = -1;
		float firstCloseDistance = 0;

		for (int i = 0; i < spawns.Count(); i++)
		{
			vector a = spawns[i];
			for (int j = i + 1; j < spawns.Count(); j++)
			{
				float distance = vector.Distance(a, spawns[j]);
				if (distance <= DUPLICATE_SPAWN_DISTANCE)
				{
					if (duplicateCount == 0)
					{
						firstDuplicateA = i;
						firstDuplicateB = j;
						firstDuplicateDistance = distance;
					}
					duplicateCount++;
				}
				else if (distance < CLOSE_SPAWN_DISTANCE)
				{
					if (closeCount == 0)
					{
						firstCloseA = i;
						firstCloseB = j;
						firstCloseDistance = distance;
					}
					closeCount++;
				}
			}
		}

		int warnings = 0;
		if (duplicateCount > 0)
			warnings += Warn(string.Format("arena '%1' has %2 duplicate/overlapping spawn pairs; first pair %3/%4 distance=%5m", arenaName, duplicateCount, firstDuplicateA, firstDuplicateB, firstDuplicateDistance));

		if (closeCount > 0)
			warnings += Warn(string.Format("arena '%1' has %2 very close spawn pairs under %3m; first pair %4/%5 distance=%6m", arenaName, closeCount, CLOSE_SPAWN_DISTANCE, firstCloseA, firstCloseB, firstCloseDistance));

		return warnings;
	}

	private static int ValidateLoadouts(DAFDMLoadoutRegistry loadouts)
	{
		if (!loadouts)
			return Warn("loadout registry is missing");

		int warnings = 0;
		TStringArray seenPools = new TStringArray();
		array<ref DAFDMLoadoutPoolConfig> pools = loadouts.GetAll();

		if (!pools || pools.Count() == 0)
			return Warn("no loadout pools loaded");

		foreach (DAFDMLoadoutPoolConfig pool: pools)
		{
			if (!pool)
			{
				warnings += Warn("loadout pool entry is null");
				continue;
			}

			if (pool.name == "")
				warnings += Warn("loadout pool has an empty name");
			else if (seenPools.Find(pool.name) >= 0)
				warnings += Warn(string.Format("duplicate loadout pool name '%1'", pool.name));
			else
				seenPools.Insert(pool.name);

			if (!pool.entries || pool.entries.Count() == 0)
			{
				warnings += Warn(string.Format("loadout pool '%1' has no entries", pool.name));
				continue;
			}

			foreach (DAFDMLoadoutEntryConfig entry: pool.entries)
			{
				warnings += ValidateLoadoutEntry(pool.name, entry);
			}
		}

		return warnings;
	}

	private static int ValidateLoadoutEntry(string poolName, DAFDMLoadoutEntryConfig entry)
	{
		if (!entry)
			return Warn(string.Format("loadout pool '%1' has a null entry", poolName));

		int warnings = 0;
		string entryName = entry.name;
		if (entryName == "")
			entryName = "<unnamed>";

		if (entry.weight <= 0)
			warnings += Warn(string.Format("loadout '%1/%2' has non-positive weight %3", poolName, entryName, entry.weight));

		if (HasWeaponPoolConfigs(entry.primaryWeapons))
		{
			if (entry.primary && entry.primary.type != "")
				warnings += ValidateWeaponConfig(poolName, entryName, "primary", entry.primary);
		}
		else if (entry.primary && entry.primary.type != "")
		{
			warnings += ValidateWeaponConfig(poolName, entryName, "primary", entry.primary);
		}
		else
		{
			warnings += Warn(string.Format("loadout '%1/%2' has no primary weapon source", poolName, entryName));
		}

		if (HasWeaponPoolConfigs(entry.secondaryWeapons))
		{
			if (entry.secondary && entry.secondary.type != "")
				warnings += ValidateWeaponConfig(poolName, entryName, "secondary", entry.secondary);
		}
		else if (entry.secondary && entry.secondary.type != "")
		{
			warnings += ValidateWeaponConfig(poolName, entryName, "secondary", entry.secondary);
		}

		warnings += ValidateWeaponPoolConfigs(poolName, entryName, "primary", entry.primaryWeapons);
		warnings += ValidateWeaponPoolConfigs(poolName, entryName, "secondary", entry.secondaryWeapons);

		if (entry.outfit)
		{
			for (int outfitIndex = 0; outfitIndex < entry.outfit.Count(); outfitIndex++)
			{
				TStringArray outfitChoices = entry.outfit[outfitIndex];
				if (!outfitChoices || outfitChoices.Count() == 0)
					warnings += Warn(string.Format("loadout '%1/%2' outfit choice group %3 is empty", poolName, entryName, outfitIndex));
				else
					warnings += ValidateClassList(outfitChoices, string.Format("loadout '%1/%2' outfit group %3", poolName, entryName, outfitIndex));
			}
		}

		if (entry.items)
		{
			foreach (DAFDMLoadoutItemConfig item: entry.items)
			{
				if (!item)
					warnings += Warn(string.Format("loadout '%1/%2' has a null item entry", poolName, entryName));
				else
					warnings += WarnMissingClass(item.type, string.Format("loadout '%1/%2' item", poolName, entryName));
			}
		}

		return warnings;
	}

	private static bool HasWeaponPoolConfigs(array<ref DAFDMWeaponPoolConfig> weapons)
	{
		if (!weapons)
			return false;

		foreach (DAFDMWeaponPoolConfig weapon: weapons)
		{
			if (weapon && weapon.variants && weapon.variants.Count() > 0)
				return true;
		}

		return false;
	}

	private static int ValidateWeaponConfig(string poolName, string entryName, string slotName, DAFDMLoadoutWeaponConfig weapon)
	{
		if (!weapon)
			return 0;

		int warnings = 0;
		warnings += WarnMissingClass(weapon.type, string.Format("loadout '%1/%2' %3 weapon", poolName, entryName, slotName));
		warnings += ValidateClassList(weapon.fallbackTypes, string.Format("loadout '%1/%2' %3 fallback weapon", poolName, entryName, slotName));
		warnings += ValidateClassList(weapon.attachments, string.Format("loadout '%1/%2' %3 attachment", poolName, entryName, slotName));
		warnings += ValidateClassList(weapon.loadedMagazines, string.Format("loadout '%1/%2' %3 loaded magazine/ammo", poolName, entryName, slotName));
		warnings += ValidateClassList(weapon.spareMagazines, string.Format("loadout '%1/%2' %3 spare magazine/ammo", poolName, entryName, slotName));
		return warnings;
	}

	private static int ValidateWeaponPoolConfigs(string poolName, string entryName, string slotName, array<ref DAFDMWeaponPoolConfig> weapons)
	{
		if (!weapons)
			return 0;

		int warnings = 0;
		foreach (DAFDMWeaponPoolConfig weaponPool: weapons)
		{
			if (!weaponPool)
			{
				warnings += Warn(string.Format("loadout '%1/%2' %3 weapon pool entry is null", poolName, entryName, slotName));
				continue;
			}

			if (!weaponPool.variants || weaponPool.variants.Count() == 0)
				warnings += Warn(string.Format("loadout '%1/%2' %3 weapon pool '%4' has no variants", poolName, entryName, slotName, weaponPool.name));

			warnings += ValidateClassList(weaponPool.variants, string.Format("loadout '%1/%2' %3 weapon pool '%4' variant", poolName, entryName, slotName, weaponPool.name));
			if (weaponPool.attachments)
			{
				for (int attachmentGroup = 0; attachmentGroup < weaponPool.attachments.Count(); attachmentGroup++)
				{
					warnings += ValidateClassList(weaponPool.attachments[attachmentGroup], string.Format("loadout '%1/%2' %3 weapon pool '%4' attachment group %5", poolName, entryName, slotName, weaponPool.name, attachmentGroup));
				}
			}
			warnings += ValidateClassList(weaponPool.accessories, string.Format("loadout '%1/%2' %3 weapon pool '%4' accessory", poolName, entryName, slotName, weaponPool.name));
		}

		return warnings;
	}

	private static int ValidateClassList(TStringArray types, string context)
	{
		if (!types)
			return 0;

		int warnings = 0;
		TStringArray seenTypes = new TStringArray();
		foreach (string type: types)
		{
			if (seenTypes.Find(type) >= 0)
				continue;

			seenTypes.Insert(type);
			warnings += WarnMissingClass(type, context);
		}

		return warnings;
	}

	private static int WarnMissingClass(string type, string context)
	{
		if (type == "")
			return Warn(context + " has an empty class name");

		if (ClassExists(type))
			return 0;

		return Warn(string.Format("%1 references missing class '%2'", context, type));
	}

	private static bool ClassExists(string type)
	{
		return GetGame().ConfigIsExisting("CfgVehicles " + type) || GetGame().ConfigIsExisting("CfgWeapons " + type) || GetGame().ConfigIsExisting("CfgMagazines " + type) || GetGame().ConfigIsExisting("CfgAmmo " + type);
	}

	private static int Warn(string message)
	{
		Print("DAFDeathmatch config warning: " + message);
		return 1;
	}

	private static int CountRoundTypes(DAFDMSettings settings)
	{
		if (!settings || !settings.roundTypes)
			return 0;

		return settings.roundTypes.Count();
	}

	private static int CountArenas(DAFDMArenaRegistry arenas)
	{
		if (!arenas)
			return 0;

		return arenas.Count();
	}

	private static int CountEnabledArenas(DAFDMSettings settings, DAFDMArenaRegistry arenas)
	{
		if (!arenas)
			return 0;

		array<ref DAFDMArena> allArenas = arenas.GetAll();
		if (!allArenas)
			return 0;

		int count = 0;
		foreach (DAFDMArena arena: allArenas)
		{
			if (arena && (!settings || !arenas.IsExcluded(settings, arena.GetName())))
				count++;
		}

		return count;
	}

	private static int CountExcludedArenas(DAFDMSettings settings)
	{
		if (!settings || !settings.excludedArenas)
			return 0;

		return settings.excludedArenas.Count();
	}

	private static int CountLoadoutPools(DAFDMLoadoutRegistry loadouts)
	{
		if (!loadouts)
			return 0;

		return loadouts.Count();
	}

	private static int CountSpawns(array<vector> spawns)
	{
		if (!spawns)
			return 0;

		return spawns.Count();
	}

	private static string BuildRoundWeightSummary(DAFDMSettings settings)
	{
		if (!settings || !settings.roundTypes || settings.roundTypes.Count() == 0)
			return "none";

		string summary = "";
		foreach (DAFDMRoundTypeConfig roundType: settings.roundTypes)
		{
			if (!roundType)
				continue;

			if (summary != "")
				summary += ",";

			summary += string.Format("%1=%2", roundType.name, roundType.weight);
		}

		if (summary == "")
			return "none";

		return summary;
	}
}
