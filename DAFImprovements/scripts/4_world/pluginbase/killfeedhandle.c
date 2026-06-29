class KillFeedHandle : PluginBase
{
	private ref array<ref KillFeedDeathType> KillFeedPhrases;
	private ref TStringIntMap RoundKills;
	private ref TStringIntMap RoundDeaths;

	void KillFeedHandle()
	{
		RoundKills = new TStringIntMap();
		RoundDeaths = new TStringIntMap();
		LoadPhrases();

		if (GetGame().IsServer())
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(this.SendPlayerCount, 30000, true);
	}

	void LoadPhrases()
	{
		string modDir = "$profile:DAFImprovements";
		string configPath = modDir + "//" + "Settings.json"; 
		bool changed = false;
		if (!FileExist(modDir))
			MakeDirectory(modDir);
		if (FileExist(configPath))
			JsonFileLoader<ref array<ref KillFeedDeathType>>.JsonLoadFile( configPath, KillFeedPhrases );

		if (!KillFeedPhrases || KillFeedPhrases.Count() == 0)
		{
			FillDefault();
			changed = true;
		}
		else
		{
			changed = EnsureRequiredPhrases();
		}

		if (changed)
			JsonFileLoader<ref array<ref KillFeedDeathType>>.JsonSaveFile( configPath, KillFeedPhrases );
	}

	void OnPlayerKilled(int deathType, PlayerBase victim = null, PlayerBase murder = null, EntityAI weapon = null, bool isHeadshot = false)
	{
		KillFeedDeathType ktype = GetDeathConfigByType(deathType);
		string msg;
		int type = 0;
		if (!ktype)
			ktype = GetDeathConfigByType(DeathType.UNKNOWN);

		if (!ktype)
			return;

		if (ktype.GetType() == "PVP")
			type = 1;
		else if (ktype.IsActive())
			msg = ktype.GetPhrase();
		else
			return;

		SendKillInfo(murder, victim, type, weapon, msg, isHeadshot);	
	}

	void SendKillInfo(PlayerBase murder, PlayerBase victim, int type, EntityAI weapon = null, string msg = "", bool isHeadshot = false)
	{
		string murderName, targetName, murderWeaponData, message;
		int dst, headshot, murderKills, murderDeaths, targetKills, targetDeaths;

		if (!victim)
			return;

		targetName = "Survivor (AI)";
		if (victim.GetIdentity())
			targetName = victim.GetIdentity().GetName();

		if (type)
		{
			if (!murder || !murder.GetIdentity())
				return;

			murderName = murder.GetIdentity().GetName();

			dst = vector.Distance(victim.GetPosition(), murder.GetPosition());
			murderWeaponData = string.Empty;
			if (weapon)
				murderWeaponData = GetWeaponData(weapon);

			AddRoundKill(murder.GetIdentity());
			AddRoundDeath(victim.GetIdentity());
			murderKills = GetRoundKills(murder.GetIdentity());
			murderDeaths = GetRoundDeaths(murder.GetIdentity());
			targetKills = GetRoundKills(victim.GetIdentity());
			targetDeaths = GetRoundDeaths(victim.GetIdentity());
			murderName = FormatRoundScoreName(murderName, murderKills, murderDeaths);
			targetName = FormatRoundScoreName(targetName, targetKills, targetDeaths);
			
			if (isHeadshot)
				headshot = 1;
			else
				headshot = 0;
		}
		else
		{
			message = msg;
			headshot = 0;
		}

		DAFRPC.SendKillfeedItem(murderName, targetName, murderWeaponData, dst, message, type, headshot);

		SendPlayerCount();
	}

	int GetPlayerCount()
	{
		array<PlayerIdentity> identities = new array<PlayerIdentity>();
		GetGame().GetPlayerIndentities(identities);
		return identities.Count();
	}

	void SendPlayerCount()
	{
		DAFRPC.SendPlayerCount(GetPlayerCount());
	}

	string FormatRoundScoreName(string name, int kills, int deaths)
	{
		return string.Format("%1 (%2:%3)", name, kills, deaths);
	}

	void ResetRoundStats()
	{
		DAFRoundStats.Reset("KillFeedHandle.ResetRoundStats");
		SendRoundStatsReset();
	}

	void SendRoundStatsReset()
	{
		DAFRPC.SendKillfeedReset();
	}

	void EnsureRoundStats(PlayerIdentity identity)
	{
		if (!identity)
			return;

		DAFRoundStats.Ensure(identity);
	}

	void AddRoundKill(PlayerIdentity identity)
	{
		if (!identity)
			return;

		DAFRoundStats.AddKill(identity);
	}

	void AddRoundDeath(PlayerIdentity identity)
	{
		if (!identity)
			return;

		DAFRoundStats.AddDeath(identity);
	}

	int GetRoundKills(PlayerIdentity identity)
	{
		if (!identity)
			return 0;

		return DAFRoundStats.GetKills(identity);
	}

	int GetRoundDeaths(PlayerIdentity identity)
	{
		if (!identity)
			return 0;

		return DAFRoundStats.GetDeaths(identity);
	}

	string GetWeaponData(EntityAI weapon)
	{
		array<string> parts = new array<string>();
		AddWeaponPart(parts, weapon, 0);

		string data = string.Empty;
		for (int i = 0; i < parts.Count(); i++)
		{
			if (i > 0)
				data = data + ",";

			data = data + parts.Get(i);
		}

		return data;
	}

	void AddWeaponPart(array<string> parts, EntityAI item, int depth)
	{
		if (!item)
			return;

		parts.Insert(depth.ToString() + ":ATT:" + item.GetType());

		int attachmentCount = item.GetInventory().AttachmentCount();
		for (int i = 0; i < attachmentCount; i++)
		{
			EntityAI attachment = item.GetInventory().GetAttachmentFromIndex(i);
			AddWeaponPart(parts, attachment, depth + 1);
		}

		AddWeaponMagazines(parts, item, depth + 1);
	}

	void AddWeaponMagazines(array<string> parts, EntityAI item, int depth)
	{
		Weapon_Base weapon = Weapon_Base.Cast(item);
		if (!weapon)
			return;

		for (int i = 0; i < weapon.GetMuzzleCount(); i++)
		{
			Magazine magazine = weapon.GetMagazine(i);
			if (magazine)
				parts.Insert(depth.ToString() + ":MAG:" + magazine.GetType());
		}
	}

	string GetDate()
	{
		int hour = 0;
		int minute = 0;
		int second = 0;
		int year = 0;
		int month = 0;
		int day = 0;

		GetYearMonthDay(year, month, day);
		GetHourMinuteSecond(hour, minute, second);
		return string.Format("%1.%2.%3 %4:%5:%6", day.ToString(), month.ToString(), year.ToString(), hour.ToString(), minute.ToString(), second.ToString());
	}

	void FillDefault()
	{
		KillFeedPhrases = new array<ref KillFeedDeathType>();
		KillFeedPhrases.Insert(new KillFeedDeathType("UNKNOWN", {"Died of an unknown cause"}));
		KillFeedPhrases.Insert(new KillFeedDeathType("PVP", {""}));
		KillFeedPhrases.Insert(new KillFeedDeathType("SUICIDE", {"Suicide"}));
		KillFeedPhrases.Insert(new KillFeedDeathType("BLEEDING", {"Bled to death"}));
		KillFeedPhrases.Insert(new KillFeedDeathType("STARVING", {"Died of exhaustion"}));
		KillFeedPhrases.Insert(new KillFeedDeathType("ZOMBIE", {"Eaten by zombies"}));
		KillFeedPhrases.Insert(new KillFeedDeathType("ANIMAL", {"Eaten by an animal"}));
		KillFeedPhrases.Insert(new KillFeedDeathType("FALL", {"Died of high altitude"}));
	}	

	bool EnsureRequiredPhrases()
	{
		bool changed = false;

		changed = EnsurePhrase("UNKNOWN", {"Died of an unknown cause"}) || changed;
		changed = EnsurePhrase("PVP", {""}) || changed;
		changed = EnsurePhrase("SUICIDE", {"Suicide"}) || changed;
		changed = EnsurePhrase("BLEEDING", {"Bled to death"}) || changed;
		changed = EnsurePhrase("STARVING", {"Died of exhaustion"}) || changed;
		changed = EnsurePhrase("ZOMBIE", {"Eaten by zombies"}) || changed;
		changed = EnsurePhrase("ANIMAL", {"Eaten by an animal"}) || changed;
		changed = EnsurePhrase("FALL", {"Died of high altitude"}) || changed;

		return changed;
	}

	bool EnsurePhrase(string deathType, array<string> phrases)
	{
		foreach (KillFeedDeathType ktype : KillFeedPhrases)
		{
			if (ktype && ktype.GetType() == deathType)
				return false;
		}

		KillFeedPhrases.Insert(new KillFeedDeathType(deathType, phrases));
		return true;
	}

	KillFeedDeathType GetDeathConfigByType(DeathType type)
	{
		foreach (KillFeedDeathType ktype : KillFeedPhrases)
		{
			if (ktype && typename.EnumToString(DeathType, type) == ktype.GetType())
				return ktype;
		}
		return null;
	}
}

KillFeedHandle GetKillFeedHandle()
{
	return KillFeedHandle.Cast(GetPlugin(KillFeedHandle));
}
