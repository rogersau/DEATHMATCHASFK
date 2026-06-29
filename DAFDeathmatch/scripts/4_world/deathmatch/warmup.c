/**
 * DAFDMLowPopWarmup owns the low-population warmup modifier.
 *
 * Extracted from the DAFDeathmatch god class. Previously four state fields
 * (m_WarmupActive, m_WarmupTickStarted, m_WarmupActivationDueTime,
 * m_WarmupInfected) and ~300 lines of warmup logic lived in the manager
 * beside death drops, loadouts, and arena walls.
 *
 * Warmup is a server-owned round modifier, not a separate round mode
 * (CONTEXT.md "Low-Pop Warmup"). Normal rounds keep running; this module
 * adds target-practice helpers (non-damaging infected, unlimited ammo) while
 * population is too low for real PvP.
 *
 * It owns its state and mutates the world (spawns/culls infected, refills
 * magazines). Round state it needs (active arena, round-active flag, player
 * count) is passed in per call so the module has no back-reference into the
 * manager's round lifecycle. The HUD broadcast is deliberately NOT owned
 * here: Evaluate/Tick return whether state changed, and the manager decides
 * whether to push a HUD update. That keeps the broadcast seam in the manager.
 *
 * Test dummies never count as real players for anchor selection or ammo
 * refill; their ids are passed in via ignorePlayerIds.
 */
class DAFDMLowPopWarmup
{
	private ref DAFDMSettings m_Settings;
	private bool m_Active = false;
	private bool m_TickStarted = false;
	private int m_ActivationDueTime = 0;
	private ref array<ZombieBase> m_Infected = new array<ZombieBase>();

	void DAFDMLowPopWarmup(DAFDMSettings settings)
	{
		m_Settings = settings;
	}

	/** Start the recurring evaluation tick. Call once at server start. */
	void StartTick(DAFDeathmatch manager)
	{
		if (m_TickStarted)
			return;

		m_TickStarted = true;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(manager.WarmupTick, m_Settings.warmupTickSeconds * 1000, true);
	}

	/**
	 * Recurring tick. Re-evaluates warmup state, then (if active and a round is
	 * running in an arena) maintains infected and refills ammo. Returns true
	 * when HUD-relevant state may have changed, so the caller can broadcast.
	 */
	bool Tick(int playerCount, bool roundActive, DAFDMArena arena, array<string> ignorePlayerIds, string source)
	{
		bool changed = Evaluate(playerCount, roundActive, arena, ignorePlayerIds, source);

		if (m_Active && roundActive && arena)
		{
			MaintainInfected(arena, ignorePlayerIds);
			RefillAmmo(ignorePlayerIds);
		}

		return changed;
	}

	/**
	 * Re-evaluate whether warmup should be active given the current player
	 * count. Activates immediately when crossing up out of low-pop is reversed;
	 * deactivates immediately when population rises above the threshold but only
	 * arms the activation timer (does not activate) when population drops.
	 * Returns true when active/pending state changed.
	 */
	bool Evaluate(int playerCount, bool roundActive, DAFDMArena arena, array<string> ignorePlayerIds, string source)
	{
		bool shouldWarmup = m_Settings.enableLowPopWarmup && playerCount > 0 && playerCount <= m_Settings.warmupPlayerThreshold;
		if (!shouldWarmup)
		{
			m_ActivationDueTime = 0;
			if (m_Active)
			{
				Deactivate(source, playerCount);
				return true;
			}

			return false;
		}

		if (m_Active)
			return false;

		int now = GetGame().GetTime();
		if (m_ActivationDueTime <= 0)
		{
			m_ActivationDueTime = now + (m_Settings.warmupActivateDelaySeconds * 1000);
			PrintFormat("DAFDeathmatch: low-pop warmup pending source=%1 players=%2 delay=%3s", source, playerCount, m_Settings.warmupActivateDelaySeconds);
			return true;
		}

		if (now >= m_ActivationDueTime)
		{
			Activate(source, playerCount, arena, ignorePlayerIds);
			return true;
		}

		return false;
	}

	bool IsActive()
	{
		return m_Active;
	}

	bool IsPending()
	{
		return !m_Active && m_ActivationDueTime > 0;
	}

	bool ShouldBlockInfectedDamage(EntityAI source)
	{
		return m_Active && source && source.IsInherited(DayZInfected);
	}

	string GetStatusString()
	{
		if (m_Active)
			return "Warmup Mode";

		return "";
	}

	int GetActiveInfectedCount()
	{
		CullInfected();
		return m_Infected.Count();
	}

	/** Delete all warmup infected. Called at round end/cleanup. */
	void Cleanup()
	{
		for (int i = m_Infected.Count() - 1; i >= 0; i--)
		{
			ZombieBase infected = m_Infected[i];
			if (infected && !infected.ToDelete())
				GetGame().ObjectDelete(infected);
		}

		m_Infected.Clear();
	}

	private void Activate(string source, int playerCount, DAFDMArena arena, array<string> ignorePlayerIds)
	{
		if (m_Active)
			return;

		m_Active = true;
		m_ActivationDueTime = 0;
		PrintFormat("DAFDeathmatch: low-pop warmup activated source=%1 players=%2 threshold=%3", source, playerCount, m_Settings.warmupPlayerThreshold);
		MaintainInfected(arena, ignorePlayerIds);
		RefillAmmo(ignorePlayerIds);
	}

