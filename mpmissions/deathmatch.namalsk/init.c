class CustomMission : MissionServer
{
	void CustomMission()
	{
	}
};

Mission CreateCustomMission(string path)
{
	return new CustomMission();
}

void main()
{
	CreateHive();
	GetHive().InitOffline();

	/*
		Namalsk Survival ties gameplay features to the stored mission date.
		Keep the old recommended November window available, but do not force it
		during local deathmatch smoke tests.
	*/
	/*int year, month, day, hour, minute;
	GetGame().GetWorld().GetDate(year, month, day, hour, minute);

	if ((month < 11) || (month >= 12))
	{
		year = 2011;
		month = 11;
		day = 1;

		GetGame().GetWorld().SetDate(year, month, day, hour, minute);
	}*/
};
