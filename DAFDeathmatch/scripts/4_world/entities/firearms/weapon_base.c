class DAFWeaponFireModeHelper
{
	private static bool s_ForcePreferredFireMode;

	static void SetForcePreferredFireMode(bool enabled)
	{
		s_ForcePreferredFireMode = enabled;
	}

	static void SetPreferredFireMode(Weapon_Base weapon)
	{
		if (!s_ForcePreferredFireMode)
			return;

		ApplyPreferredFireMode(weapon);
	}

	static void ApplyPreferredFireMode(Weapon_Base weapon)
	{
		if (!CanSetPreferredFireMode(weapon))
			return;

		bool changed = false;

		for (int muzzle = 0; muzzle < weapon.GetMuzzleCount(); muzzle++)
		{
			int originalMode = weapon.GetCurrentMode(muzzle);
			int modeCount = weapon.GetMuzzleModeCount(muzzle);
			int preferredMode = -1;
			int preferredBurstSize = 1;

			for (int mode = 0; mode < modeCount; mode++)
			{
				weapon.SetCurrentMode(muzzle, mode);

				if (weapon.GetCurrentModeAutoFire(muzzle))
				{
					preferredMode = mode;
					break;
				}

				int burstSize = weapon.GetCurrentModeBurstSize(muzzle);
				if (burstSize > preferredBurstSize)
				{
					preferredMode = mode;
					preferredBurstSize = burstSize;
				}
			}

			int targetMode = originalMode;
			if (preferredMode != -1)
				targetMode = preferredMode;

			if (weapon.GetCurrentMode(muzzle) != targetMode)
				weapon.SetCurrentMode(muzzle, targetMode);

			if (originalMode != targetMode)
				changed = true;
		}

		if (changed)
			weapon.Synchronize();
	}

	private static bool CanSetPreferredFireMode(Weapon_Base weapon)
	{
		if (!weapon)
			return false;

		return !weapon.IsKindOf("B95");
	}
}

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
