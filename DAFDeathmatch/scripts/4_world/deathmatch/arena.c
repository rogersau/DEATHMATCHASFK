class DAFDMArenaConfig
{
	string name;
	ref array<float> center = new array<float>();
	float radius = 250;
	ref array<ref array<float>> playerSpawns = new array<ref array<float>>();
}

class DAFDMArena
{
	string m_Name;
	vector m_Center;
	float m_Radius;
	ref array<vector> m_PlayerSpawns;

	void DAFDMArena(string name, vector center, float radius)
	{
		m_Name = name;
		m_Center = center;
		m_Radius = radius;
		m_PlayerSpawns = new array<vector>();
	}

	string GetName()
	{
		return m_Name;
	}

	vector GetCenter()
	{
		return m_Center;
	}

	float GetRadius()
	{
		return m_Radius;
	}

	vector GetRandomPlayerSpawn()
	{
		if (m_PlayerSpawns.Count() == 0)
			return SnapToGround(m_Center);

		return SnapToGround(m_PlayerSpawns.GetRandomElement());
	}

	void FaceCenter(PlayerBase player)
	{
		if (!player)
			return;

		player.SetDirection(vector.Direction(player.GetPosition(), m_Center));
		player.SetPosition(player.GetPosition() + "0 0.1 0");
	}

	static vector SnapToGround(vector position)
	{
		position[1] = GetGame().SurfaceY(position[0], position[2]);
		return position;
	}

	static DAFDMArena FromConfig(DAFDMArenaConfig config)
	{
		DAFDMArena arena = new DAFDMArena(config.name, VectorFromArray(config.center), config.radius);

		foreach (array<float> spawn: config.playerSpawns)
		{
			arena.m_PlayerSpawns.Insert(VectorFromArray(spawn));
		}

		return arena;
	}

	static vector VectorFromArray(array<float> values)
	{
		if (!values || values.Count() < 3)
			return "0 0 0";

		return Vector(values[0], values[1], values[2]);
	}
}

class DAFDMArenaRegistry
{
	private ref array<ref DAFDMArena> m_Arenas = new array<ref DAFDMArena>();

	void Load()
	{
		array<ref DAFDMArenaConfig> configs = new array<ref DAFDMArenaConfig>();

		if (!FileExist(DAFDMFilenames.DIRECTORY))
			MakeDirectory(DAFDMFilenames.DIRECTORY);

		if (FileExist(DAFDMFilenames.ARENAS_JSON))
		{
			JsonFileLoader<ref array<ref DAFDMArenaConfig>>.JsonLoadFile(DAFDMFilenames.ARENAS_JSON, configs);
		}
		else
		{
			FillDefaultArenaConfigs(configs);
			JsonFileLoader<ref array<ref DAFDMArenaConfig>>.JsonSaveFile(DAFDMFilenames.ARENAS_JSON, configs);
		}

		m_Arenas.Clear();
		foreach (DAFDMArenaConfig config: configs)
		{
			if (config && config.name != "")
				m_Arenas.Insert(DAFDMArena.FromConfig(config));
		}

		PrintFormat("DAFDeathmatch: loaded %1 arenas", m_Arenas.Count());
	}

	DAFDMArena PickArena(DAFDMSettings settings)
	{
		if (m_Arenas.Count() == 0)
			return null;

		if (settings)
		{
			foreach (string arenaName: settings.arenaRotation)
			{
				DAFDMArena arena = GetByName(arenaName);
				if (arena)
					return arena;
			}
		}

		return m_Arenas.GetRandomElement();
	}

	DAFDMArena PickArenaForRound(DAFDMSettings settings, DAFDMRoundTypeConfig roundType)
	{
		if (m_Arenas.Count() == 0)
			return null;

		if (roundType && roundType.arenaNames.Count() > 0)
		{
			array<ref DAFDMArena> allowed = new array<ref DAFDMArena>();
			foreach (string allowedName: roundType.arenaNames)
			{
				DAFDMArena allowedArena = GetByName(allowedName);
				if (allowedArena)
					allowed.Insert(allowedArena);
			}

			if (allowed.Count() > 0)
				return allowed.GetRandomElement();
		}

		return PickArena(settings);
	}

	DAFDMArena GetByName(string name)
	{
		foreach (DAFDMArena arena: m_Arenas)
		{
			if (arena && arena.GetName() == name)
				return arena;
		}

		return null;
	}

	private void FillDefaultArenaConfigs(array<ref DAFDMArenaConfig> configs)
	{
		DAFDMArenaConfig config = new DAFDMArenaConfig();
		config.name = "default";
		config.center.Insert(7500);
		config.center.Insert(0);
		config.center.Insert(7500);
		config.radius = 250;

		array<float> spawn = new array<float>();
		spawn.Insert(7500);
		spawn.Insert(0);
		spawn.Insert(7500);
		config.playerSpawns.Insert(spawn);

		configs.Insert(config);
	}
}
