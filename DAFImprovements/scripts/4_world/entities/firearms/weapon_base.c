modded class Weapon_Base
{
	override bool IsJammed()
	{
		return false;
	}

	override void SetJammed(bool value)
	{
		super.SetJammed(false);
	}

	override float GetSyncChanceToJam()
	{
		return 0.0;
	}

	override float GetChanceToJam()
	{
		return 0.0;
	}
}
