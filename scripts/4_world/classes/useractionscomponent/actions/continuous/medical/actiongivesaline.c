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
		super.OnFinishProgressServer(action_data);

		if (action_data.m_Player)
			action_data.m_Player.DAF_FullHealFromSaline();
	}
}

modded class ActionGiveSalineTarget
{
	override void OnFinishProgressServer(ActionData action_data)
	{
		super.OnFinishProgressServer(action_data);

		PlayerBase target = PlayerBase.Cast(action_data.m_Target.GetObject());
		if (target)
			target.DAF_FullHealFromSaline();
	}
}
