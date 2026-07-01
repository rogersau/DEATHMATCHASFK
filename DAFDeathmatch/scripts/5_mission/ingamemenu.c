modded class InGameMenu
{
	private ref DAFDMLeaderboardMenu m_DAFDM_Leaderboard;

	override Widget Init()
	{
		Widget root = super.Init();
		m_DAFDM_Leaderboard = new DAFDMLeaderboardMenu(root);
		return root;
	}

	void ~InGameMenu()
	{
		if (m_DAFDM_Leaderboard)
		{
			m_DAFDM_Leaderboard.Destroy();
			m_DAFDM_Leaderboard = null;
		}
	}

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
