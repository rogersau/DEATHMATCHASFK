class DAFDeathmatch
{
	private static const int COMMAND_HELP = 1;
	private static const int COMMAND_PLAYERS = 2;
	private static const int COMMAND_TIMELEFT = 3;
	private static const int COMMAND_SCORE = 4;
	private static const int COMMAND_STATUS = 5;
	private static const int COMMAND_TEAMS = 6;
	private static const int COMMAND_INSPECT = 7;
	private static const int COMMAND_SPAWNREPORT = 8;
	private static const int COMMAND_AUTORESPAWN = 9;
	private static const int COMMAND_ENDVOTE = 10;
	private static const int COMMAND_ARENAVOTE = 11;
	private static const int COMMAND_ROUNDVOTE = 12;
	private static const int COMMAND_VOTE = 13;
	private static const int COMMAND_RESPAWN = 14;
	private static const int COMMAND_VERSION = 15;
	private static const int COMMAND_ENDROUND = 16;
	private static const int COMMAND_FORCEROUND = 17;
	private static const int COMMAND_FORCEARENA = 18;
	private static const int COMMAND_FORCENEXT = 19;
	private static const int COMMAND_FORCETDM = 20;
	private static const int COMMAND_SHUFFLETEAMS = 21;
	private static const int COMMAND_SPAWNDUMMY = 22;
	private static const int COMMAND_CLEARDUMMIES = 23;
	private static const int COMMAND_TESTDROP = 24;
	private static const int COMMAND_RELOADCONFIG = 25;
	private static const int COMMAND_DISCORDTEST = 26;

	private ref DAFDMSettings m_Settings;
	private ref DAFDMArenaRegistry m_Arenas;
	private ref DAFDMLoadoutRegistry m_Loadouts;
	private ref DAFDMScoreboard m_Scoreboard;
	private ref Timer m_RoundTimer;
	private DAFDMArena m_CurrentArena;
	private DAFDMRoundTypeConfig m_CurrentRoundType;
	private string m_CurrentGameMode;
	private string m_LastRoundTypeName;
	private string m_ForcedRoundTypeName;
	private string m_ForcedArenaName;
	private string m_ForcedGameMode;
	private bool m_RoundActive;
	private ref map<string, bool> m_AutoRespawnOverrides;
	private ref DAFDMLoadoutEngine m_LoadoutEngine;
	private ref DAFDMTeams m_Teams;
	private ref DAFDMSpawnSafety m_SpawnSafety;
	private ref array<Object> m_RoundObjects;
	private ref array<PlayerBase> m_TestDummies;
	private ref DAFDMDeathFlow m_DeathFlow;
	private ref DAFDMLowPopWarmup m_Warmup;
	private ref DAFDMDiscord m_Discord;
	private ref DAFDMVoteManager m_Votes;
	private ref map<string, int> m_CommandRoutes;
	private bool m_VoteTickStarted;

	void DAFDeathmatch()
	{
		m_Settings = new DAFDMSettings();
		m_Settings.Load();

		m_Arenas = new DAFDMArenaRegistry();
		m_Arenas.Load();

		m_Loadouts = new DAFDMLoadoutRegistry();
		m_Loadouts.Load();
		DAFDMConfigReport.PrintReport(m_Settings, m_Arenas, m_Loadouts, "startup");

		m_Scoreboard = new DAFDMScoreboard();
		m_RoundTimer = new Timer();
		m_CurrentGameMode = "ffa";
		m_ForcedRoundTypeName = "";
		m_ForcedArenaName = "";
		m_ForcedGameMode = "";
		m_AutoRespawnOverrides = new map<string, bool>();
		m_Teams = new DAFDMTeams(m_Settings);
		m_LoadoutEngine = new DAFDMLoadoutEngine(m_Settings, m_Loadouts, m_Teams);
		m_SpawnSafety = new DAFDMSpawnSafety(m_Settings, m_Teams);
		m_RoundObjects = new array<Object>();
		m_TestDummies = new array<PlayerBase>();
		m_DeathFlow = new DAFDMDeathFlow(m_Settings.corpseCleanupSeconds, m_Settings.deathDropCleanupSeconds);
		m_Warmup = new DAFDMLowPopWarmup(m_Settings);
		m_Discord = new DAFDMDiscord();
		m_Votes = new DAFDMVoteManager();
		m_CommandRoutes = new map<string, int>();
		RegisterCommandRoutes();
		m_VoteTickStarted = false;
		ApplyWeaponFireModeSetting();
	}

	void Start()
	{
		EnsureRoundReady();
		m_Warmup.StartTick(this);
		StartVoteTick();
		EvaluateWarmupState("start");
		m_Discord.PostServerReady(m_Settings);
	}

	void StartRound()
	{
		CleanupRoundObjects();
		m_CurrentRoundType = PickRoundType();
		m_CurrentGameMode = PickGameMode(m_CurrentRoundType);
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
		m_Teams.Reset();
		m_Discord.ResetRoundStats();
		PrintFormat("DAFDeathmatch: %1 round started in arena %2", GetRoundDisplayName(), m_CurrentArena.GetName());

		SpawnArenaWalls();
		int roundMinutes = GetRoundMinutes();
		DAFDMRoundTimer.Start(roundMinutes, GetRoundDisplayName());
		m_RoundTimer.Stop();
		m_RoundTimer.Run(roundMinutes * 60, this, "EndRound");
		DAFDMChat.Announce("Round started: " + GetRoundDisplayName());
		m_Discord.PostRoundStart(m_Settings, GetRoundDisplayName(), m_CurrentArena.GetName(), m_CurrentGameMode, roundMinutes);

		BroadcastClientState();
		RespawnAllPlayers();
		EvaluateWarmupState("round started");
	}

	void EndRound()
	{
		if (!m_RoundActive)
			return;

		m_RoundActive = false;
		DAFDMRoundTimer.Stop();
		BroadcastClientState();
		m_Discord.PostRoundEnd(m_Settings, m_Scoreboard, m_Settings, GetRoundDisplayName(), m_CurrentGameMode, m_Settings.teamNames);
		CleanupRoundObjects();
		Print("DAFDeathmatch: round ended");

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.StartRound, m_Settings.roundTransitionSeconds * 1000, false);
	}

	void OnPlayerConnected(PlayerBase player)
	{
		if (!player)
			return;

		m_Teams.Assign(player.GetIdentity(), m_CurrentGameMode);

		if (m_CurrentArena)
			m_CurrentArena.FaceCenter(player);

		EvaluateWarmupState("player connected");
		if (player.GetIdentity())
		{
			SendClientStateTo(player.GetIdentity());
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.SendClientStateTo, 1000, false, player.GetIdentity());
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.SendClientStateTo, 3000, false, player.GetIdentity());
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.SendClientStateTo, 6000, false, player.GetIdentity());
		}
	}

	void ResetConnectedPlayer(PlayerIdentity identity)
	{
		if (!identity)
			return;

		m_Scoreboard.ResetPlayer(identity);
		m_Teams.Assign(identity, m_CurrentGameMode);
		EvaluateWarmupState("player reset");
		SendClientStateTo(identity);
	}

	void OnPlayerKilled(PlayerBase victim, PlayerBase killer, EntityAI weapon, bool headshot)
	{
		if (!victim)
			return;

		HandleKillScore(victim, killer, weapon, headshot);

		UntrackTestDummy(victim);
		m_DeathFlow.OnKilled(victim);

		PlayerIdentity identity = victim.GetIdentity();
		if (ShouldAutoRespawn(identity))
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.RespawnIdentity, m_Settings.respawnSeconds * 1000, false, identity, victim);
	}

	void HandleKillScore(PlayerBase victim, PlayerBase killer, EntityAI weapon, bool headshot)
	{
		PlayerIdentity victimIdentity = victim.GetIdentity();
		PlayerIdentity killerIdentity;
		if (killer)
			killerIdentity = killer.GetIdentity();

		if (!killer || killer == victim)
		{
			m_Scoreboard.AddDeath(victimIdentity);
			return;
		}

		if (IsTeamRound())
		{
			string victimTeam = m_Teams.GetTeam(victimIdentity, m_CurrentGameMode);
			string killerTeam = m_Teams.GetTeam(killerIdentity, m_CurrentGameMode);
			if (victimTeam != "" && killerTeam != "" && victimTeam == killerTeam)
			{
				m_Scoreboard.AddDeath(victimIdentity);
				PrintFormat("DAFDeathmatch: friendly kill ignored for team score team=%1", killerTeam);
				return;
			}

			m_Scoreboard.AddKill(killerIdentity, victimIdentity);
			m_Scoreboard.AddTeamKill(killerTeam);
			OnScoredPvpKill(victim, killer, weapon, headshot);
			return;
		}

		m_Scoreboard.AddKill(killerIdentity, victimIdentity);
		OnScoredPvpKill(victim, killer, weapon, headshot);
	}

	void OnScoredPvpKill(PlayerBase victim, PlayerBase killer, EntityAI weapon, bool headshot)
	{
		if (!victim || !killer)
			return;

		if (IsTestDummy(victim) || IsTestDummy(killer))
			return;

		m_Discord.RegisterPvpKill(m_Settings, victim, killer, weapon, headshot);
		m_Discord.PostKillfeedKill(m_Settings, victim, killer, weapon, headshot);
	}

	void OnPlayerPickedUpItem(PlayerBase player, EntityAI item)
	{
		m_DeathFlow.OnItemPickedUp(player, item);

		Weapon_Base weapon = Weapon_Base.Cast(item);
		if (weapon)
			DAFWeaponFireModeHelper.SetPreferredFireMode(weapon);
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
		int route;
		if (!m_CommandRoutes.Find(command, route))
		{
			DAFDMChat.MessagePlayer(source, "Invalid command: " + command);
			return;
		}

		switch (route)
		{
		case COMMAND_HELP:
			SendHelp(source);
			break;
		case COMMAND_PLAYERS:
			DAFDMChat.MessagePlayer(source, string.Format("Players: %1", GetPlayerCount()));
			break;
		case COMMAND_TIMELEFT:
			DAFDMChat.MessagePlayer(source, "Time remaining: " + DAFDMRoundTimer.GetRemainingText());
			break;
		case COMMAND_SCORE:
			SendScore(source);
			break;
		case COMMAND_STATUS:
			SendStatus(source);
			break;
		case COMMAND_TEAMS:
			SendTeams(source);
			break;
		case COMMAND_INSPECT:
			SendInspect(source);
			break;
		case COMMAND_SPAWNREPORT:
			if (!RequireAdmin(source))
				return;

			SendSpawnReport(source);
			break;
		case COMMAND_AUTORESPAWN:
			ToggleAutoRespawn(source);
			break;
		case COMMAND_ENDVOTE:
			StartEndRoundVote(source);
			break;
		case COMMAND_ARENAVOTE:
			StartArenaVote(source, args);
			break;
		case COMMAND_ROUNDVOTE:
			StartRoundTypeVote(source, args);
			break;
		case COMMAND_VOTE:
			CastVote(source, args);
			break;
		case COMMAND_RESPAWN:
			if (!CanUseRespawnCommand(source))
				return;

			RespawnPlayer(source);
			DAFDMChat.MessagePlayer(source, "Respawned");
			break;
		case COMMAND_VERSION:
			DAFDMChat.MessagePlayer(source, m_Settings.version);
			break;
		case COMMAND_ENDROUND:
			if (!RequireAdmin(source))
				return;

			DAFDMChat.Announce("Admin ended the round");
			EndRound();
			break;
		case COMMAND_FORCEROUND:
			if (!RequireAdmin(source))
				return;

			ForceRound(source, args);
			break;
		case COMMAND_FORCEARENA:
			if (!RequireAdmin(source))
				return;

			ForceArena(source, args);
			break;
		case COMMAND_FORCENEXT:
			if (!RequireAdmin(source))
				return;

			ForceNext(source, args);
			break;
		case COMMAND_FORCETDM:
			if (!RequireAdmin(source))
				return;

			ForceTeamDeathmatch(source, args);
			break;
		case COMMAND_SHUFFLETEAMS:
			if (!RequireAdmin(source))
				return;

			ShuffleTeams(source);
			break;
		case COMMAND_SPAWNDUMMY:
			if (!RequireAdminTestCommand(source))
				return;

			SpawnTestDummy(source);
			break;
		case COMMAND_CLEARDUMMIES:
			if (!RequireAdminTestCommand(source))
				return;

			ClearTestDummies();
			DAFDMChat.MessagePlayer(source, "Cleared test dummies");
			break;
		case COMMAND_TESTDROP:
			if (!RequireAdminTestCommand(source))
				return;

			SpawnTestDrop(source, args);
			break;
		case COMMAND_RELOADCONFIG:
			if (!RequireAdmin(source))
				return;

			m_Settings.Load();
			m_Arenas.Load();
			m_Loadouts.Load();
			DAFDMConfigReport.PrintReport(m_Settings, m_Arenas, m_Loadouts, "reload");
			ApplyWeaponFireModeSetting();
			EvaluateWarmupState("reloadconfig");
			CancelVoteIfDisabled();
			DAFDMChat.MessagePlayer(source, "Config reloaded for future rounds");
			break;
		case COMMAND_DISCORDTEST:
			if (!RequireAdmin(source))
				return;

			DiscordTest(source, args);
			break;
		default:
			DAFDMChat.MessagePlayer(source, "Invalid command: " + command);
		}
	}

	void RegisterCommandRoutes()
	{
		m_CommandRoutes.Set("help", COMMAND_HELP);
		m_CommandRoutes.Set("players", COMMAND_PLAYERS);
		m_CommandRoutes.Set("timeleft", COMMAND_TIMELEFT);
		m_CommandRoutes.Set("score", COMMAND_SCORE);
		m_CommandRoutes.Set("status", COMMAND_STATUS);
		m_CommandRoutes.Set("teams", COMMAND_TEAMS);
		m_CommandRoutes.Set("inspect", COMMAND_INSPECT);
		m_CommandRoutes.Set("spawnreport", COMMAND_SPAWNREPORT);
		m_CommandRoutes.Set("autorespawn", COMMAND_AUTORESPAWN);
		m_CommandRoutes.Set("endvote", COMMAND_ENDVOTE);
		m_CommandRoutes.Set("arenavote", COMMAND_ARENAVOTE);
		m_CommandRoutes.Set("roundvote", COMMAND_ROUNDVOTE);
		m_CommandRoutes.Set("eventvote", COMMAND_ROUNDVOTE);
		m_CommandRoutes.Set("vote", COMMAND_VOTE);
		m_CommandRoutes.Set("respawn", COMMAND_RESPAWN);
		m_CommandRoutes.Set("version", COMMAND_VERSION);
		m_CommandRoutes.Set("endround", COMMAND_ENDROUND);
		m_CommandRoutes.Set("forceround", COMMAND_FORCEROUND);
		m_CommandRoutes.Set("forcearena", COMMAND_FORCEARENA);
		m_CommandRoutes.Set("forcenext", COMMAND_FORCENEXT);
		m_CommandRoutes.Set("forcetdm", COMMAND_FORCETDM);
		m_CommandRoutes.Set("shuffleteams", COMMAND_SHUFFLETEAMS);
		m_CommandRoutes.Set("spawndummy", COMMAND_SPAWNDUMMY);
		m_CommandRoutes.Set("cleardummies", COMMAND_CLEARDUMMIES);
		m_CommandRoutes.Set("testdrop", COMMAND_TESTDROP);
		m_CommandRoutes.Set("reloadconfig", COMMAND_RELOADCONFIG);
		m_CommandRoutes.Set("discordtest", COMMAND_DISCORDTEST);
	}

	void SendScore(PlayerBase source)
	{
		PlayerIdentity identity = source.GetIdentity();
		if (IsTeamRound())
		{
			string team = m_Teams.GetTeam(identity, m_CurrentGameMode);
			DAFDMChat.MessagePlayer(source, string.Format("Score: %1 K / %2 D | Team: %3 %4", m_Scoreboard.GetKills(identity), m_Scoreboard.GetDeaths(identity), team, m_Scoreboard.GetTeamScore(team)));
			return;
		}

		DAFDMChat.MessagePlayer(source, string.Format("Score: %1 K / %2 D", m_Scoreboard.GetKills(identity), m_Scoreboard.GetDeaths(identity)));
	}

	void SendHelp(PlayerBase source)
	{
		string leader = m_Settings.commandCharacter;
		DAFDMChat.MessagePlayer(source, "Available commands:");
		DAFDMChat.MessagePlayer(source, leader + "help, " + leader + "players, " + leader + "timeleft");
		DAFDMChat.MessagePlayer(source, leader + "score, " + leader + "status, " + leader + "teams, " + leader + "inspect, " + leader + "autorespawn, " + leader + "respawn");
		DAFDMChat.MessagePlayer(source, leader + "endvote, " + leader + "arenavote <arena>, " + leader + "roundvote <type>, " + leader + "eventvote <type>, " + leader + "vote 1|2");
		DAFDMChat.MessagePlayer(source, leader + "version, " + leader + "endround, " + leader + "forceround <type>");
		DAFDMChat.MessagePlayer(source, leader + "forcearena <arena>, " + leader + "forcenext <type> [arena], " + leader + "forcetdm <type> [arena], " + leader + "spawnreport, " + leader + "reloadconfig (admins)");
		DAFDMChat.MessagePlayer(source, leader + "shuffleteams (admins)");
		DAFDMChat.MessagePlayer(source, leader + "discordtest killfeed|events (admins)");
		DAFDMChat.MessagePlayer(source, leader + "spawndummy, " + leader + "cleardummies, " + leader + "testdrop [weapon] [bonus] (admins)");
	}

	bool RequireAdmin(PlayerBase source)
	{
		if (source && m_Settings.IsAdmin(source.GetIdentity()))
			return true;

		DAFDMChat.MessagePlayer(source, "Admin command");
		return false;
	}

	bool CanUseRespawnCommand(PlayerBase source)
	{
		if (!source)
			return false;

		if (m_Settings.IsAdmin(source.GetIdentity()))
			return true;

		if (m_Settings.enablePlayerRespawnCommand)
			return true;

		DAFDMChat.MessagePlayer(source, "Respawn command is admin only");
		return false;
	}

	bool RequireAdminTestCommand(PlayerBase source)
	{
		if (!RequireAdmin(source))
			return false;

		if (m_Settings.enableAdminTestCommands)
			return true;

		DAFDMChat.MessagePlayer(source, "Admin test commands are disabled");
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

	void DiscordTest(PlayerBase source, string args)
	{
		string endpoint = args;
		endpoint.ToLower();
		if (endpoint == "")
		{
			DAFDMChat.MessagePlayer(source, "Usage: @discordtest killfeed|events");
			return;
		}

		bool killfeed = endpoint == "killfeed";
		bool events = endpoint == "events";
		if (!killfeed && !events)
		{
			DAFDMChat.MessagePlayer(source, "Unknown endpoint: " + args);
			return;
		}

		bool enabled;
		if (killfeed)
			enabled = m_Settings.enableDiscordKillfeed;
		else
			enabled = m_Settings.enableDiscordServerEvents;
		if (!enabled)
		{
			DAFDMChat.MessagePlayer(source, "Discord " + endpoint + " endpoint is disabled");
			return;
		}

		if (killfeed)
		{
			if (!m_Discord.WebhookReady(m_Settings.discordKillfeedWebhookUrl))
			{
				DAFDMChat.MessagePlayer(source, "Discord killfeed endpoint is enabled but the webhook URL is missing or invalid");
				return;
			}
		}
		else
		{
			if (!m_Discord.WebhookReady(m_Settings.discordServerEventsWebhookUrl))
			{
				DAFDMChat.MessagePlayer(source, "Discord events endpoint is enabled but the webhook URL is missing or invalid");
				return;
			}
		}

		m_Discord.TestEndpoint(m_Settings, killfeed);
		DAFDMChat.MessagePlayer(source, "Discord " + endpoint + " test posted");
	}

	void SendStatus(PlayerBase source)
	{
		string roundName = GetRoundDisplayName();
		string arenaName = "none";
		if (m_CurrentArena)
			arenaName = m_CurrentArena.GetName();

		string warmup = "";
		if (m_Warmup.IsActive())
			warmup = " | Warmup Mode";

		if (IsTeamRound())
			DAFDMChat.MessagePlayer(source, string.Format("Round: %1 | Mode: TDM | Arena: %2 | Time: %3 | Players: %4%5", roundName, arenaName, DAFDMRoundTimer.GetRemainingText(), GetPlayerCount(), warmup));
		else
			DAFDMChat.MessagePlayer(source, string.Format("Round: %1 | Mode: FFA | Arena: %2 | Time: %3 | Players: %4%5", roundName, arenaName, DAFDMRoundTimer.GetRemainingText(), GetPlayerCount(), warmup));
	}

	void SendTeams(PlayerBase source)
	{
		if (!IsTeamRound())
		{
			DAFDMChat.MessagePlayer(source, "Teams inactive: current round is FFA");
			return;
		}

		string sourceTeam = m_Teams.GetTeam(source.GetIdentity(), m_CurrentGameMode);
		DAFDMChat.MessagePlayer(source, "Your team: " + sourceTeam);
		foreach (string team: m_Settings.teamNames)
		{
			if (team == "")
				continue;

			team.ToLower();
			DAFDMChat.MessagePlayer(source, string.Format("Team %1: players=%2 score=%3", team, m_Teams.CountAssigned(team), m_Scoreboard.GetTeamScore(team)));
		}
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
			loadoutName = m_LoadoutEngine.GetLastLoadoutName(playerId);

		string handsType = "none";
		if (source)
		{
			EntityAI inHands = source.GetHumanInventory().GetEntityInHands();
			if (inHands)
				handsType = inHands.GetType();
		}

		string mode = "ffa";
		string team = "";
		if (IsTeamRound())
		{
			mode = "tdm";
			team = m_Teams.GetTeam(source.GetIdentity(), m_CurrentGameMode);
		}

		DAFDMChat.MessagePlayer(source, string.Format("Inspect: round=%1 mode=%2 team=%3 arena=%4 pool=%5 loadout=%6 hands=%7", GetRoundDisplayName(), mode, team, arenaName, GetRoundLoadoutPool(), loadoutName, handsType));
		DAFDMChat.MessagePlayer(source, string.Format("Inspect: dummies=%1 drops=%2 corpses=%3 dropTtl=%4s corpseTtl=%5s", GetActiveTestDummyCount(), m_DeathFlow.GetActiveDeathDropCount(), m_DeathFlow.GetPendingCorpseCleanupCount(), m_Settings.deathDropCleanupSeconds, m_Settings.corpseCleanupSeconds));
		DAFDMChat.MessagePlayer(source, string.Format("Inspect: spawnSafety=%1 minPlayer=%2m minEnemy=%3m view=%4m angle=%5deg", m_Settings.enableSpawnSafety, m_Settings.spawnSafetyMinPlayerDistance, m_Settings.spawnSafetyMinEnemyDistance, m_Settings.spawnSafetyEnemyViewDistance, m_Settings.spawnSafetyEnemyViewAngleDegrees));
		DAFDMChat.MessagePlayer(source, string.Format("Inspect: warmup=%1 pending=%2 infected=%3 threshold=%4 delay=%5s", m_Warmup.IsActive(), m_Warmup.IsPending(), m_Warmup.GetActiveInfectedCount(), m_Settings.warmupPlayerThreshold, m_Settings.warmupActivateDelaySeconds));
		DAFDMChat.MessagePlayer(source, "Inspect: " + m_Votes.GetActiveSummary());
	}

	void StartEndRoundVote(PlayerBase source)
	{
		if (!m_Settings.enableEndRoundVote)
		{
			DAFDMChat.MessagePlayer(source, "End-round voting is disabled");
			return;
		}

		StartVote(source, "endround", "", "end current round");
	}

	void StartArenaVote(PlayerBase source, string arenaName)
	{
		if (!m_Settings.enableArenaVote)
		{
			DAFDMChat.MessagePlayer(source, "Arena voting is disabled");
			return;
		}

		if (arenaName == "")
		{
			DAFDMChat.MessagePlayer(source, "Usage: @arenavote <arena>");
			return;
		}

		DAFDMArena arena = m_Arenas.GetByName(arenaName);
		if (!arena)
		{
			DAFDMChat.MessagePlayer(source, "Unknown arena: " + arenaName);
			return;
		}

		StartVote(source, "arena", arena.GetName(), "next arena: " + arena.GetName());
	}

	void StartRoundTypeVote(PlayerBase source, string roundTypeName)
	{
		if (!m_Settings.enableRoundTypeVote)
		{
			DAFDMChat.MessagePlayer(source, "Round-type voting is disabled");
			return;
		}

		if (roundTypeName == "")
		{
			DAFDMChat.MessagePlayer(source, "Usage: @roundvote <round type>");
			return;
		}

		roundTypeName.ToLower();
		DAFDMRoundTypeConfig roundType = GetRoundTypeByName(roundTypeName);
		if (!roundType)
		{
			DAFDMChat.MessagePlayer(source, "Unknown round type: " + roundTypeName);
			return;
		}

		StartVote(source, "roundtype", roundType.name, "next round type: " + GetRoundTypeDisplayName(roundType));
	}

	void StartVote(PlayerBase source, string voteType, string target, string label)
	{
		if (!m_Settings.enableVoting)
		{
			DAFDMChat.MessagePlayer(source, "Voting is disabled");
			return;
		}

		if (GetPlayerCount() < m_Settings.voteMinimumPlayers)
		{
			DAFDMChat.MessagePlayer(source, string.Format("Voting requires at least %1 players", m_Settings.voteMinimumPlayers));
			return;
		}

		if (m_Votes.HasActiveVote())
		{
			DAFDMChat.MessagePlayer(source, m_Votes.GetActiveSummary());
			return;
		}

		PlayerIdentity identity = source.GetIdentity();
		if (!m_Votes.StartVote(identity, voteType, target, label, m_Settings.voteDurationSeconds))
		{
			DAFDMChat.MessagePlayer(source, "Could not start vote");
			return;
		}

		DAFDMChat.Announce(string.Format("%1 started a vote: %2. Use @vote 1 for yes or @vote 2 for no. Ends in %3s.", identity.GetName(), label, m_Settings.voteDurationSeconds));
	}

	void CastVote(PlayerBase source, string args)
	{
		args.TrimInPlace();
		if (args != "1" && args != "2")
		{
			DAFDMChat.MessagePlayer(source, "Usage: @vote 1 for yes, @vote 2 for no");
			return;
		}

		bool yes = args == "1";
		bool passed;
		string voteType;
		string target;
		string label;
		string message;
		bool finished = m_Votes.CastVote(source.GetIdentity(), yes, GetPlayerCount(), passed, voteType, target, label, message);

		if (finished)
		{
			if (message != "")
				DAFDMChat.Announce(message);

			ApplyVoteResult(passed, voteType, target, label);
		}
		else if (message != "")
		{
			if (message.IndexOf("Vote tally:") == 0)
				DAFDMChat.Announce(message);
			else
				DAFDMChat.MessagePlayer(source, message);
		}
	}

	void StartVoteTick()
	{
		if (m_VoteTickStarted)
			return;

		m_VoteTickStarted = true;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.VoteTick, 1000, true);
	}

	void VoteTick()
	{
		if (!m_Settings.enableVoting)
		{
			CancelVoteIfDisabled();
			return;
		}

		bool passed;
		string voteType;
		string target;
		string label;
		string message;
		bool finished = m_Votes.CheckExpiredOrMajority(GetPlayerCount(), passed, voteType, target, label, message);
		if (!finished)
			return;

		if (message != "")
			DAFDMChat.Announce(message);

		ApplyVoteResult(passed, voteType, target, label);
	}

	void CancelVoteIfDisabled()
	{
		if (m_Settings.enableVoting)
			return;

		string message;
		m_Votes.Cancel(message);
		if (message != "")
			DAFDMChat.Announce(message);
	}

	void ApplyVoteResult(bool passed, string voteType, string target, string label)
	{
		if (!passed)
			return;

		if (voteType == "endround")
		{
			DAFDMChat.Announce("Vote result applied: ending current round");
			EndRound();
			return;
		}

		if (voteType == "arena")
		{
			m_ForcedArenaName = target;
			DAFDMChat.Announce("Vote result applied: next arena will be " + target);
			return;
		}

		if (voteType == "roundtype")
		{
			m_ForcedRoundTypeName = target;
			m_ForcedGameMode = "";
			DAFDMChat.Announce("Vote result applied: next round type will be " + target);
		}
	}

	void SendSpawnReport(PlayerBase source)
	{
		EnsureRoundReady();

		if (!source || !m_CurrentArena)
		{
			DAFDMChat.MessagePlayer(source, "SpawnReport: no active arena");
			return;
		}

		DAFDMSpawnCandidate candidate = PickSpawnCandidate(source.GetIdentity(), source, false);
		if (!candidate)
		{
			DAFDMChat.MessagePlayer(source, "SpawnReport: no spawn candidate available");
			return;
		}

		string mode = "ffa";
		string team = "";
		if (IsTeamRound())
		{
			mode = "tdm";
			team = m_Teams.GetTeam(source.GetIdentity(), m_CurrentGameMode);
		}

		DAFDMChat.MessagePlayer(source, string.Format("SpawnReport: arena=%1 mode=%2 team=%3 chosen=%4/%5 safe=%6 fallback=%7 score=%8 pos=%9", m_CurrentArena.GetName(), mode, team, candidate.index, candidate.totalSpawns, !candidate.rejected, candidate.fallback, candidate.score, candidate.position));
		DAFDMChat.MessagePlayer(source, string.Format("SpawnReport: reason=%1 players=%2 enemies=%3 friends=%4 closePlayers=%5 closeEnemies=%6 viewThreats=%7", candidate.reason, candidate.playerCount, candidate.enemyCount, candidate.friendlyCount, candidate.closePlayerCount, candidate.closeEnemyCount, candidate.viewThreatCount));
		DAFDMChat.MessagePlayer(source, string.Format("SpawnReport: nearestPlayer=%1m %2 nearestEnemy=%3m %4 thresholds player=%5m enemy=%6m view=%7m/%8deg", candidate.nearestPlayerDistance, candidate.nearestPlayerName, candidate.nearestEnemyDistance, candidate.nearestEnemyName, m_Settings.spawnSafetyMinPlayerDistance, m_Settings.spawnSafetyMinEnemyDistance, m_Settings.spawnSafetyEnemyViewDistance, m_Settings.spawnSafetyEnemyViewAngleDegrees));
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
		ForceNextWithMode(source, args, "");
	}

	void ForceTeamDeathmatch(PlayerBase source, string args)
	{
		if (args == "")
			args = "normal";

		ForceNextWithMode(source, args, "tdm");
	}

	void ForceNextWithMode(PlayerBase source, string args, string gameMode)
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
		m_ForcedGameMode = gameMode;

		string modeText = "";
		if (gameMode != "")
			modeText = " (" + gameMode + ")";

		if (m_ForcedArenaName != "")
			DAFDMChat.Announce("Admin forced next round: " + GetRoundTypeDisplayName(roundType) + modeText + " in " + m_ForcedArenaName);
		else
			DAFDMChat.Announce("Admin forced next round: " + GetRoundTypeDisplayName(roundType) + modeText);

		if (m_RoundActive)
			EndRound();
		else
			StartRound();
	}

	void ShuffleTeams(PlayerBase source)
	{
		if (!IsTeamRound())
		{
			DAFDMChat.MessagePlayer(source, "Cannot shuffle teams during FFA");
			return;
		}

		m_Teams.Reset();
		array<Man> players = new array<Man>();
		GetGame().GetWorld().GetPlayerList(players);
		foreach (Man man: players)
		{
			PlayerBase player = PlayerBase.Cast(man);
			if (player && player.GetIdentity() && player.IsAlive() && !IsTestDummy(player))
				m_Teams.Assign(player.GetIdentity(), m_CurrentGameMode);
		}

		DAFDMChat.Announce("Admin shuffled teams");
		RespawnAllPlayers();
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
			if (player && !IsTestDummy(player))
				RespawnPlayer(player);
		}
	}

	void RespawnPlayer(PlayerBase player)
	{
		if (!player || !m_CurrentArena)
			return;

		m_Teams.Assign(player.GetIdentity(), m_CurrentGameMode);
		vector position = PickSafeSpawn(player.GetIdentity(), player);
		player.SetPosition(position);
		m_CurrentArena.FaceCenter(player);
		EquipPlayer(player);
		SendClientStateTo(player.GetIdentity());
	}

	void RespawnIdentity(PlayerIdentity identity, PlayerBase oldPlayer)
	{
		if (!identity || !m_CurrentArena)
			return;

		m_Teams.Assign(identity, m_CurrentGameMode);
		vector position = PickSafeSpawn(identity, oldPlayer);
		PlayerBase player = PlayerBase.Cast(GetGame().CreatePlayer(identity, "SurvivorM_Mirek", position, 0, "NONE"));
		if (!player)
		{
			PrintFormat("DAFDeathmatch: failed to create respawn player for %1", identity.GetId());
			return;
		}

		GetGame().SelectPlayer(identity, player);
		m_CurrentArena.FaceCenter(player);
		EquipPlayer(player);
		SendClientStateTo(identity);
		SendRespawnCursorFix(identity);
	}

	void SendRespawnCursorFix(PlayerIdentity identity)
	{
		DAFRPC.SendRespawnCursorFix(identity);
	}

	vector GetRandomPlayerSpawnPosition()
	{
		return GetPlayerSpawnPosition(null);
	}

	vector GetPlayerSpawnPosition(PlayerIdentity identity)
	{
		EnsureRoundReady();
		m_Teams.Assign(identity, m_CurrentGameMode);

		if (m_CurrentArena)
			return PickSafeSpawn(identity, null);

		return "0 0 0";
	}

	void StartingEquipSetup(PlayerBase player)
	{
		EquipPlayer(player);
	}

	void EnsureRoundReady()
	{
		if (m_RoundActive && m_CurrentArena)
			return;

		StartRound();
	}

	vector PickSafeSpawn(PlayerIdentity identity, PlayerBase currentPlayer)
	{
		DAFDMSpawnCandidate candidate = PickSpawnCandidate(identity, currentPlayer, true);
		if (!candidate)
			return "0 0 0";

		return candidate.position;
	}

	DAFDMSpawnCandidate PickSpawnCandidate(PlayerIdentity identity, PlayerBase currentPlayer, bool includeRandom)
	{
		return m_SpawnSafety.Pick(identity, currentPlayer, m_CurrentArena, m_CurrentGameMode, includeRandom, BuildSpawnIgnoreList());
	}

	array<string> BuildSpawnIgnoreList()
	{
		array<string> ignore = new array<string>();
		foreach (PlayerBase dummy: m_TestDummies)
		{
			if (dummy && dummy.GetIdentity())
				ignore.Insert(dummy.GetIdentity().GetId());
		}

		return ignore;
	}

	bool IsTeamRound()
	{
		return DAFDMTeams.IsTeamMode(m_CurrentGameMode);
	}

	void SpawnTestDummy(PlayerBase source)
	{
		if (!m_CurrentArena)
		{
			DAFDMChat.MessagePlayer(source, "No active arena");
			return;
		}

		vector position = GetTestDummyPosition(source);
		PlayerBase dummy = PlayerBase.Cast(GetGame().CreateObject("SurvivorM_Mirek", position, false, true));
		if (!dummy)
		{
			DAFDMChat.MessagePlayer(source, "Failed to spawn test dummy");
			Print("DAFDeathmatch: failed to spawn test dummy");
			return;
		}

		m_TestDummies.Insert(dummy);
		if (source)
			dummy.SetDirection(vector.Direction(dummy.GetPosition(), source.GetPosition()));
		else
			m_CurrentArena.FaceCenter(dummy);

		EquipPlayer(dummy);
		DAFDMChat.MessagePlayer(source, "Spawned test dummy");
	}

	void SpawnTestDrop(PlayerBase source, string args)
	{
		string weaponType = "Winchester70";
		string bonusType = "Ammo_308Win";
		ParseTestDropArgs(args, weaponType, bonusType);

		vector position = GetTestDropPosition(source);
		EntityAI weapon = EntityAI.Cast(GetGame().CreateObject(weaponType, position, false, true));
		if (!weapon)
		{
			DAFDMChat.MessagePlayer(source, "Failed to spawn test drop: " + weaponType);
			PrintFormat("DAFDeathmatch: failed to spawn test drop %1", weaponType);
			return;
		}

		Weapon_Base weaponBase = Weapon_Base.Cast(weapon);
		if (weaponBase && bonusType != "")
			weaponBase.SpawnAmmo(bonusType, Weapon_Base.SAMF_DEFAULT);

		m_DeathFlow.TrackDeathDrop(weapon, "DAF_TEST_DROP", bonusType);
		DAFDMChat.MessagePlayer(source, string.Format("Spawned test drop: %1 bonus=%2", weapon.GetType(), bonusType));
	}

	void ParseTestDropArgs(string args, out string weaponType, out string bonusType)
	{
		if (args == "")
			return;

		int space = args.IndexOf(" ");
		if (space <= 0)
		{
			weaponType = args;
			bonusType = GuessDeathDropBonusType(weaponType);
			return;
		}

		weaponType = args.Substring(0, space);
		bonusType = args.Substring(space + 1, args.Length() - space - 1);
	}

	string GuessDeathDropBonusType(string weaponType)
	{
		if (weaponType == "M4A1" || weaponType == "M16A2" || weaponType == "Aug" || weaponType == "AugShort" || weaponType == "FAMAS")
			return "Mag_STANAG_30Rnd";

		if (weaponType == "AKM")
			return "Mag_AKM_30Rnd";

		if (weaponType == "AK74" || weaponType == "AKS74U")
			return "Mag_AK74_30Rnd";

		if (weaponType == "Mosin9130" || weaponType == "SVD" || weaponType == "SV98")
			return "Ammo_762x54";

		return "Ammo_308Win";
	}

	vector GetTestDropPosition(PlayerBase source)
	{
		vector position;
		if (source)
			position = source.ModelToWorld("0 0 3");
		else if (m_CurrentArena)
			position = m_CurrentArena.GetRandomPlayerSpawn();
		else
			position = "0 0 0";

		position = DAFDMArena.SnapToGround(position);
		return position + "0 0.1 0";
	}

	vector GetTestDummyPosition(PlayerBase source)
	{
		vector position;
		if (source)
			position = source.ModelToWorld("0 0 12");
		else
			position = m_CurrentArena.GetRandomPlayerSpawn();

		position = DAFDMArena.SnapToGround(position);
		return position + "0 0.1 0";
	}

	void EquipPlayer(PlayerBase player)
	{
		m_LoadoutEngine.EquipPlayer(player, GetRoundLoadoutPool(), m_CurrentGameMode);
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

	string PickGameMode(DAFDMRoundTypeConfig roundType)
	{
		if (m_ForcedGameMode != "")
		{
			string forcedMode = m_ForcedGameMode;
			forcedMode.ToLower();
			m_ForcedGameMode = "";
			return forcedMode;
		}

		if (!roundType || roundType.gameMode == "")
			return "ffa";

		string mode = roundType.gameMode;
		mode.ToLower();
		if (mode == "tdm")
			return "tdm";

		return "ffa";
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

		m_CurrentArena.Prepare(m_RoundObjects);
		m_CurrentArena.Enclose(m_RoundObjects, m_Settings.arenaWallType);
	}

	void CleanupRoundObjects()
	{
		m_Warmup.Cleanup();
		ClearTestDummies();
		m_DeathFlow.CleanupAll();

		foreach (Object roundObject: m_RoundObjects)
		{
			if (roundObject)
				GetGame().ObjectDelete(roundObject);
		}

		m_RoundObjects.Clear();
	}

	bool IsTestDummy(PlayerBase player)
	{
		return player && m_TestDummies && m_TestDummies.Find(player) >= 0;
	}

	void UntrackTestDummy(PlayerBase player)
	{
		if (!player || !m_TestDummies)
			return;

		int index = m_TestDummies.Find(player);
		if (index >= 0)
			m_TestDummies.Remove(index);
	}

	void ClearTestDummies()
	{
		for (int i = m_TestDummies.Count() - 1; i >= 0; i--)
		{
			PlayerBase dummy = m_TestDummies[i];
			if (dummy)
				GetGame().ObjectDelete(dummy);
		}

		m_TestDummies.Clear();
	}

	int GetActiveTestDummyCount()
	{
		int count = 0;
		foreach (PlayerBase dummy: m_TestDummies)
		{
			if (dummy)
				count++;
		}

		return count;
	}

	void WarmupTick()
	{
		if (m_Warmup.Tick(GetPlayerCount(), m_RoundActive, m_CurrentArena, GetWarmupIgnorePlayerIds(), "tick"))
			BroadcastClientState();
	}

	void EvaluateWarmupState(string source)
	{
		if (m_Warmup.Evaluate(GetPlayerCount(), m_RoundActive, m_CurrentArena, GetWarmupIgnorePlayerIds(), source))
			BroadcastClientState();
	}

	bool ShouldBlockWarmupInfectedDamage(EntityAI source)
	{
		return m_Warmup.ShouldBlockInfectedDamage(source);
	}

	void OnPlayerDisconnected(PlayerBase player)
	{
		EvaluateWarmupState("player disconnected");
		BroadcastClientState();
	}

	void BroadcastClientState()
	{
		int roundSeconds = DAFDMRoundTimer.GetRemainingSeconds();
		string roundLabel = "";
		if (m_RoundActive)
			roundLabel = GetRoundDisplayName();

		DAFRPC.BroadcastRoundHudState(roundSeconds, roundLabel, GetPlayerCount(), GetClientPlayerCountStatus(), IsManualRespawnAllowedForClients());
	}

	void SendClientStateTo(PlayerIdentity identity)
	{
		if (!identity)
			return;

		int roundSeconds = DAFDMRoundTimer.GetRemainingSeconds();
		string roundLabel = "";
		if (m_RoundActive)
			roundLabel = GetRoundDisplayName();

		DAFRPC.SendRoundHudState(identity, roundSeconds, roundLabel, GetPlayerCount(), GetClientPlayerCountStatus(), IsManualRespawnAllowedForClients());
	}

	string GetClientPlayerCountStatus()
	{
		return m_Warmup.GetStatusString();
	}

	array<string> GetWarmupIgnorePlayerIds()
	{
		array<string> ids = new array<string>();
		foreach (PlayerBase dummy: m_TestDummies)
		{
			if (dummy && dummy.GetIdentity())
				ids.Insert(dummy.GetIdentity().GetId());
		}

		return ids;
	}

	bool IsManualRespawnAllowedForClients()
	{
		return m_Settings.enablePlayerRespawnCommand;
	}

	DAFDMSettings GetSettings()
	{
		return m_Settings;
	}

	void ApplyWeaponFireModeSetting()
	{
		DAFWeaponFireModeHelper.SetForcePreferredFireMode(m_Settings.forcePreferredWeaponFireMode);
	}
}

static ref DAFDeathmatch g_DAFDeathmatch;

DAFDeathmatch GetDAFDeathmatch()
{
	return g_DAFDeathmatch;
}
