# Crimson Zamboni Local Test

Use this when testing the current Crimson Zamboni compatibility stack, not standalone `DAFDeathmatch`.

## Start Server

```powershell
.\Start-CrimsonNamalskTest.ps1 -Repack
```

The server loads:

```text
-mod=@NamalskIsland;@NamalskSurvival;@DAFImprovements
-serverMod=@CrimsonZamboniDeathmatch;@DAFServerImprovements
```

It does not load `@DAFDeathmatch`.

The local launcher writes `build\server-test\serverDZ.crimson-namalsk.cfg` with `BattlEye = 0;` so local client testing does not need a BattleEye restart.

To prepare files without launching the server:

```powershell
.\Start-CrimsonNamalskTest.ps1 -Repack -NoStart
```

## Start Client

```powershell
.\Start-CrimsonNamalskClient.ps1 -NoBattleEye
```

The client loads only the client-visible Crimson stack mods:

```text
-mod=@NamalskIsland;@NamalskSurvival;@DAFImprovements
```

It does not load `@DAFServerImprovements`, `@CrimsonZamboniDeathmatch`, or `@DAFDeathmatch`.
