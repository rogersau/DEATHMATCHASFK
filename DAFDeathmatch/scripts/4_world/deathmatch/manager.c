class DAFDeathmatch
{
	private ref DAFDMSettings m_Settings;
	private ref DAFDMArenaRegistry m_Arenas;
	private ref DAFDMLoadoutRegistry m_Loadouts;
	private ref DAFDMScoreboard m_Scoreboard;
	private ref Timer m_RoundTimer;
	private DAFDMArena m_CurrentArena;
	private DAFDMRoundTypeConfig m_CurrentRoundType;
	private string m_LastRoundTypeName;
	private string m_ForcedRoundTypeName;
	private string m_ForcedArenaName;
	private bool m_RoundActive;
	private ref map<string, bool> m_AutoRespawnOverrides;
	private ref map<string, string> m_LastLoadoutByPlayer;
	private ref array<Object> m_RoundObjects;
	private ref array<ref DAFDMDeathDrop> m_DeathDrops;

	void DAFDeathmatch()
	{
		m_Settings = new DAFDMSettings();
		m_Settings.Load();

		m_Arenas = new DAFDMArenaRegistry();
		m_Arenas.Load();

		m_Loadouts = new DAFDMLoadoutRegistry();
		m_Loadouts.Load();

		m_Scoreboard = new DAFDMScoreboard();
		m_RoundTimer = new Timer();
		m_ForcedRoundTypeName = "";
		m_ForcedArenaName = "";
		m_AutoRespawnOverrides = new map<string, bool>();
		m_LastLoadoutByPlayer = new map<string, string>();
		m_RoundObjects = new array<Object>();
		m_DeathDrops = new array<ref DAFDMDeathDrop>();
	}

	void Start()
	{
		StartRound();
	}

	void StartRound()
	{
		CleanupRoundObjects();
		m_CurrentRoundType = PickRoundType();
		m_CurrentArena = PickForcedArena();
		if (!m_CurrentArena)
			m_CurrentArena = m_Arenas.PickArenaForRound(m_Settings, m_CurrentRoundType);

		if (!m_CurrentArena)
		{
			Print("DAFDeathmatch: cannot start round because no arena is configured");
			return;
		}

		m_RoundActive = true;
		m_Scoreboard.Reset();
		PrintFormat("DAFDeathmatch: %1 round started in arena %2", GetRoundDisplayName(), m_CurrentArena.GetName());

		SpawnArenaWalls();
		int roundMinutes = GetRoundMinutes();
		DAFDMRoundTimer.Start(roundMinutes, GetRoundDisplayName());
		m_RoundTimer.Stop();
		m_RoundTimer.Run(roundMinutes * 60, this, "EndRound");
		DAFDMChat.Announce("Round started: " + GetRoundDisplayName());

		RespawnAllPlayers();
	}

	void EndRound()
	{
		if (!m_RoundActive)
			return;

		m_RoundActive = false;
		DAFDMRoundTimer.Stop();
		CleanupRoundObjects();
		Print("DAFDeathmatch: round ended");

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.StartRound, m_Settings.roundTransitionSeconds * 1000, false);
	}

	void OnPlayerConnected(PlayerBase player)
	{
		if (!player)
			return;

		if (m_RoundActive)
			RespawnPlayer(player);
	}

	void OnPlayerKilled(PlayerBase victim, PlayerBase killer, EntityAI weapon, bool headshot)
	{
		if (!victim)
			return;

		if (killer && killer != victim)
			m_Scoreboard.AddKill(killer.GetIdentity(), victim.GetIdentity());
		else if (victim.GetIdentity())
			m_Scoreboard.Ensure(victim.GetIdentity()).deaths++;

		EntityAI deathDrop = HandleDeathDrop(victim);
		DeleteCorpseInventory(victim, deathDrop);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.CleanupCorpse, m_Settings.corpseCleanupSeconds * 1000, false, victim);

		PlayerIdentity identity = victim.GetIdentity();
		if (ShouldAutoRespawn(identity))
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.RespawnIdentity, m_Settings.respawnSeconds * 1000, false, identity, victim);
	}

	void OnPlayerPickedUpItem(PlayerBase player, EntityAI item)
	{
		if (!player || !item)
			return;

		for (int i = m_DeathDrops.Count() - 1; i >= 0; i--)
		{
			DAFDMDeathDrop drop = m_DeathDrops[i];
			if (!drop || drop.weapon != item)
				continue;

			if (player.GetIdentity() && player.GetIdentity().GetId() != drop.ownerId && drop.magazineType != "")
				player.GetHumanInventory().CreateInInventory(drop.magazineType);

			m_DeathDrops.Remove(i);
			return;
		}
	}

	void OnEvent(EventType eventTypeId, Param params)
	{
		if (eventTypeId != ChatMessageEventTypeID)
			return;

		ChatMessageEventParams chatParams = ChatMessageEventParams.Cast(params);
		if (!chatParams)
			return;

		OnChat(chatParams.param2, chatParams.param3);
	}

	void OnChat(string playerName, string message)
	{
		string args;
		string command = ParseCommand(message, args);
		if (command == "")
			return;

		PlayerBase source = FindPlayerByName(playerName);
		if (!source)
			return;

		HandleCommand(source, command, args);
	}

	string ParseCommand(string input, out string args)
	{
		args = "";

		if (!input || input.Length() < 2)
			return "";

		if (input.Substring(0, 1) != m_Settings.commandCharacter)
			return "";

		int space = input.IndexOf(" ");
		if (space == -1)
			space = input.Length();

		if (space < 2)
			return "";

		string command = input.Substring(1, space - 1);
		command.ToLower();

		if (space < input.Length())
			args = input.Substring(space + 1, input.Length() - space - 1);

		return command;
	}

	void HandleCommand(PlayerBase source, string command, string args)
	{
		if (command == "help")
		{
			SendHelp(source);
		}
		else if (command == "players")
		{
			DAFDMChat.MessagePlayer(source, string.Format("Players: %1", GetPlayerCount()));
		}
		else if (command == "timeleft")
		{
			DAFDMChat.MessagePlayer(source, "Time remaining: " + DAFDMRoundTimer.GetRemainingText());
		}
		else if (command == "score")
		{
			PlayerIdentity identity = source.GetIdentity();
			DAFDMChat.MessagePlayer(source, string.Format("Score: %1 K / %2 D", m_Scoreboard.GetKills(identity), m_Scoreboard.GetDeaths(identity)));
		}
		else if (command == "status")
		{
			SendStatus(source);
		}
		else if (command == "inspect")
		{
			SendInspect(source);
		}
		else if (command == "autorespawn")
		{
			ToggleAutoRespawn(source);
		}
		else if (command == "respawn")
		{
			RespawnPlayer(source);
			DAFDMChat.MessagePlayer(source, "Respawned");
		}
		else if (command == "version")
		{
			DAFDMChat.MessagePlayer(source, m_Settings.version);
		}
		else if (command == "endround")
		{
			if (!RequireAdmin(source))
				return;

			DAFDMChat.Announce("Admin ended the round");
			EndRound();
		}
		else if (command == "forceround")
		{
			if (!RequireAdmin(source))
				return;

			ForceRound(source, args);
		}
		else if (command == "forcearena")
		{
			if (!RequireAdmin(source))
				return;

			ForceArena(source, args);
		}
		else if (command == "forcenext")
		{
			if (!RequireAdmin(source))
				return;

			ForceNext(source, args);
		}
		else if (command == "reloadconfig")
		{
			if (!RequireAdmin(source))
				return;

			m_Settings.Load();
			m_Arenas.Load();
			m_Loadouts.Load();
			DAFDMChat.MessagePlayer(source, "Config reloaded for future rounds");
		}
		else
		{
			DAFDMChat.MessagePlayer(source, "Invalid command: " + command);
		}
	}

	void SendHelp(PlayerBase source)
	{
		string leader = m_Settings.commandCharacter;
		DAFDMChat.MessagePlayer(source, "Available commands:");
		DAFDMChat.MessagePlayer(source, leader + "help, " + leader + "players, " + leader + "timeleft");
		DAFDMChat.MessagePlayer(source, leader + "score, " + leader + "status, " + leader + "inspect, " + leader + "autorespawn, " + leader + "respawn");
		DAFDMChat.MessagePlayer(source, leader + "version, " + leader + "endround, " + leader + "forceround <type>");
		DAFDMChat.MessagePlayer(source, leader + "forcearena <arena>, " + leader + "forcenext <type> [arena], " + leader + "reloadconfig (admins)");
	}

	bool RequireAdmin(PlayerBase source)
	{
		if (source && m_Settings.IsAdmin(source.GetIdentity()))
			return true;

		DAFDMChat.MessagePlayer(source, "Admin command");
		return false;
	}

	void ToggleAutoRespawn(PlayerBase source)
	{
		PlayerIdentity identity = source.GetIdentity();
		if (!identity)
			return;

		bool enabled = !ShouldAutoRespawn(identity);
		m_AutoRespawnOverrides.Set(identity.GetId(), enabled);

		if (enabled)
			DAFDMChat.MessagePlayer(source, "Auto-respawn enabled");
		else
			DAFDMChat.MessagePlayer(source, "Auto-respawn disabled");
	}

	void SendStatus(PlayerBase source)
	{
		string roundName = GetRoundDisplayName();
		string arenaName = "none";
		if (m_CurrentArena)
			arenaName = m_CurrentArena.GetName();

		DAFDMChat.MessagePlayer(source, string.Format("Round: %1 | Arena: %2 | Time: %3 | Players: %4", roundName, arenaName, DAFDMRoundTimer.GetRemainingText(), GetPlayerCount()));
	}

	void SendInspect(PlayerBase source)
	{
		string arenaName = "none";
		if (m_CurrentArena)
			arenaName = m_CurrentArena.GetName();

		string playerId = "";
		if (source && source.GetIdentity())
			playerId = source.GetIdentity().GetId();

		string loadoutName = "none";
		if (playerId != "")
			m_LastLoadoutByPlayer.Find(playerId, loadoutName);

		string handsType = "none";
		if (source)
		{
			EntityAI inHands = source.GetHumanInventory().GetEntityInHands();
			if (inHands)
				handsType = inHands.GetType();
		}

		DAFDMChat.MessagePlayer(source, string.Format("Inspect: round=%1 arena=%2 pool=%3 loadout=%4 hands=%5", GetRoundDisplayName(), arenaName, GetRoundLoadoutPool(), loadoutName, handsType));
	}

	void ForceRound(PlayerBase source, string roundTypeName)
	{
		roundTypeName.ToLower();
		DAFDMRoundTypeConfig roundType = GetRoundTypeByName(roundTypeName);
		if (!roundType)
		{
			DAFDMChat.MessagePlayer(source, "Unknown round type: " + roundTypeName);
			return;
		}

		m_ForcedRoundTypeName = roundType.name;
		DAFDMChat.Announce("Admin forced next round: " + GetRoundTypeDisplayName(roundType));

		if (m_RoundActive)
			EndRound();
		else
			StartRound();
	}

	void ForceArena(PlayerBase source, string arenaName)
	{
		DAFDMArena arena = m_Arenas.GetByName(arenaName);
		if (!arena)
		{
			DAFDMChat.MessagePlayer(source, "Unknown arena: " + arenaName);
			return;
		}

		m_ForcedArenaName = arena.GetName();
		DAFDMChat.Announce("Admin forced next arena: " + arena.GetName());

		if (m_RoundActive)
			EndRound();
		else
			StartRound();
	}

	void ForceNext(PlayerBase source, string args)
	{
		int space = args.IndexOf(" ");
		string roundTypeName = args;
		string arenaName = "";
		if (space > 0)
		{
			roundTypeName = args.Substring(0, space);
			arenaName = args.Substring(space + 1, args.Length() - space - 1);
		}

		roundTypeName.ToLower();
		DAFDMRoundTypeConfig roundType = GetRoundTypeByName(roundTypeName);
		if (!roundType)
		{
			DAFDMChat.MessagePlayer(source, "Unknown round type: " + roundTypeName);
			return;
		}

		if (arenaName != "")
		{
			DAFDMArena arena = m_Arenas.GetByName(arenaName);
			if (!arena)
			{
				DAFDMChat.MessagePlayer(source, "Unknown arena: " + arenaName);
				return;
			}

			m_ForcedArenaName = arena.GetName();
		}

		m_ForcedRoundTypeName = roundType.name;
		if (m_ForcedArenaName != "")
			DAFDMChat.Announce("Admin forced next round: " + GetRoundTypeDisplayName(roundType) + " in " + m_ForcedArenaName);
		else
			DAFDMChat.Announce("Admin forced next round: " + GetRoundTypeDisplayName(roundType));

		if (m_RoundActive)
			EndRound();
		else
			StartRound();
	}

	bool ShouldAutoRespawn(PlayerIdentity identity)
	{
		if (!identity)
			return m_Settings.autoRespawn;

		bool enabled;
		if (m_AutoRespawnOverrides.Find(identity.GetId(), enabled))
			return enabled;

		return m_Settings.autoRespawn;
	}

	PlayerBase FindPlayerByName(string playerName)
	{
		array<Man> players = new array<Man>();
		GetGame().GetPlayers(players);

		foreach (Man man: players)
		{
			PlayerBase player = PlayerBase.Cast(man);
			if (player && player.GetIdentity() && player.GetIdentity().GetName() == playerName)
				return player;
		}

		return null;
	}

	int GetPlayerCount()
	{
		array<PlayerIdentity> identities = new array<PlayerIdentity>();
		GetGame().GetPlayerIndentities(identities);
		return identities.Count();
	}

	void RespawnAllPlayers()
	{
		array<Man> players = new array<Man>();
		GetGame().GetWorld().GetPlayerList(players);

		foreach (Man man: players)
		{
			PlayerBase player = PlayerBase.Cast(man);
			RespawnPlayer(player);
		}
	}

	void RespawnPlayer(PlayerBase player)
	{
		if (!player || !m_CurrentArena)
			return;

		vector position = m_CurrentArena.GetRandomPlayerSpawn();
		player.SetPosition(position);
		m_CurrentArena.FaceCenter(player);
		EquipPlayer(player);
	}

	void RespawnIdentity(PlayerIdentity identity, PlayerBase oldPlayer)
	{
		if (!identity || !m_CurrentArena)
			return;

		vector position = m_CurrentArena.GetRandomPlayerSpawn();
		PlayerBase player = PlayerBase.Cast(GetGame().CreatePlayer(identity, "SurvivorM_Mirek", position, 0, "NONE"));
		if (!player)
		{
			PrintFormat("DAFDeathmatch: failed to create respawn player for %1", identity.GetId());
			return;
		}

		GetGame().SelectPlayer(identity, player);
		m_CurrentArena.FaceCenter(player);
		EquipPlayer(player);

		if (oldPlayer && oldPlayer != player)
			GetGame().ObjectDelete(oldPlayer);
	}

	void EquipPlayer(PlayerBase player)
	{
		if (!player)
			return;

		ClearPlayerInventory(player);
		HumanInventory inventory = player.GetHumanInventory();
		DAFDMLoadoutEntryConfig loadout = PickLoadoutForPlayer(player);
		if (!loadout)
		{
			PrintFormat("DAFDeathmatch: no loadout found for pool %1", GetRoundLoadoutPool());
			return;
		}

		CreateOutfit(inventory, loadout);
		EntityAI primary = CreateLoadoutWeapon(player, inventory, loadout.primary, true);
		if (!primary)
			CreateEmergencyPrimary(player, inventory);

		CreateLoadoutWeapon(player, inventory, loadout.secondary, false);

		foreach (DAFDMLoadoutItemConfig itemConfig: loadout.items)
		{
			if (!itemConfig || itemConfig.type == "")
				continue;

			EntityAI item = inventory.CreateInInventory(itemConfig.type);
			if (item && itemConfig.quickbarSlot >= 0)
				player.SetQuickBarEntityShortcut(item, itemConfig.quickbarSlot, true);
		}
	}

	DAFDMLoadoutEntryConfig PickLoadoutForPlayer(PlayerBase player)
	{
		string id = "";
		if (player.GetIdentity())
			id = player.GetIdentity().GetId();

		string lastEntry;
		m_LastLoadoutByPlayer.Find(id, lastEntry);
		DAFDMLoadoutEntryConfig loadout = m_Loadouts.PickEntry(GetRoundLoadoutPool(), lastEntry);
		if (loadout)
			m_LastLoadoutByPlayer.Set(id, loadout.name);

		return loadout;
	}

	void CreateOutfit(HumanInventory inventory, DAFDMLoadoutEntryConfig loadout)
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

	EntityAI CreateLoadoutWeapon(PlayerBase player, HumanInventory inventory, DAFDMLoadoutWeaponConfig config, bool inHands)
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

		foreach (string spareMagazineType: config.spareMagazines)
		{
			inventory.CreateInInventory(spareMagazineType);
		}

		if (config.quickbarSlot >= 0)
			player.SetQuickBarEntityShortcut(weapon, config.quickbarSlot, true);

		return weapon;
	}

	EntityAI CreateEmergencyPrimary(PlayerBase player, HumanInventory inventory)
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
			}

			player.SetQuickBarEntityShortcut(weapon, 0, true);
			PrintFormat("DAFDeathmatch: created emergency primary weapon %1", weapon.GetType());
			return weapon;
		}

		Print("DAFDeathmatch: failed to create emergency primary weapon");
		return null;
	}

	void ClearPlayerInventory(PlayerBase player)
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

	EntityAI TryCreateLoadoutWeapon(HumanInventory inventory, string type, bool inHands)
	{
		if (!inventory || type == "")
			return null;

		if (inHands)
			return inventory.CreateInHands(type);

		return inventory.CreateInInventory(type);
	}

	DAFDMRoundTypeConfig PickRoundType()
	{
		if (m_ForcedRoundTypeName != "")
		{
			DAFDMRoundTypeConfig forced = GetRoundTypeByName(m_ForcedRoundTypeName);
			m_ForcedRoundTypeName = "";
			if (forced)
			{
				m_LastRoundTypeName = forced.name;
				return forced;
			}
		}

		DAFDMRoundTypeConfig picked = PickWeightedRoundType();
		if (picked && picked.name == m_LastRoundTypeName && m_Settings.roundTypes.Count() > 1)
			picked = PickWeightedRoundType();

		if (picked)
			m_LastRoundTypeName = picked.name;

		return picked;
	}

	DAFDMArena PickForcedArena()
	{
		if (m_ForcedArenaName == "")
			return null;

		DAFDMArena arena = m_Arenas.GetByName(m_ForcedArenaName);
		m_ForcedArenaName = "";
		if (arena)
			return arena;

		return null;
	}

	DAFDMRoundTypeConfig PickWeightedRoundType()
	{
		int total = 0;
		foreach (DAFDMRoundTypeConfig roundType: m_Settings.roundTypes)
		{
			if (roundType)
				total += Math.Max(roundType.weight, 1);
		}

		int roll = Math.RandomInt(0, total);
		int cursor = 0;
		foreach (DAFDMRoundTypeConfig candidate: m_Settings.roundTypes)
		{
			if (!candidate)
				continue;

			cursor += Math.Max(candidate.weight, 1);
			if (roll < cursor)
				return candidate;
		}

		return null;
	}

	DAFDMRoundTypeConfig GetRoundTypeByName(string name)
	{
		foreach (DAFDMRoundTypeConfig roundType: m_Settings.roundTypes)
		{
			if (roundType && roundType.name == name)
				return roundType;
		}

		return null;
	}

	string GetRoundTypeDisplayName(DAFDMRoundTypeConfig roundType)
	{
		if (!roundType)
			return "Normal";

		if (roundType.displayName != "")
			return roundType.displayName;

		return roundType.name;
	}

	string GetRoundDisplayName()
	{
		if (!m_CurrentRoundType)
			return "Normal";

		if (m_CurrentRoundType.displayName != "")
			return m_CurrentRoundType.displayName;

		return m_CurrentRoundType.name;
	}

	string GetRoundLoadoutPool()
	{
		if (m_CurrentRoundType && m_CurrentRoundType.loadoutPool != "")
			return m_CurrentRoundType.loadoutPool;

		return "normal";
	}

	int GetRoundMinutes()
	{
		if (m_CurrentRoundType && m_CurrentRoundType.roundMinutes > 0)
			return m_CurrentRoundType.roundMinutes;

		return m_Settings.roundMinutes;
	}

	void SpawnArenaWalls()
	{
		if (!m_Settings.spawnArenaWalls || !m_CurrentArena)
			return;

		if (m_CurrentArena.IsRectangular())
		{
			SpawnRectangularArenaWalls();
			return;
		}

		vector center = m_CurrentArena.GetCenter();
		float radius = m_CurrentArena.GetRadius();
		for (int i = 0; i < m_Settings.arenaWallSegments; i++)
		{
			float angle = ((Math.PI * 2) / m_Settings.arenaWallSegments) * i;
			vector position = Vector(center[0] + Math.Cos(angle) * radius, center[1], center[2] + Math.Sin(angle) * radius);
			position = DAFDMArena.SnapToGround(position);
			Object wall = GetGame().CreateObject(m_Settings.arenaWallType, position, false, true);
			if (wall)
			{
				wall.SetDirection(Vector(Math.Sin(angle), 0, Math.Cos(angle)));
				m_RoundObjects.Insert(wall);
			}
		}
	}

	void SpawnRectangularArenaWalls()
	{
		vector center = m_CurrentArena.GetCenter();
		float halfX = m_CurrentArena.GetXSize() * 0.5;
		float halfZ = m_CurrentArena.GetZSize() * 0.5;
		if (halfX <= 0 || halfZ <= 0)
			return;

		int xSegments = m_Settings.arenaWallSegments / 4;
		int zSegments = m_Settings.arenaWallSegments / 4;
		if (xSegments < 2)
			xSegments = 2;
		if (zSegments < 2)
			zSegments = 2;

		SpawnWallLine(Vector(center[0] - halfX, center[1], center[2] - halfZ), Vector(center[0] + halfX, center[1], center[2] - halfZ), xSegments, 90);
		SpawnWallLine(Vector(center[0] - halfX, center[1], center[2] + halfZ), Vector(center[0] + halfX, center[1], center[2] + halfZ), xSegments, 90);
		SpawnWallLine(Vector(center[0] - halfX, center[1], center[2] - halfZ), Vector(center[0] - halfX, center[1], center[2] + halfZ), zSegments, 0);
		SpawnWallLine(Vector(center[0] + halfX, center[1], center[2] - halfZ), Vector(center[0] + halfX, center[1], center[2] + halfZ), zSegments, 0);
	}

	void SpawnWallLine(vector start, vector finish, int segments, float direction)
	{
		for (int i = 0; i < segments; i++)
		{
			float t = (i + 0.5) / segments;
			vector position = Vector(start[0] + ((finish[0] - start[0]) * t), start[1], start[2] + ((finish[2] - start[2]) * t));
			position = DAFDMArena.SnapToGround(position);
			Object wall = GetGame().CreateObject(m_Settings.arenaWallType, position, false, true);
			if (wall)
			{
				wall.SetOrientation(Vector(direction, 0, 0));
				m_RoundObjects.Insert(wall);
			}
		}
	}

	void CleanupRoundObjects()
	{
		foreach (Object roundObject: m_RoundObjects)
		{
			if (roundObject)
				GetGame().ObjectDelete(roundObject);
		}

		m_RoundObjects.Clear();
		CleanupDeathDrops();
	}

	void CleanupCorpse(PlayerBase player)
	{
		if (player && !player.IsAlive())
			GetGame().ObjectDelete(player);
	}

	EntityAI HandleDeathDrop(PlayerBase victim)
	{
		EntityAI inHands = victim.GetHumanInventory().GetEntityInHands();
		Weapon_Base weapon = Weapon_Base.Cast(inHands);
		if (!weapon)
			return null;

		string ownerId = "";
		if (victim.GetIdentity())
			ownerId = victim.GetIdentity().GetId();

		victim.GetHumanInventory().DropEntity(InventoryMode.SERVER, victim, weapon);
		m_DeathDrops.Insert(new DAFDMDeathDrop(weapon, ownerId, GetPrimaryMagazineType(weapon)));
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.DeleteDeathDrop, m_Settings.deathDropCleanupSeconds * 1000, false, weapon);
		return weapon;
	}

	string GetPrimaryMagazineType(Weapon_Base weapon)
	{
		if (!weapon)
			return "";

		for (int i = 0; i < weapon.GetMuzzleCount(); i++)
		{
			Magazine magazine = weapon.GetMagazine(i);
			if (magazine)
				return magazine.GetType();
		}

		return "";
	}

	void DeleteCorpseInventory(PlayerBase victim, EntityAI deathDrop)
	{
		if (!victim)
			return;

		array<EntityAI> items = new array<EntityAI>();
		victim.GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);
		foreach (EntityAI item: items)
		{
			if (item && item != deathDrop)
				GetGame().ObjectDelete(item);
		}
	}

	void DeleteDeathDrop(EntityAI weapon)
	{
		bool tracked = false;
		for (int i = m_DeathDrops.Count() - 1; i >= 0; i--)
		{
			DAFDMDeathDrop drop = m_DeathDrops[i];
			if (drop && drop.weapon == weapon)
			{
				tracked = true;
				m_DeathDrops.Remove(i);
			}
		}

		if (tracked && weapon)
			GetGame().ObjectDelete(weapon);
	}

	void CleanupDeathDrops()
	{
		for (int i = m_DeathDrops.Count() - 1; i >= 0; i--)
		{
			DAFDMDeathDrop drop = m_DeathDrops[i];
			if (drop && drop.weapon)
				GetGame().ObjectDelete(drop.weapon);
		}

		m_DeathDrops.Clear();
	}

	DAFDMSettings GetSettings()
	{
		return m_Settings;
	}
}

class DAFDMDeathDrop
{
	EntityAI weapon;
	string ownerId;
	string magazineType;

	void DAFDMDeathDrop(EntityAI dropWeapon, string dropOwnerId, string dropMagazineType)
	{
		weapon = dropWeapon;
		ownerId = dropOwnerId;
		magazineType = dropMagazineType;
	}
}

static ref DAFDeathmatch g_DAFDeathmatch;

DAFDeathmatch GetDAFDeathmatch()
{
	return g_DAFDeathmatch;
}