	private void Deactivate(string source, int playerCount)
	{
		if (!m_Active)
			return;

		m_Active = false;
		m_ActivationDueTime = 0;
		Cleanup();
		PrintFormat("DAFDeathmatch: low-pop warmup deactivated source=%1 players=%2 threshold=%3", source, playerCount, m_Settings.warmupPlayerThreshold);
	}

	private void MaintainInfected(DAFDMArena arena, array<string> ignorePlayerIds)
	{
		CullInfected();

		if (m_Settings.warmupInfectedTargetCount <= 0)
			return;

		while (m_Infected.Count() < m_Settings.warmupInfectedTargetCount)
		{
			if (!SpawnInfected(arena, ignorePlayerIds))
				return;
		}
	}

	private void CullInfected()
	{
		for (int i = m_Infected.Count() - 1; i >= 0; i--)
		{
			ZombieBase infected = m_Infected[i];
			if (!infected || infected.ToDelete() || !infected.IsAlive())
			{
				if (infected && !infected.ToDelete())
					GetGame().ObjectDelete(infected);

				m_Infected.Remove(i);
			}
		}
	}

	private bool SpawnInfected(DAFDMArena arena, array<string> ignorePlayerIds)
	{
		vector position = PickInfectedPosition(arena, ignorePlayerIds);
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

		m_Infected.Insert(infected);
		ApplyInfectedMovement(infected);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.ApplyInfectedMovement, 500, false, infected);
		return true;
	}

	private void ApplyInfectedMovement(ZombieBase infected)
	{
		if (!infected || infected.ToDelete())
			return;

		DayZInfectedInputController controller = infected.GetInputController();
		if (controller)
			controller.OverrideMovementSpeed(true, m_Settings.warmupInfectedMovementSpeed);
	}

	private vector PickInfectedPosition(DAFDMArena arena, array<string> ignorePlayerIds)
	{
		vector anchor = GetAnchorPosition(arena, ignorePlayerIds);
		float angle = Math.RandomFloatInclusive(0, Math.PI * 2);
		float distance = Math.RandomFloatInclusive(m_Settings.warmupInfectedMinSpawnDistance, m_Settings.warmupInfectedMaxSpawnDistance);
		vector position = Vector(anchor[0] + (Math.Cos(angle) * distance), anchor[1], anchor[2] + (Math.Sin(angle) * distance));
		position = ClampPositionToArena(position, arena);
		return DAFDMArena.SnapToGround(position);
	}

	private vector GetAnchorPosition(DAFDMArena arena, array<string> ignorePlayerIds)
	{
		PlayerBase player = GetAnchorPlayer(ignorePlayerIds);
		if (player)
			return player.GetPosition();

		if (arena)
			return arena.GetCenter();

		return "0 0 0";
	}

	private PlayerBase GetAnchorPlayer(array<string> ignorePlayerIds)
	{
		array<Man> players = new array<Man>();
		GetGame().GetPlayers(players);

		foreach (Man man: players)
		{
			PlayerBase player = PlayerBase.Cast(man);
			if (player && player.GetIdentity() && player.IsAlive() && !IsIgnored(player, ignorePlayerIds))
				return player;
		}

		return null;
	}

	private vector ClampPositionToArena(vector position, DAFDMArena arena)
	{
		if (!arena)
			return position;

		vector center = arena.GetCenter();
		if (arena.IsRectangular())
		{
			float halfX = (arena.GetXSize() * 0.5) - 5;
			float halfZ = (arena.GetZSize() * 0.5) - 5;
			if (halfX > 1)
				position[0] = Math.Clamp(position[0], center[0] - halfX, center[0] + halfX);
			if (halfZ > 1)
				position[2] = Math.Clamp(position[2], center[2] - halfZ, center[2] + halfZ);

			return position;
		}

		float radius = arena.GetRadius() - 5;
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

	private void RefillAmmo(array<string> ignorePlayerIds)
	{
		if (!m_Settings.warmupUnlimitedAmmo)
			return;

		array<Man> players = new array<Man>();
		GetGame().GetPlayers(players);

		foreach (Man man: players)
		{
			PlayerBase player = PlayerBase.Cast(man);
			if (player && player.GetIdentity() && player.IsAlive() && !IsIgnored(player, ignorePlayerIds))
				RefillPlayerMagazines(player);
		}
	}

	private bool IsIgnored(PlayerBase player, array<string> ignorePlayerIds)
	{
		if (!player || !player.GetIdentity() || !ignorePlayerIds)
			return false;

		return ignorePlayerIds.Find(player.GetIdentity().GetId()) >= 0;
	}

	private void RefillPlayerMagazines(PlayerBase player)
	{
		bool changed = false;
		array<EntityAI> items = new array<EntityAI>();
		player.GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);

		EntityAI inHands = player.GetHumanInventory().GetEntityInHands();
		foreach (EntityAI item: items)
		{
			changed = RefillMagazine(item, inHands, false) || changed;
		}

		changed = RefillMagazine(inHands, inHands, false) || changed;
		if (changed)
			player.SetSynchDirty();
	}

	private bool RefillMagazine(EntityAI item, EntityAI inHands, bool allowWeaponMagazine)
	{
		bool changed = false;
		Weapon_Base weapon = Weapon_Base.Cast(item);
		if (weapon)
		{
			bool isHeldWeapon = item == inHands;
			for (int muzzle = 0; muzzle < weapon.GetMuzzleCount(); muzzle++)
			{
				if (!isHeldWeapon)
					changed = RefillMagazine(weapon.GetMagazine(muzzle), inHands, true) || changed;
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
}
