/**
 * DAFDMDeathFlow owns the death-drop and corpse-cleanup lifecycle described in
 * CONTEXT.md ("Death Flow").
 *
 * Previously this logic was smeared across five methods on the DAFDeathmatch
 * god class plus the DAFDMDeathDrop helper tacked on at the bottom of manager.c.
 * This module concentrates it behind one interface:
 *
 *   - OnKilled(victim)         -> drops the in-hands weapon, strips the corpse,
 *                                 schedules corpse cleanup, schedules TTL expiry.
 *   - OnItemPickedUp(player,item) -> grants the one-mag pickup bonus when a
 *                                 different player recovers a tracked drop.
 *   - TrackDeathDrop(...)      -> shared with the @testdrop admin command.
 *   - CleanupAll()             -> round-end purge of drops and pending corpses.
 *
 * It owns its two state collections (m_DeathDrops, m_PendingCorpseCleanup) and
 * has no back-reference into the manager. The TTLs it needs are passed in at
 * construction so the module does not depend on DAFDMSettings directly.
 */
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

class DAFDMDeathFlow
{
	private ref array<ref DAFDMDeathDrop> m_DeathDrops = new array<ref DAFDMDeathDrop>();
	private ref array<PlayerBase> m_PendingCorpseCleanup = new array<PlayerBase>();
	private int m_CorpseCleanupSeconds;
	private int m_DeathDropCleanupSeconds;

	void DAFDMDeathFlow(int corpseCleanupSeconds, int deathDropCleanupSeconds)
	{
		m_CorpseCleanupSeconds = corpseCleanupSeconds;
		m_DeathDropCleanupSeconds = deathDropCleanupSeconds;
	}

	/**
	 * Full death-side handling for a victim: drop the in-hands weapon (preserving
	 * attachments), delete the rest of the corpse inventory, and schedule both
	 * corpse cleanup and death-drop TTL expiry. Returns the dropped weapon, or
	 * null if no weapon was in hands.
	 *
	 * Only the weapon in hands drops. If no weapon is in hands, nothing drops.
	 * The original owner can recover their weapon, but gets no mag bonus.
	 */
	EntityAI OnKilled(PlayerBase victim)
	{
		if (!victim)
			return null;

		EntityAI deathDrop = HandleDeathDrop(victim);
		DeleteCorpseInventory(victim, deathDrop);

		if (m_PendingCorpseCleanup.Find(victim) < 0)
			m_PendingCorpseCleanup.Insert(victim);

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.CleanupCorpse, m_CorpseCleanupSeconds * 1000, false, victim);
		return deathDrop;
	}

	/** Grant the one-mag pickup bonus if another player recovers a tracked drop. */
	void OnItemPickedUp(PlayerBase player, EntityAI item)
	{
		if (!player || !item)
			return;

		for (int i = m_DeathDrops.Count() - 1; i >= 0; i--)
		{
			DAFDMDeathDrop drop = m_DeathDrops[i];
			if (!drop || drop.weapon != item)
				continue;

			HandleDeathDropPickup(player, drop);
			m_DeathDrops.Remove(i);
			return;
		}
	}

	/** Track a death drop directly. Shared with the @testdrop admin command. */
	void TrackDeathDrop(EntityAI weapon, string ownerId, string magazineType)
	{
		if (!weapon)
			return;

		m_DeathDrops.Insert(new DAFDMDeathDrop(weapon, ownerId, magazineType));
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.DeleteDeathDrop, m_DeathDropCleanupSeconds * 1000, false, weapon);
		PrintFormat("DAFDeathmatch: tracked death drop weapon=%1 owner=%2 bonus=%3 ttl=%4s", weapon.GetType(), ownerId, magazineType, m_DeathDropCleanupSeconds);
	}

	/** Round-end purge: remove pending corpses from tracking and delete all drops. */
	void CleanupAll()
	{
		m_PendingCorpseCleanup.Clear();

		for (int i = m_DeathDrops.Count() - 1; i >= 0; i--)
		{
			DAFDMDeathDrop drop = m_DeathDrops[i];
			if (drop && drop.weapon)
				GetGame().ObjectDelete(drop.weapon);
		}

		m_DeathDrops.Clear();
	}

	int GetActiveDeathDropCount()
	{
		int count = 0;
		foreach (DAFDMDeathDrop drop: m_DeathDrops)
		{
			if (drop && drop.weapon)
				count++;
		}

		return count;
	}

	int GetPendingCorpseCleanupCount()
	{
		int count = 0;
		foreach (PlayerBase corpse: m_PendingCorpseCleanup)
		{
			if (corpse)
				count++;
		}

		return count;
	}

	private EntityAI HandleDeathDrop(PlayerBase victim)
	{
		EntityAI inHands = victim.GetHumanInventory().GetEntityInHands();
		Weapon_Base weapon = Weapon_Base.Cast(inHands);
		if (!weapon)
			return null;

		string ownerId = "";
		if (victim.GetIdentity())
			ownerId = victim.GetIdentity().GetId();

		victim.GetHumanInventory().DropEntity(InventoryMode.SERVER, victim, weapon);
		TrackDeathDrop(weapon, ownerId, GetPrimaryMagazineType(weapon));
		return weapon;
	}

	private void HandleDeathDropPickup(PlayerBase player, DAFDMDeathDrop drop)
	{
		if (!player || !drop)
			return;

		string pickerId = "";
		if (player.GetIdentity())
			pickerId = player.GetIdentity().GetId();

		bool grantBonus = pickerId != "" && pickerId != drop.ownerId && drop.magazineType != "";
		if (grantBonus)
		{
			EntityAI bonus = player.GetHumanInventory().CreateInInventory(drop.magazineType);
			if (bonus)
				PrintFormat("DAFDeathmatch: death drop picked up by %1; granted bonus %2", pickerId, drop.magazineType);
			else
				PrintFormat("DAFDeathmatch: death drop picked up by %1; failed to grant bonus %2", pickerId, drop.magazineType);
		}
		else
		{
			PrintFormat("DAFDeathmatch: death drop picked up by %1; no bonus granted", pickerId);
		}
	}

	private string GetPrimaryMagazineType(Weapon_Base weapon)
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

	private void DeleteCorpseInventory(PlayerBase victim, EntityAI deathDrop)
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

	private void DeleteDeathDrop(EntityAI weapon)
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
		{
			GetGame().ObjectDelete(weapon);
			Print("DAFDeathmatch: cleaned expired death drop");
		}
	}

	private void CleanupCorpse(PlayerBase player)
	{
		if (player && m_PendingCorpseCleanup.Find(player) >= 0)
			m_PendingCorpseCleanup.RemoveItem(player);

		if (player && !player.IsAlive())
		{
			GetGame().ObjectDelete(player);
			Print("DAFDeathmatch: cleaned pending corpse");
		}
	}
}
