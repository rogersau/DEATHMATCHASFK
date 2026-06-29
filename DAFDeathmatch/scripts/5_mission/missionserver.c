modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();

		if (!g_DAFDeathmatch)
		{
			g_DAFDeathmatch = new DAFDeathmatch();
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(g_DAFDeathmatch.Start, 5000, false);
		}
	}

	override void OnEvent(EventType eventTypeId, Param params)
	{
		super.OnEvent(eventTypeId, params);

		if (g_DAFDeathmatch)
			g_DAFDeathmatch.OnEvent(eventTypeId, params);
	}

	override PlayerBase CreateCharacter(PlayerIdentity identity, vector pos, ParamsReadContext ctx, string characterName)
	{
		if (g_DAFDeathmatch)
			pos = g_DAFDeathmatch.GetPlayerSpawnPosition(identity);

		m_player = PlayerBase.Cast(GetGame().CreateObject(characterName, pos));
		GetGame().SelectPlayer(identity, m_player);
		return m_player;
	}

	override void EquipCharacter(MenuDefaultCharacterData char_data)
	{
		if (g_DAFDeathmatch)
			g_DAFDeathmatch.StartingEquipSetup(m_player);
		else
			StartingEquipSetup(m_player, true);
	}

	override PlayerBase OnClientNewEvent(PlayerIdentity identity, vector pos, ParamsReadContext ctx)
	{
		if (g_DAFDeathmatch)
			g_DAFDeathmatch.ResetConnectedPlayer(identity);

		PlayerBase oldPlayer;
		if (identity)
			oldPlayer = PlayerBase.Cast(identity.GetPlayer());

		if (oldPlayer && oldPlayer.IsAlive())
		{
			PrintFormat("DAFDeathmatch: deleting old alive player for reconnect %1", identity.GetId());
			GetGame().ObjectDelete(oldPlayer);
		}

		string characterType = GetGame().CreateRandomPlayer();
		if (CreateCharacter(identity, pos, ctx, characterType))
			EquipCharacter(new MenuDefaultCharacterData());

		return m_player;
	}

	override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
	{
		super.InvokeOnConnect(player, identity);

		if (g_DAFDeathmatch)
		{
			g_DAFDeathmatch.OnPlayerConnected(player);
		}
	}

	override void InvokeOnDisconnect(PlayerBase player)
	{
		super.InvokeOnDisconnect(player);

		if (g_DAFDeathmatch)
			g_DAFDeathmatch.OnPlayerDisconnected(player);
	}
}
