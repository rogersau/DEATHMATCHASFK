class DAFRoundStats
{
	private static ref TStringIntMap s_RoundKills;
	private static ref TStringIntMap s_RoundDeaths;

	private static void EnsureMaps()
	{
		if (!s_RoundKills)
			s_RoundKills = new TStringIntMap();

		if (!s_RoundDeaths)
			s_RoundDeaths = new TStringIntMap();
	}

	static void Reset(string source = "")
	{
		EnsureMaps();
		s_RoundKills.Clear();
		s_RoundDeaths.Clear();

		if (source != "")
			Print("DAFImprovements: resetting round K:D stats from " + source);
		else
			Print("DAFImprovements: resetting round K:D stats");
	}

	static void Ensure(PlayerIdentity identity)
	{
		if (!identity)
			return;

		EnsureMaps();

		string id = identity.GetId();
		if (!s_RoundKills.Contains(id))
			s_RoundKills.Set(id, 0);

		if (!s_RoundDeaths.Contains(id))
			s_RoundDeaths.Set(id, 0);
	}

	static void AddKill(PlayerIdentity identity)
	{
		if (!identity)
			return;

		Ensure(identity);
		string id = identity.GetId();
		s_RoundKills.Set(id, s_RoundKills.Get(id) + 1);
	}

	static void AddDeath(PlayerIdentity identity)
	{
		if (!identity)
			return;

		Ensure(identity);
		string id = identity.GetId();
		s_RoundDeaths.Set(id, s_RoundDeaths.Get(id) + 1);
	}

	static int GetKills(PlayerIdentity identity)
	{
		if (!identity)
			return 0;

		Ensure(identity);
		return s_RoundKills.Get(identity.GetId());
	}

	static int GetDeaths(PlayerIdentity identity)
	{
		if (!identity)
			return 0;

		Ensure(identity);
		return s_RoundDeaths.Get(identity.GetId());
	}
}
