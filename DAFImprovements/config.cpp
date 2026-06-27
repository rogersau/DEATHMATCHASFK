////////////////////////////////////////////////////////////////////
//DeRap: config.bin
//Produced from mikero's Dos Tools Dll version 9.98
//https://mikero.bytex.digital/Downloads
//'now' is Fri Aug 01 13:44:16 2025 : 'file' last modified on Wed Sep 20 05:39:34 2023
////////////////////////////////////////////////////////////////////

#define _ARMA_

class CfgPatches
{
	class DAFImprovements
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data"};
	};
};
class CfgMods
{
	class DAFImprovements
	{
		dir = "DAFImprovements";
		picture = "";
		action = "";
		hideName = 1;
		hidePicture = 1;
		name = "DAF Improvements";
		credits = "";
		author = "";
		authorID = "0";
		version = "1.0";
		extra = 0;
		type = "mod";
		dependencies[] = {"Game","World","Mission"};
		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"DAFImprovements/scripts/3_Game"};
			};
			class worldScriptModule
			{
				value = "";
				files[] = {"DAFImprovements/scripts/4_World"};
			};
		};
	};
};
