class CfgPatches
{
	class DAFServerImprovements
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DAFImprovements","CrimsonZamboniDeathmatch"};
	};
};
class CfgMods
{
	class DAFServerImprovements
	{
		dir = "DAFServerImprovements";
		hideName = 1;
		hidePicture = 1;
		name = "DAF Server Improvements";
		type = "mod";
		dependencies[] = {"Game","World","Mission"};
		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"DAFServerImprovements/scripts/3_Game"};
			};
			class worldScriptModule
			{
				value = "";
				files[] = {"DAFServerImprovements/scripts/4_World"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"DAFServerImprovements/scripts/5_Mission"};
			};
		};
	};
};
