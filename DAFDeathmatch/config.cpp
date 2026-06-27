class CfgPatches
{
	class DAFDeathmatch
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DAFImprovements"};
	};
};
class CfgMods
{
	class DAFDeathmatch
	{
		dir = "DAFDeathmatch";
		hideName = 1;
		hidePicture = 1;
		name = "DAF Deathmatch";
		type = "mod";
		dependencies[] = {"Game","World","Mission"};
		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"DAFDeathmatch/scripts/3_Game"};
			};
			class worldScriptModule
			{
				value = "";
				files[] = {"DAFDeathmatch/scripts/4_World"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"DAFDeathmatch/scripts/5_Mission"};
			};
		};
	};
};
