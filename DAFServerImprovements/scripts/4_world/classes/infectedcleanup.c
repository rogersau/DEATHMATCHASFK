class DAFServerInfectedCleanup
{
	private static ref array<ZombieBase> s_Infected;

	private static void EnsureInfectedArray()
	{
		if (!s_Infected)
			s_Infected = new array<ZombieBase>();
	}

	static void Register(ZombieBase infected)
	{
		if (!infected || !GetGame() || !GetGame().IsServer())
			return;

		EnsureInfectedArray();
		if (s_Infected.Find(infected) == -1)
			s_Infected.Insert(infected);
	}

	static void Unregister(ZombieBase infected)
	{
		if (!s_Infected || !infected)
			return;

		s_Infected.RemoveItem(infected);
	}

	static int GetPlayerCount()
	{
		array<PlayerIdentity> identities = new array<PlayerIdentity>();
		GetGame().GetPlayerIndentities(identities);
		return identities.Count();
	}

	static bool ShouldCleanupForCurrentPlayerCount()
	{
		DeathmatchSettings settings = DeathmatchSettings.Instance();
		if (!settings || settings.forceInfectedPlayerLimit <= 0)
			return false;

		return GetPlayerCount() >= settings.forceInfectedPlayerLimit;
	}

	static void CleanupIfNeeded(string source)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;

		if (!ShouldCleanupForCurrentPlayerCount())
			return;

		DeleteAll(source);
	}

	static void DeleteAll(string source)
	{
		EnsureInfectedArray();

		int deleted = 0;
		for (int i = s_Infected.Count() - 1; i >= 0; i--)
		{
			ZombieBase infected = s_Infected[i];
			s_Infected.Remove(i);

			if (!infected || infected.ToDelete())
				continue;

			GetGame().ObjectDelete(infected);
			deleted++;
		}

		DMInfected.aliveCount = 0;
		PrintFormat("DAFServer: deleted %1 infected from %2 because player count reached forceInfectedPlayerLimit", deleted, source);
	}
}
