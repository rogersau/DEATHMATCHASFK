modded class MissionServer
{
	override void OnRoundStart(DMArena arena)
	{
		super.OnRoundStart(arena);

		DAFServerLowPopAmmoRefill.Start();
		DAFServerRoundTimer.Start(DAFServer_GetRoundMinutes());
		DAFServer_ResetRoundStats("MissionServer.OnRoundStart");
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.DAFServer_CheckInfectedCleanup, 1000, false, "MissionServer.OnRoundStart");
	}

	override void OnRoundEnd(DMArena arena)
	{
		super.OnRoundEnd(arena);

		DAFServerRoundTimer.Stop();
		DAFServer_ResetRoundStats("MissionServer.OnRoundEnd");
	}

	override void DMRespawnPlayer(PlayerBase player)
	{
		PlayerIdentity identity;
		if (player)
			identity = player.GetIdentity();

		super.DMRespawnPlayer(player);

		if (identity)
		{
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.DAFServer_SendRespawnFix, 750, false, identity);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.DAFServer_PostRespawnSetup, 750, false, identity);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.DAFServer_PostRespawnSetup, 1500, false, identity);
		}
	}

	override void EquipCharacter(MenuDefaultCharacterData char_data)
	{
		super.EquipCharacter(char_data);

		DAFServer_NormalizeSurvivalQuickbar(m_player);
		DAFServerLowPopAmmoRefill.RefillPlayerMagazines(m_player);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.DAFServer_NormalizeSurvivalQuickbar, 500, false, m_player);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.DAFServer_NormalizeSurvivalQuickbar, 1500, false, m_player);
	}

	override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
	{
		super.InvokeOnConnect(player, identity);

		DAFServerLowPopAmmoRefill.Start();
		DAFServerRoundTimer.Broadcast();
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.DAFServer_CheckInfectedCleanup, 1000, false, "MissionServer.InvokeOnConnect");
	}

	int DAFServer_GetRoundMinutes()
	{
		DeathmatchSettings settings = DeathmatchSettings.Instance();
		if (!settings)
			return 0;

		return settings.roundMinutes;
	}

	void DAFServer_CheckInfectedCleanup(string source)
	{
		DAFServerInfectedCleanup.CleanupIfNeeded(source);
	}

	void DAFServer_SendRespawnFix(PlayerIdentity identity)
	{
		if (!identity)
			return;

		PlayerBase player = PlayerBase.Cast(identity.GetPlayer());
		if (player)
			player.RPCSingleParam(-74700007, new Param1<bool>(true), true, identity);
	}

	void DAFServer_PostRespawnSetup(PlayerIdentity identity)
	{
		if (!identity)
			return;

		PlayerBase player = PlayerBase.Cast(identity.GetPlayer());
		if (!player)
			return;

		DAFServer_NormalizeSurvivalQuickbar(player);
		DAFServerLowPopAmmoRefill.Start();
		DAFServerLowPopAmmoRefill.RefillPlayerMagazines(player);
	}

	void DAFServer_NormalizeSurvivalQuickbar(PlayerBase player)
	{
		if (!player)
			return;

		EntityAI bandage = DAFServer_FindInventoryItem(player, "BandageDressing");
		EntityAI morphine = DAFServer_FindInventoryItem(player, "Morphine");
		EntityAI saline = DAFServer_FindInventoryItem(player, "SalineBagIV");

		if (bandage)
			player.SetQuickBarEntityShortcut(bandage, 2, true);

		if (morphine)
			player.SetQuickBarEntityShortcut(morphine, 3, true);

		if (saline)
			player.SetQuickBarEntityShortcut(saline, 4, true);

		DAFServer_DeleteInventoryItems(player, "EasterEgg");
	}

	EntityAI DAFServer_FindInventoryItem(PlayerBase player, string type)
	{
		array<EntityAI> items = new array<EntityAI>();
		player.GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);

		foreach (EntityAI item: items)
		{
			if (item && item.IsKindOf(type))
				return item;
		}

		EntityAI inHands = player.GetHumanInventory().GetEntityInHands();
		if (inHands && inHands.IsKindOf(type))
			return inHands;

		return null;
	}

	void DAFServer_DeleteInventoryItems(PlayerBase player, string type)
	{
		array<EntityAI> items = new array<EntityAI>();
		player.GetInventory().EnumerateInventory(InventoryTraversalType.LEVELORDER, items);

		foreach (EntityAI item: items)
		{
			if (item && item.IsKindOf(type))
				item.Delete();
		}

		EntityAI inHands = player.GetHumanInventory().GetEntityInHands();
		if (inHands && inHands.IsKindOf(type))
			inHands.Delete();
	}
}
