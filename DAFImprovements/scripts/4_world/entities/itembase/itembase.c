modded class ItemBase
{
	private float m_DAF_PreHitGlobalHealth;
	private ref array<string> m_DAF_PreHitZones;
	private ref array<float> m_DAF_PreHitZoneHealth;

	protected bool DAF_ShouldProtectCarriedItemDamage()
	{
		if (!GetGame() || !GetGame().IsServer())
			return false;

		PlayerBase player = PlayerBase.Cast(GetHierarchyRootPlayer());
		return player && player.IsAlive();
	}

	protected void DAF_SnapshotOwnDamage()
	{
		m_DAF_PreHitGlobalHealth = GetHealth("", "Health");
		m_DAF_PreHitZones = new array<string>();
		m_DAF_PreHitZoneHealth = new array<float>();

		GetDamageZones(m_DAF_PreHitZones);
		foreach (string zone: m_DAF_PreHitZones)
		{
			m_DAF_PreHitZoneHealth.Insert(GetHealth(zone, "Health"));
		}
	}

	void DAF_RestoreOwnDamage()
	{
		if (!m_DAF_PreHitZones || !DAF_ShouldProtectCarriedItemDamage())
			return;

		SetHealth("", "Health", m_DAF_PreHitGlobalHealth);
		for (int i = 0; i < m_DAF_PreHitZones.Count(); i++)
		{
			SetHealth(m_DAF_PreHitZones[i], "Health", m_DAF_PreHitZoneHealth[i]);
		}
	}

	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		bool protect = DAF_ShouldProtectCarriedItemDamage();
		if (protect)
			DAF_SnapshotOwnDamage();

		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);

		if (protect)
		{
			DAF_RestoreOwnDamage();
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.DAF_RestoreOwnDamage, 100, false);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.DAF_RestoreOwnDamage, 500, false);
		}
	}
}
