class DAFDeathmatch
{
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
	private ref map<string, string> m_LastLoadoutByPlayer;
	private ref DAFDMTeams m_Teams;
	private ref DAFDMSpawnSafety m_SpawnSafety;
	private ref array<Object> m_RoundObjects;
	private ref array<PlayerBase> m_TestDummies;
	private ref DAFDMDeathFlow m_DeathFlow;
	private bool m_WarmupActive;
	private bool m_WarmupTickStarted;
	private int m_WarmupActivationDueTime;
	private ref array<ZombieBase> m_WarmupInfected;
	private ref DAFDMDiscord m_Discord;
	private ref DAFDMVoteManager m_Votes;
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
		m_LastLoadoutByPlayer = new map<string, string>();
		m_Teams = new DAFDMTeams(m_Settings);
		m_SpawnSafety = new DAFDMSpawnSafety(m_Settings, m_Teams);
		m_RoundObjects = new array<Object>();
		m_TestDummies = new array<PlayerBase>();
		m_DeathFlow = new DAFDMDeathFlow(m_Settings.corpseCleanupSeconds, m_Settings.deathDropCleanupSeconds);
		m_WarmupActive = false;
		m_WarmupTickStarted = false;
		m_WarmupActivationDueTime = 0;
		m_WarmupInfected = new array<ZombieBase>();
		m_Discord = new DAFDMDiscord();
		m_Votes = new DAFDMVoteManager();
		m_VoteTickStarted = false;
	}

	void Start()
	{
		EnsureRoundReady();
		StartWarmupTick();
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
			if (IsTeamRound())
			{
				string team = m_Teams.GetTeam(identity, m_CurrentGameMode);
				DAFDMChat.MessagePlayer(source, string.Format("Score: %1 K / %2 D | Team: %3 %4", m_Scoreboard.GetKills(identity), m_Scoreboard.GetDeaths(identity), team, m_Scoreboard.GetTeamScore(team)));
			}
			else
			{
				DAFDMChat.MessagePlayer(source, string.Format("Score: %1 K / %2 D", m_Scoreboard.GetKills(identity), m_Scoreboard.GetDeaths(identity)));
			}
		}
		else if (command == "status")
		{
			SendStatus(source);
		}
		else if (command == "teams")
		{
			SendTeams(source);
		}
		else if (command == "inspect")
		{
			SendInspect(source);
		}
		else if (command == "spawnreport")
		{
			if (!RequireAdmin(source))
				return;

			SendSpawnReport(source);
		}
		else if (command == "autorespawn")
		{
			ToggleAutoRespawn(source);
		}
		else if (command == "endvote")
		{
			StartEndRoundVote(source);
		}
		else if (command == "arenavote")
		{
			StartArenaVote(source, args);
		}
		else if (command == "roundvote")
		{
			StartRoundTypeVote(source, args);
		}
		else if (command == "eventvote")
		{
			StartRoundTypeVote(source, args);
		}
		else if (command == "vote")
		{
			CastVote(source, args);
		}
		else if (command == "respawn")
		{
			if (!CanUseRespawnCommand(source))
				return;

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
		else if (command == "forcetdm")
		{
			if (!RequireAdmin(source))
				return;

			ForceTeamDeathmatch(source, args);
		}
		else if (command == "shuffleteams")
		{
			if (!RequireAdmin(source))
				return;

			ShuffleTeams(source);
		}
		else if (command == "spawndummy")
		{
			if (!RequireAdminTestCommand(source))
				return;

			SpawnTestDummy(source);
		}
		else if (command == "cleardummies")
		{
			if (!RequireAdminTestCommand(source))
				return;

			ClearTestDummies();
			DAFDMChat.MessagePlayer(source, "Cleared test dummies");
		}
		else if (command == "testdrop")
		{
			if (!RequireAdminTestCommand(source))
				return;

			SpawnTestDrop(source, args);
		}
		else if (command == "reloadconfig")
		{
			if (!RequireAdmin(source))
				return;

			m_Settings.Load();
			m_Arenas.Load();
			m_Loadouts.Load();
			DAFDMConfigReport.PrintReport(m_Settings, m_Arenas, m_Loadouts, "reload");
			EvaluateWarmupState("reloadconfig");
			CancelVoteIfDisabled();
			DAFDMChat.MessagePlayer(source, "Config reloaded for future rounds");
		}
		else if (command == "discordtest")
		{
			if (!RequireAdmin(source))
				return;

			DiscordTest(source, args);
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
		if (m_WarmupActive)
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
			m_LastLoadoutByPlayer.Find(playerId, loadoutName);

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
		DAFDMChat.MessagePlayer(source, string.Format("Inspect: warmup=%1 pending=%2 infected=%3 threshold=%4 delay=%5s", m_WarmupActive, IsWarmupPending(), GetActiveWarmupInfectedCount(), m_Settings.warmupPlayerThreshold, m_Settings.warmupActivateDelaySeconds));
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
		ApplyTeamOutfit(player);
		RepairPlayerAttachments(player);
		EntityAI primary = CreateLoadoutWeapon(player, inventory, loadout.primary, true);
		if (!primary)
			primary = CreateEmergencyPrimary(player, inventory);

		EntityAI secondary = CreateLoadoutWeapon(player, inventory, loadout.secondary, false);

		foreach (DAFDMLoadoutItemConfig itemConfig: loadout.items)
		{
			if (!itemConfig || itemConfig.type == "")
				continue;

			EntityAI item = inventory.CreateInInventory(itemConfig.type);
			if (item && itemConfig.quickbarSlot >= 0)
				player.SetQuickBarEntityShortcut(item, itemConfig.quickbarSlot, true);
		}

		NormalizeLoadoutHotbar(player, primary, secondary);
	}

	void NormalizeLoadoutHotbar(PlayerBase player, EntityAI primary, EntityAI secondary)
	{
		if (!player)
			return;

		if (primary)
			player.SetQuickBarEntityShortcut(primary, 0, true);

		if (secondary)
			player.SetQuickBarEntityShortcut(secondary, 1, true);

		EntityAI knife = EnsureKnife(player);
		EntityAI morphine = EnsureLoadoutItem(player, "Morphine", "morphine");
		EntityAI bandage = EnsureLoadoutItem(player, "BandageDressing", "bandage");
		EntityAI saline = EnsureLoadoutItem(player, "SalineBagIV", "saline");

		if (knife)
			player.SetQuickBarEntityShortcut(knife, 2, true);

		if (morphine)
			player.SetQuickBarEntityShortcut(morphine, 3, true);

		if (bandage)
			player.SetQuickBarEntityShortcut(bandage, 4, true);

		if (saline)
			player.SetQuickBarEntityShortcut(saline, 5, true);
	}

	EntityAI EnsureKnife(PlayerBase player)
	{
		TStringArray knifeTypes = new TStringArray();
		knifeTypes.Insert("CombatKnife");
		knifeTypes.Insert("HuntingKnife");
		knifeTypes.Insert("KitchenKnife");
		knifeTypes.Insert("SteakKnife");
		knifeTypes.Insert("StoneKnife");
		return EnsureLoadoutItemAny(player, knifeTypes, "knife");
	}

	EntityAI EnsureLoadoutItem(PlayerBase player, string type, string label)
	{
		TStringArray types = new TStringArray();
		types.Insert(type);
		return EnsureLoadoutItemAny(player, types, label);
	}

	EntityAI EnsureLoadoutItemAny(PlayerBase player, TStringArray types, string label)
	{
		EntityAI existing = FindInventoryItemAny(player, types);
		if (existing)
			return existing;

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

	EntityAI FindInventoryItemAny(PlayerBase player, TStringArray types)
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

	bool ItemMatchesAnyType(EntityAI item, TStringArray types)
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

	void ApplyTeamOutfit(PlayerBase player)
	{
		if (!player || !IsTeamRound() || !m_Settings.enforceTDMTeamOutfits)
			return;

		string team = m_Teams.GetTeam(player.GetIdentity(), m_CurrentGameMode);
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

	void GetTeamOutfitTypes(string team, out string jacket, out string pants, out string shoes, out string mask, out string armband)
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

	void ReplacePlayerAttachment(PlayerBase player, string slotName, string itemType)
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

	void RepairPlayerAttachments(PlayerBase player)
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

	void RepairEntityDamage(EntityAI item)
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
		CleanupWarmupInfected();
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

	void StartWarmupTick()
	{
		if (m_WarmupTickStarted)
			return;

		m_WarmupTickStarted = true;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.WarmupTick, m_Settings.warmupTickSeconds * 1000, true);
	}

	void WarmupTick()
	{
		EvaluateWarmupState("tick");
		BroadcastWarmupHud();

		if (!m_WarmupActive || !m_RoundActive || !m_CurrentArena)
			return;

		MaintainWarmupInfected();
		RefillWarmupAmmo();
	}

	void EvaluateWarmupState(string source)
	{
		int playerCount = GetPlayerCount();
		bool shouldWarmup = m_Settings.enableLowPopWarmup && playerCount > 0 && playerCount <= m_Settings.warmupPlayerThreshold;
		if (!shouldWarmup)
		{
			m_WarmupActivationDueTime = 0;
			if (m_WarmupActive)
				DeactivateWarmup(source, playerCount);
			else
				BroadcastWarmupHud();

			return;
		}

		if (m_WarmupActive)
		{
			BroadcastWarmupHud();
			return;
		}

		int now = GetGame().GetTime();
		if (m_WarmupActivationDueTime <= 0)
		{
			m_WarmupActivationDueTime = now + (m_Settings.warmupActivateDelaySeconds * 1000);
			PrintFormat("DAFDeathmatch: low-pop warmup pending source=%1 players=%2 delay=%3s", source, playerCount, m_Settings.warmupActivateDelaySeconds);
		}

		if (now >= m_WarmupActivationDueTime)
			ActivateWarmup(source, playerCount);
	}

	void ActivateWarmup(string source, int playerCount)
	{
		if (m_WarmupActive)
			return;

		m_WarmupActive = true;
		m_WarmupActivationDueTime = 0;
		PrintFormat("DAFDeathmatch: low-pop warmup activated source=%1 players=%2 threshold=%3", source, playerCount, m_Settings.warmupPlayerThreshold);
		BroadcastWarmupHud();
		MaintainWarmupInfected();
		RefillWarmupAmmo();
	}

	void DeactivateWarmup(string source, int playerCount)
	{
		if (!m_WarmupActive)
			return;

		m_WarmupActive = false;
		m_WarmupActivationDueTime = 0;
		CleanupWarmupInfected();
		PrintFormat("DAFDeathmatch: low-pop warmup deactivated source=%1 players=%2 threshold=%3", source, playerCount, m_Settings.warmupPlayerThreshold);
		BroadcastWarmupHud();
	}

	bool IsWarmupActive()
	{
		return m_WarmupActive;
	}

	bool IsWarmupPending()
	{
		return !m_WarmupActive && m_WarmupActivationDueTime > 0;
	}

	bool ShouldBlockWarmupInfectedDamage(EntityAI source)
	{
		return m_WarmupActive && source && source.IsInherited(DayZInfected);
	}

	void OnPlayerDisconnected(PlayerBase player)
	{
		EvaluateWarmupState("player disconnected");
		BroadcastClientState();
	}

	void BroadcastWarmupHud()
	{
		BroadcastClientState();
	}

	void SendWarmupHudTo(PlayerIdentity identity)
	{
		SendClientStateTo(identity);
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
		if (m_WarmupActive)
			return "Warmup Mode";

		return "";
	}

	bool IsManualRespawnAllowedForClients()
	{
		return m_Settings.enablePlayerRespawnCommand;
	}

	void MaintainWarmupInfected()
	{
		CullWarmupInfected();

		if (m_Settings.warmupInfectedTargetCount <= 0)
			return;

		while (m_WarmupInfected.Count() < m_Settings.warmupInfectedTargetCount)
		{
			if (!SpawnWarmupInfected())
				return;
		}
	}

	void CullWarmupInfected()
	{
		for (int i = m_WarmupInfected.Count() - 1; i >= 0; i--)
		{
			ZombieBase infected = m_WarmupInfected[i];
			if (!infected || infected.ToDelete() || !infected.IsAlive())
			{
				if (infected && !infected.ToDelete())
					GetGame().ObjectDelete(infected);

				m_WarmupInfected.Remove(i);
			}
		}
	}

	bool SpawnWarmupInfected()
	{
		vector position = PickWarmupInfectedPosition();
		Object spawnedObject = GetGame().CreateObjectEx(m_Settings.warmupInfectedType, position, ECE_INITAI | ECE_PLACE_ON_SURFACE);
		if (!spawnedObject)
		{
			PrintFormat("DAFDeathmatch: failed to spawn warmup infected type=%1 at %2", m_Settings.warmupInfectedType, position.ToString(true));
			return false;
		}

		ZombieBase infected = ZombieBase.Cast(spawnedObject);
		if (!infected)
		{
			PrintFormat("DAFDeathmatch: warmup infected type=%1 did not create a ZombieBase", m_Settings.warmupInfectedType);
			GetGame().ObjectDelete(spawnedObject);
			return false;
		}

		m_WarmupInfected.Insert(infected);
		ApplyWarmupInfectedMovement(infected);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.ApplyWarmupInfectedMovement, 500, false, infected);
		return true;
	}

	void ApplyWarmupInfectedMovement(ZombieBase infected)
	{
		if (!infected || infected.ToDelete())
			return;

		DayZInfectedInputController controller = infected.GetInputController();
		if (controller)
			controller.OverrideMovementSpeed(true, m_Settings.warmupInfectedMovementSpeed);
	}

	vector PickWarmupInfectedPosition()
	{
		vector anchor = GetWarmupAnchorPosition();
		float angle = Math.RandomFloatInclusive(0, Math.PI * 2);
		float distance = Math.RandomFloatInclusive(m_Settings.warmupInfectedMinSpawnDistance, m_Settings.warmupInfectedMaxSpawnDistance);
		vector position = Vector(anchor[0] + (Math.Cos(angle) * distance), anchor[1], anchor[2] + (Math.Sin(angle) * distance));
		position = ClampWarmupPositionToArena(position);
		return DAFDMArena.SnapToGround(position);
	}

	vector GetWarmupAnchorPosition()
	{
		PlayerBase player = GetWarmupAnchorPlayer();
		if (player)
			return player.GetPosition();

		if (m_CurrentArena)
			return m_CurrentArena.GetCenter();

		return "0 0 0";
	}

	PlayerBase GetWarmupAnchorPlayer()
	{
		array<Man> players = new array<Man>();
		GetGame().GetPlayers(players);

		foreach (Man man: players)
		{
			PlayerBase player = PlayerBase.Cast(man);
			if (player && player.GetIdentity() && player.IsAlive() && !IsTestDummy(player))
				return player;
		}

		return null;
	}

	vector ClampWarmupPositionToArena(vector position)
	{
		if (!m_CurrentArena)
			return position;

		vector center = m_CurrentArena.GetCenter();
		if (m_CurrentArena.IsRectangular())
		{
			float halfX = (m_CurrentArena.GetXSize() * 0.5) - 5;
			float halfZ = (m_CurrentArena.GetZSize() * 0.5) - 5;
			if (halfX > 1)
				position[0] = Math.Clamp(position[0], center[0] - halfX, center[0] + halfX);
			if (halfZ > 1)
				position[2] = Math.Clamp(position[2], center[2] - halfZ, center[2] + halfZ);

			return position;
		}

		float radius = m_CurrentArena.GetRadius() - 5;
		if (radius <= 1)
			return position;

		float dx = position[0] - center[0];
		float dz = position[2] - center[2];
		float distance = Math.Sqrt((dx * dx) + (dz * dz));
		if (distance <= radius || distance <= 0.1)
			return position;

		position[0] = center[0] + ((dx / distance) * radius);
		position[2] = center[2] + ((dz / distance) * radius);
		return position;
	}

	void CleanupWarmupInfected()
	{
		for (int i = m_WarmupInfected.Count() - 1; i >= 0; i--)
		{
			ZombieBase infected = m_WarmupInfected[i];
			if (infected && !infected.ToDelete())
				GetGame().ObjectDelete(infected);
		}

		m_WarmupInfected.Clear();
	}

	int GetActiveWarmupInfectedCount()
	{
		CullWarmupInfected();
		return m_WarmupInfected.Count();
	}

	void RefillWarmupAmmo()
	{
		if (!m_Settings.warmupUnlimitedAmmo)
			return;

		array<Man> players = new array<Man>();
		GetGame().GetPlayers(players);

		foreach (Man man: players)
		{
			PlayerBase player = PlayerBase.Cast(man);
			if (player && player.GetIdentity() && player.IsAlive() && !IsTestDummy(player))
				RefillWarmupPlayerMagazines(player);
		}
	}

	void RefillWarmupPlayerMagazines(PlayerBase player)
	{
		bool changed = false;
		array<EntityAI> items = new array<EntityAI>();
		player.GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);

		EntityAI inHands = player.GetHumanInventory().GetEntityInHands();
		foreach (EntityAI item: items)
		{
			changed = RefillWarmupMagazine(item, inHands, false) || changed;
		}

		changed = RefillWarmupMagazine(inHands, inHands, false) || changed;
		if (changed)
			player.SetSynchDirty();
	}

	bool RefillWarmupMagazine(EntityAI item, EntityAI inHands, bool allowWeaponMagazine)
	{
		bool changed = false;
		Weapon_Base weapon = Weapon_Base.Cast(item);
		if (weapon)
		{
			bool isHeldWeapon = item == inHands;
			for (int muzzle = 0; muzzle < weapon.GetMuzzleCount(); muzzle++)
			{
				if (!isHeldWeapon)
					changed = RefillWarmupMagazine(weapon.GetMagazine(muzzle), inHands, true) || changed;
			}

			if (changed)
			{
				weapon.SetSynchDirty();
				weapon.Synchronize();
			}
		}

		Magazine magazine = Magazine.Cast(item);
		if (!magazine || magazine.IsAmmoPile())
			return changed;

		if (!allowWeaponMagazine && Weapon_Base.Cast(magazine.GetHierarchyParent()))
			return changed;

		if (magazine.GetAmmoCount() >= magazine.GetAmmoMax())
			return changed;

		magazine.ServerSetAmmoMax();
		magazine.SetSynchDirty();
		changed = true;
		return changed;
	}

	DAFDMSettings GetSettings()
	{
		return m_Settings;
	}
}

static ref DAFDeathmatch g_DAFDeathmatch;

DAFDeathmatch GetDAFDeathmatch()
{
	return g_DAFDeathmatch;
}
