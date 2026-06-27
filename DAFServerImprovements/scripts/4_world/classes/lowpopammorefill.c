class DAFServerLowPopAmmoRefill
{
	private static const int REFILL_INTERVAL_MS = 1000;
	private static bool s_Started;

	static void Start()
	{
		if (s_Started || !GetGame() || !GetGame().IsServer())
			return;

		s_Started = true;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DAFServerLowPopAmmoRefill.RefillTick, REFILL_INTERVAL_MS, true);
	}

	static bool ShouldRefillForCurrentPlayerCount()
	{
		DeathmatchSettings settings = DeathmatchSettings.Instance();
		if (!settings || settings.forceInfectedPlayerLimit <= 0)
			return false;

		return DAFServerInfectedCleanup.GetPlayerCount() < settings.forceInfectedPlayerLimit;
	}

	static void RefillTick()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;

		if (!ShouldRefillForCurrentPlayerCount())
			return;

		array<Man> players = new array<Man>();
		GetGame().GetPlayers(players);

		foreach (Man man: players)
		{
			PlayerBase player = PlayerBase.Cast(man);
			if (player)
				RefillPlayerMagazines(player);
		}
	}

	static void RefillPlayerMagazines(PlayerBase player)
	{
		if (!player || !player.IsAlive())
			return;

		bool changed = false;
		array<EntityAI> items = new array<EntityAI>();
		player.GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);

		foreach (EntityAI item: items)
		{
			changed = RefillMagazine(item) || changed;
		}

		changed = RefillMagazine(player.GetHumanInventory().GetEntityInHands()) || changed;
		if (changed)
			player.SetSynchDirty();
	}

	private static bool RefillMagazine(EntityAI item)
	{
		bool changed = false;
		Weapon_Base weapon = Weapon_Base.Cast(item);
		if (weapon)
		{
			for (int muzzle = 0; muzzle < weapon.GetMuzzleCount(); muzzle++)
			{
				changed = RefillMagazine(weapon.GetMagazine(muzzle)) || changed;
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

		if (magazine.GetAmmoCount() < magazine.GetAmmoMax())
		{
			magazine.ServerSetAmmoMax();
			magazine.SetSynchDirty();
			changed = true;
		}

		return changed;
	}
}
