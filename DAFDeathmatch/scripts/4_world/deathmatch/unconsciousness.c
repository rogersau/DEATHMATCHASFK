modded class UnconsciousnessMdfr
{
	protected bool DAFDM_IsUnconsciousnessEnabled()
	{
		DAFDMSettings settings = DAFDMSettings.Instance();
		if (!settings)
			return true;

		return settings.enableUnconsciousness;
	}

	override bool ActivateCondition(PlayerBase player)
	{
		if (!DAFDM_IsUnconsciousnessEnabled())
			return false;

		return super.ActivateCondition(player);
	}

	override bool DeactivateCondition(PlayerBase player)
	{
		if (!DAFDM_IsUnconsciousnessEnabled())
			return true;

		return super.DeactivateCondition(player);
	}
}
