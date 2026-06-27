modded class PlayerBase
{
	override void EEKilled(Object killer)
	{
		bool wasHeadshot = DAF_WasLastHitHeadshot();

		EntityAI killerEntity = EntityAI.Cast(killer);
		PlayerBase attacker;
		if (killerEntity)
			attacker = PlayerBase.Cast(killerEntity.GetHierarchyRootPlayer());

		if (attacker && attacker != this && attacker.GetIdentity() && GetIdentity())
		{
			string weaponName = "Unknown";
			if (killerEntity)
			{
				weaponName = killerEntity.GetDisplayName();
				if (weaponName == "")
					weaponName = killerEntity.GetType();
			}

			int distance = vector.Distance(GetPosition(), attacker.GetPosition());
			DAFServerDiscordStats.RegisterKill(
				attacker.GetIdentity().GetName(),
				GetIdentity().GetName(),
				weaponName,
				distance,
				wasHeadshot);
		}

		super.EEKilled(killer);
	}
}
