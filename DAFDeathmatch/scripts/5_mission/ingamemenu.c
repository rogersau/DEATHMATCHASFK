modded class InGameMenu
{
	override void OnClick_Respawn()
	{
		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		if (player && !player.DAFDM_IsManualRespawnAllowed())
			return;

		super.OnClick_Respawn();
	}

	override void GameRespawn(bool random)
	{
		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		if (player && !player.DAFDM_IsManualRespawnAllowed())
			return;

		super.GameRespawn(random);
	}
}
