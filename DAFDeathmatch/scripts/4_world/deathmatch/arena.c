class DAFDMPropConfig
{
	string type;
	ref array<float> position = new array<float>();
	ref array<float> orientation = new array<float>();
}

class DAFDMArenaConfig
{
	string name;
	ref array<float> center = new array<float>();
	float radius = 250;
	bool rectangular = false;
	float xSize = 0;
	float zSize = 0;
	int minimumPlayers = 0;
	int maximumPlayers = 0;
	ref array<ref array<float>> playerSpawns = new array<ref array<float>>();
	bool disableWalls = false;
	ref array<ref DAFDMPropConfig> props = new array<ref DAFDMPropConfig>();
}

class DAFDMProp
{
	string type;
	vector position;
	vector orientation;

	void DAFDMProp(string typeName, vector pos, vector orient)
	{
		type = typeName;
		position = pos;
		orientation = orient;
	}

	Object Spawn()
	{
		return GetGame().CreateObjectEx(type, position, ECE_PLACE_ON_SURFACE);
	}
}

class DAFDMArena
{
	string m_Name;
	vector m_Center;
	float m_Radius;
	bool m_Rectangular;
	float m_XSize;
	float m_ZSize;
	ref array<vector> m_PlayerSpawns;
	bool m_DisableWalls;
	ref array<ref DAFDMProp> m_Props;

