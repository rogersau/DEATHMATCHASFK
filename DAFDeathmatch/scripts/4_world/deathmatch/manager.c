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
		DAFDMRoundTimer.Start(m_Settings.roundMinutes, GetRoundDisplayName());
		m_RoundTimer.Stop();
		m_RoundTimer.Run(m_Settings.roundMinutes * 60, this, "EndRound");
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

		if (ShouldAutoRespawn(victim.GetIdentity()))
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.RespawnPlayer, m_Settings.respawnSeconds * 1000, false, victim);
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
		else if (command == "autorespawn")
		{
			ToggleAutoRespawn(source);
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
		DAFDMChat.MessagePlayer(source, leader + "score, " + leader + "autorespawn, " + leader + "version");
		DAFDMChat.MessagePlayer(source, leader + "endround, " + leader + "reloadconfig (admins)");
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

	void EquipPlayer(PlayerBase player)
	{
		if (!player)
			return;

		player.RemoveAllItems();
		HumanInventory inventory = player.GetHumanInventory();
		DAFDMLoadoutEntryConfig loadout = PickLoadoutForPlayer(player);
		if (!loadout)
			return;

		CreateLoadoutWeapon(player, inventory, loadout.primary, true);
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

	EntityAI CreateLoadoutWeapon(PlayerBase player, HumanInventory inventory, DAFDMLoadoutWeaponConfig config, bool inHands)
	{
		if (!inventory || !config)
			return null;

		EntityAI weapon;
		if (inHands)
			weapon = inventory.CreateInHands(config.type);
		else
			weapon = inventory.CreateInInventory(config.type);

		if (!weapon)
			return null;

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

	DAFDMRoundTypeConfig PickRoundType()
	{
		DAFDMRoundTypeConfig picked = PickWeightedRoundType();
		if (picked && picked.name == m_LastRoundTypeName && m_Settings.roundTypes.Count() > 1)
			picked = PickWeightedRoundType();

		if (picked)
			m_LastRoundTypeName = picked.name;

		return picked;
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

	void SpawnArenaWalls()
	{
		if (!m_Settings.spawnArenaWalls || !m_CurrentArena)
			return;

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
