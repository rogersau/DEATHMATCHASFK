////////////////////////////////////////////////////////////////////
//DeRap: config.bin
//Produced from mikero's Dos Tools Dll version 9.98
//https://mikero.bytex.digital/Downloads
//'now' is Fri Aug 01 13:44:16 2025 : 'file' last modified on Wed Sep 20 05:39:34 2023
////////////////////////////////////////////////////////////////////

#define _ARMA_

class CfgPatches
{
	class PeppersKillfeed
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data"};
	};
};
class CfgMods
{
	class PeppersKillfeed
	{
		dir = "PeppersKillfeed";
		picture = "";
		action = "";
		hideName = 1;
		hidePicture = 1;
		name = "PeppersKillfeed";
		credits = "";
		author = "";
		authorID = "0";
		version = "1.0";
		extra = 0;
		type = "mod";
		dependencies[] = {"Game","World","Mission"};
		class defs
		{
			class worldScriptModule
			{
				value = "";
				files[] = {"PeppersKillfeed/scripts/4_World"};
			};
		};
	};
};
