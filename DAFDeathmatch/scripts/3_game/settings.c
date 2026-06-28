class DAFDMWeaponConfig
{
	string weapon;
	ref TStringArray attachments = new TStringArray();
	ref TStringArray magazines = new TStringArray();
}

class DAFDMRoundTypeConfig
{
	string name;
	string displayName;
	string gameMode = "ffa";
	int weight = 1;
	int roundMinutes = 0;
	string loadoutPool;
	ref TStringArray arenaNames = new TStringArray();
}

class DAFDMTeamAssignmentConfig
{
	string playerId;
	string team;
}

class DAFDMSettings
{
	private static ref DAFDMSettings s_Settings;

	int roundMinutes = 20;
	int respawnSeconds = 3;
	int roundTransitionSeconds = 10;
	bool autoRespawn = true;
	string commandCharacter = "@";
	string version = "DAFDeathmatch first playable";
	bool spawnArenaWalls = true;
	string arenaWallType = "Land_ConcreteBlock";
	int arenaWallSegments = 24;
	int corpseCleanupSeconds = 5;
	int deathDropCleanupSeconds = 60;
	bool enableAdminTestCommands = false;
	bool enablePlayerRespawnCommand = false;
	bool enableSpawnSafety = true;
	float spawnSafetyMinPlayerDistance = 12;
	float spawnSafetyMinEnemyDistance = 35;
	float spawnSafetyEnemyViewDistance = 120;
	float spawnSafetyEnemyViewAngleDegrees = 70;
	string teamAssignmentMode = "balancedRandom";
	bool enforceTDMTeamOutfits = true;
	string tdmRedJacket = "TrackSuitJacket_Red";
	string tdmRedPants = "TrackSuitPants_Red";
	string tdmRedShoes = "JoggingShoes_Red";
	string tdmRedMask = "Bandana_RedPattern";
	string tdmRedArmband = "Armband_Red";
	string tdmBlueJacket = "TrackSuitJacket_Blue";
	string tdmBluePants = "TrackSuitPants_Blue";
	string tdmBlueShoes = "JoggingShoes_Blue";
	string tdmBlueMask = "Bandana_Blue";
	string tdmBlueArmband = "Armband_Blue";
	ref TStringArray arenaRotation = new TStringArray();
	ref TStringArray excludedArenas = new TStringArray();
	ref TStringArray admins = new TStringArray();
	ref TStringArray teamNames = new TStringArray();
	ref array<ref DAFDMTeamAssignmentConfig> preassignedTeams = new array<ref DAFDMTeamAssignmentConfig>();
	ref array<ref DAFDMRoundTypeConfig> roundTypes = new array<ref DAFDMRoundTypeConfig>();
	ref array<ref DAFDMWeaponConfig> primaryWeapons = new array<ref DAFDMWeaponConfig>();
	ref array<ref DAFDMWeaponConfig> secondaryWeapons = new array<ref DAFDMWeaponConfig>();

	void DAFDMSettings()
	{
		s_Settings = this;
	}

	static DAFDMSettings Instance()
	{
		return s_Settings;
	}

	void Load()
	{
		if (!FileExist(DAFDMFilenames.DIRECTORY))
			MakeDirectory(DAFDMFilenames.DIRECTORY);

		if (FileExist(DAFDMFilenames.SETTINGS_JSON))
		{
			JsonFileLoader<ref DAFDMSettings>.JsonLoadFile(DAFDMFilenames.SETTINGS_JSON, this);
		}
		else
		{
			FillDefaults();
			JsonFileLoader<ref DAFDMSettings>.JsonSaveFile(DAFDMFilenames.SETTINGS_JSON, this);
		}

		if (roundMinutes < 1)
			roundMinutes = 20;

		if (respawnSeconds < 0)
			respawnSeconds = 3;

		if (roundTransitionSeconds < 1)
			roundTransitionSeconds = 10;

		if (corpseCleanupSeconds < 0)
			corpseCleanupSeconds = 5;

		if (deathDropCleanupSeconds < 1)
			deathDropCleanupSeconds = 60;

		if (spawnSafetyMinPlayerDistance < 0)
			spawnSafetyMinPlayerDistance = 0;

		if (spawnSafetyMinEnemyDistance < 0)
			spawnSafetyMinEnemyDistance = 0;

		if (spawnSafetyEnemyViewDistance < 0)
			spawnSafetyEnemyViewDistance = 0;

		if (spawnSafetyEnemyViewAngleDegrees < 1)
			spawnSafetyEnemyViewAngleDegrees = 70;

		if (arenaWallSegments < 8)
			arenaWallSegments = 8;

		if (commandCharacter.Length() == 0)
			commandCharacter = "@";
		commandCharacter = commandCharacter.Substring(0, 1);

		if (roundTypes.Count() == 0)
			FillDefaultRoundTypes();

		if (teamNames.Count() < 2)
			FillDefaultTeams();

		if (primaryWeapons.Count() == 0 || secondaryWeapons.Count() == 0)
			FillDefaultWeapons();

		PrintFormat("DAFDeathmatch: settings loaded. roundMinutes=%1 respawnSeconds=%2 autoRespawn=%3", roundMinutes, respawnSeconds, autoRespawn);
	}

	bool IsAdmin(PlayerIdentity identity)
	{
		if (!identity)
			return false;

		return admins.Find(identity.GetId()) >= 0;
	}

	private void FillDefaults()
	{
		arenaRotation.Insert("default");
		FillDefaultRoundTypes();
		FillDefaultTeams();
		FillDefaultWeapons();
	}

	private void FillDefaultRoundTypes()
	{
		roundTypes.Clear();
		AddRoundType("normal", "Normal", 70, "normal");
		AddRoundType("snipers", "Snipers", 10, "snipers");
		AddRoundType("freshies", "Freshies", 10, "freshies");
		AddRoundType("juiced", "Juiced", 10, "juiced");
	}

	private void AddRoundType(string name, string displayName, int weight, string loadoutPool)
	{
		DAFDMRoundTypeConfig roundType = new DAFDMRoundTypeConfig();
		roundType.name = name;
		roundType.displayName = displayName;
		roundType.weight = weight;
		roundType.loadoutPool = loadoutPool;
		roundTypes.Insert(roundType);
	}

	private void FillDefaultTeams()
	{
		teamNames.Clear();
		teamNames.Insert("red");
		teamNames.Insert("blue");
	}

	private void FillDefaultWeapons()
	{
		primaryWeapons.Clear();
		secondaryWeapons.Clear();

		DAFDMWeaponConfig primary = new DAFDMWeaponConfig();
		primary.weapon = "M4A1";
		primary.attachments.Insert("M4_RISHndgrd");
		primary.attachments.Insert("M4_MPBttstck");
		primary.attachments.Insert("ACOGOptic");
		primary.magazines.Insert("Mag_STANAG_30Rnd");
		primary.magazines.Insert("Mag_STANAG_30Rnd");
		primaryWeapons.Insert(primary);

		DAFDMWeaponConfig secondary = new DAFDMWeaponConfig();
		secondary.weapon = "Glock19";
		secondary.magazines.Insert("Mag_Glock_15Rnd");
		secondary.magazines.Insert("Mag_Glock_15Rnd");
		secondaryWeapons.Insert(secondary);
	}
}