	void DAFDMArena(string name, vector center, float radius, bool rectangular = false, float xSize = 0, float zSize = 0)
	{
		m_Name = name;
		m_Center = center;
		m_Radius = radius;
		m_Rectangular = rectangular;
		m_XSize = xSize;
		m_ZSize = zSize;
		m_PlayerSpawns = new array<vector>();
		m_DisableWalls = false;
		m_Props = new array<ref DAFDMProp>();
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

	bool IsRectangular()
	{
		return m_Rectangular;
	}

	float GetXSize()
	{
		return m_XSize;
	}

	float GetZSize()
	{
		return m_ZSize;
	}

	vector GetRandomPlayerSpawn()
	{
		if (m_PlayerSpawns.Count() == 0)
			return SnapToGround(m_Center);

		return SnapToGround(m_PlayerSpawns.GetRandomElement());
	}

	array<vector> GetPlayerSpawns()
	{
		return m_PlayerSpawns;
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

	void DisableWalls()
	{
		m_DisableWalls = true;
	}

	bool IsWallsDisabled()
	{
		return m_DisableWalls;
	}

	void AddProp(string type, vector position, vector orientation)
	{
		m_Props.Insert(new DAFDMProp(type, position, orientation));
	}

	void Prepare(array<Object> objects)
	{
		foreach (DAFDMProp prop : m_Props)
		{
			Object obj = prop.Spawn();
			if (obj)
			{
				obj.SetOrientation(prop.orientation);
				objects.Insert(obj);
			}
		}
	}

	Object PlaceWall(vector position, vector orientation, string wallType)
	{
		position[1] = GetGame().SurfaceY(position[0], position[2]);

		int flags = ECE_PLACE_ON_SURFACE;
		float depth = GetGame().GetWaterDepth(position);
		if (depth > 1)
		{
			position[1] = position[1] + depth - 1;
			flags |= ECE_KEEPHEIGHT;
		}

		Object obj = GetGame().CreateObjectEx(wallType, position, flags);
		if (obj)
			obj.SetOrientation(orientation);

		return obj;
	}

	void EncloseCircle(array<Object> enclosure, string wallType)
	{
		float step = 360 / ((2 * Math.PI * m_Radius) / 20);
		float angle = 0;
		while (angle < 360)
		{
			vector vec = Vector(angle, 0, 0).AnglesToVector();
			vector position = m_Center + (vec * m_Radius);
			vector orientation = Vector(angle + 180, 0, 0);

			Object wall = PlaceWall(position, orientation, wallType);
			if (wall)
				enclosure.Insert(wall);

			angle += step;
		}
	}

	void EncloseRectangle(array<Object> enclosure, string wallType)
	{
		float x = m_Center[0] - (m_XSize / 2);

		while (x <= m_Center[0] + (m_XSize / 2))
		{
			Object wall1 = PlaceWall(Vector(x, 0, m_Center[2] - (m_ZSize / 2)), "0 0 0", wallType);
			if (wall1)
				enclosure.Insert(wall1);

			Object wall2 = PlaceWall(Vector(x, 0, m_Center[2] + (m_ZSize / 2)), "180 0 0", wallType);
			if (wall2)
				enclosure.Insert(wall2);

			x += 20;
		}

		float z = m_Center[2] - (m_ZSize / 2);
		while (z <= m_Center[2] + (m_ZSize / 2))
		{
			Object wall3 = PlaceWall(Vector(m_Center[0] - (m_XSize / 2), 0, z), "90 0 0", wallType);
			if (wall3)
				enclosure.Insert(wall3);

			Object wall4 = PlaceWall(Vector(m_Center[0] + (m_XSize / 2), 0, z), "270 0 0", wallType);
			if (wall4)
				enclosure.Insert(wall4);

			z += 20;
		}
	}

	void Enclose(array<Object> enclosure, string wallType)
	{
		if (m_DisableWalls)
			return;

		if (m_Rectangular)
			EncloseRectangle(enclosure, wallType);
		else
			EncloseCircle(enclosure, wallType);
	}

	static DAFDMArena FromConfig(DAFDMArenaConfig config)
	{
		float radius = config.radius;
		if (radius <= 0)
			radius = Math.Sqrt((config.xSize * config.xSize) + (config.zSize * config.zSize)) * 0.5;

		DAFDMArena arena = new DAFDMArena(config.name, VectorFromArray(config.center), radius, config.rectangular, config.xSize, config.zSize);

		foreach (array<float> spawn: config.playerSpawns)
		{
			arena.m_PlayerSpawns.Insert(VectorFromArray(spawn));
		}

		if (config.disableWalls)
			arena.DisableWalls();

		foreach (DAFDMPropConfig prop : config.props)
		{
			if (prop && prop.type != "")
				arena.AddProp(prop.type, VectorFromArray(prop.position), VectorFromArray(prop.orientation));
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
			array<ref DAFDMArena> rotation = new array<ref DAFDMArena>();
			foreach (string arenaName: settings.arenaRotation)
			{
				DAFDMArena arena = GetByName(arenaName);
				if (arena && !IsExcluded(settings, arena.GetName()))
					rotation.Insert(arena);
			}

			if (rotation.Count() > 0)
				return rotation.GetRandomElement();
		}

		array<ref DAFDMArena> allowed = new array<ref DAFDMArena>();
		foreach (DAFDMArena candidate: m_Arenas)
		{
			if (candidate && (!settings || !IsExcluded(settings, candidate.GetName())))
				allowed.Insert(candidate);
		}

		if (allowed.Count() > 0)
			return allowed.GetRandomElement();

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
				if (allowedArena && (!settings || !IsExcluded(settings, allowedArena.GetName())))
					allowed.Insert(allowedArena);
			}

			if (allowed.Count() > 0)
				return allowed.GetRandomElement();
		}

		return PickArena(settings);
	}

	DAFDMArena GetByName(string name)
	{
		string requested = name;
		requested.ToLower();
		foreach (DAFDMArena arena: m_Arenas)
		{
			if (!arena)
				continue;

			string candidate = arena.GetName();
			candidate.ToLower();
			if (candidate == requested)
				return arena;
		}

		return null;
	}

	int Count()
	{
		return m_Arenas.Count();
	}

	array<ref DAFDMArena> GetAll()
	{
		return m_Arenas;
	}

	bool IsExcluded(DAFDMSettings settings, string arenaName)
	{
		return settings && settings.excludedArenas && settings.excludedArenas.Find(arenaName) >= 0;
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
