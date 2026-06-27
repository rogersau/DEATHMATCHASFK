modded class ActionGiveSalineSelfCB
{
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousTime(8.0);
	}
}

modded class ActionGiveSalineTargetCB
{
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousTime(8.0);
	}
}

modded class ActionGiveSalineSelf
{
	override void OnFinishProgressServer(ActionData action_data)
	{
		if (action_data.m_Player && action_data.m_Player.GetModifiersManager())
			action_data.m_Player.GetModifiersManager().ActivateModifier(eModifiers.MDF_SALINE);

		if (action_data.m_Player)
			action_data.m_Player.DAF_FullHealFromSaline();

		if (action_data.m_MainItem)
			action_data.m_MainItem.Delete();
	}
}

modded class ActionGiveSalineTarget
{
	override void OnFinishProgressServer(ActionData action_data)
	{
		PlayerBase target = PlayerBase.Cast(action_data.m_Target.GetObject());
		if (target && target.GetModifiersManager())
			target.GetModifiersManager().ActivateModifier(eModifiers.MDF_SALINE);

		if (target)
			target.DAF_FullHealFromSaline();

		if (action_data.m_MainItem)
			action_data.m_MainItem.Delete();
	}
}
