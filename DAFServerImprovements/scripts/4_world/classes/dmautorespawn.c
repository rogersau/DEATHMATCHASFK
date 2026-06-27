class DAFServerAutoRespawnSetting
{
	string Id;
	bool Enabled;

	void DAFServerAutoRespawnSetting(string id = "", bool enabled = false)
	{
		Id = id;
		Enabled = enabled;
	}
}

modded class DMAutoRespawn
{
	private static const string DAFServer_ProfileDir = "$profile:DAFImprovements";
	private static const string DAFServer_AutoRespawnFile = "$profile:DAFImprovements/DeathmatchAutoRespawn.json";
	private static ref array<ref DAFServerAutoRespawnSetting> DAFServer_Settings;
	private static bool DAFServer_Loaded;

	override void InitializeID(string id)
	{
		super.InitializeID(id);

		DAFServer_LoadAutoRespawnSettings();

		bool savedEnabled;
		if (DAFServer_FindAutoRespawnSetting(id, savedEnabled))
		{
			bool currentEnabled = super.IsEnabledForID(id);
			if (currentEnabled != savedEnabled)
				super.ToggleForID(id);
		}
	}

	override void ToggleForID(string id)
	{
		super.ToggleForID(id);

		DAFServer_SetAutoRespawnSetting(id, super.IsEnabledForID(id));
		DAFServer_SaveAutoRespawnSettings();
	}

	private void DAFServer_LoadAutoRespawnSettings()
	{
		if (DAFServer_Loaded)
			return;

		DAFServer_Loaded = true;
		DAFServer_Settings = new array<ref DAFServerAutoRespawnSetting>();

		if (!FileExist(DAFServer_ProfileDir))
			MakeDirectory(DAFServer_ProfileDir);

		if (FileExist(DAFServer_AutoRespawnFile))
			JsonFileLoader<ref array<ref DAFServerAutoRespawnSetting>>.JsonLoadFile(DAFServer_AutoRespawnFile, DAFServer_Settings);
	}

	private void DAFServer_SaveAutoRespawnSettings()
	{
		if (!FileExist(DAFServer_ProfileDir))
			MakeDirectory(DAFServer_ProfileDir);

		JsonFileLoader<ref array<ref DAFServerAutoRespawnSetting>>.JsonSaveFile(DAFServer_AutoRespawnFile, DAFServer_Settings);
	}

	private bool DAFServer_FindAutoRespawnSetting(string id, out bool enabled)
	{
		foreach (DAFServerAutoRespawnSetting setting : DAFServer_Settings)
		{
			if (setting && setting.Id == id)
			{
				enabled = setting.Enabled;
				return true;
			}
		}

		return false;
	}

	private void DAFServer_SetAutoRespawnSetting(string id, bool enabled)
	{
		foreach (DAFServerAutoRespawnSetting setting : DAFServer_Settings)
		{
			if (setting && setting.Id == id)
			{
				setting.Enabled = enabled;
				return;
			}
		}

		DAFServer_Settings.Insert(new DAFServerAutoRespawnSetting(id, enabled));
	}
}
