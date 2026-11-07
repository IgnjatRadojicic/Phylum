# Resurrection Capsule: Original Lua + C++ Server

An independent continuation of the [Resurrection Capsule](https://github.com/Resurrection-Capsule/Resurrection-Capsule) (ReCap) server, a local server that lets you play Darkspore offline. It runs the game's original compiled Lua scripts on a C++ server, and focuses on making combat work. Tested on the Steam Demo (5.3.0.103). Based on [recap_server_cpp](https://github.com/Resurrection-Capsule/recap_server_cpp), with its full commit history kept.

This is an unofficial community project, not affiliated with EA or Maxis. You need your own copy of Darkspore. Game scripts are extracted from your own install. Some data and images inherited from upstream ReCap are still in this repository, and the aim is to remove them too.

## Status

| Area | State |
| --- | --- |
| Login, registration, entering levels | Working |
| Levels in worlds 1 to 6 | Should load, only 1-1 has been tested |
| Levels 7-1 and above | Do not load, the server's level table stops at world 6 |
| Abilities | Run the game's own scripts. About 60% of scripted abilities pass the self-check, and basic attacks work in play |
| Effects | Buffs, debuffs and shields have no effect yet, because modifiers are not built |
| Damage | Random roll in the hero's weapon range, with no armour or resistances |
| Enemies | Only one spawns per spawn group |

## What this project changes

- **Hero abilities:** read from the game's player classes, because `creature_templates.json` has them all set to 0.
- **Attack crash:** the game crashes on an unknown ability ID in an ability reply, so replies now carry valid IDs.
- **Stuck actions:** ability replies now echo the sequence number of the client's command, and send a real cast start time.
- **False level end:** squad abilities no longer tell the game the boss is dead.
- **Ability scripts:** the server runs the game's original compiled scripts on Lua 5.1, instead of decompiled copies on LuaJIT. An adapter in `scriptapi.cpp` connects them to server functions.
- **Script check:** `recap_server --check-scripts` loads every script and tests each ability against fake objects, with no game needed.
- **Build:** repaired the dead RakNet and LuaJIT sources, and added Windows scripts that need no Visual Studio install.

## Getting started on Windows

1. Install Darkspore. The Steam Demo works.
2. Download the [latest ReCap release](https://github.com/Resurrection-Capsule/Resurrection-Capsule/releases/latest) and extract it into the game's `DarksporeBin` folder.
3. Build this server and install it into the game:
   ```powershell
   cd scripts\windows
   .\setup-tools.ps1 -AcceptMsvcLicense
   .\build.ps1 -GameBin "C:\Program Files (x86)\Steam\steamapps\common\Darkspore\DarksporeBin"
   ```
   The first command accepts the Visual Studio license and downloads about 3 GB of tools into `.tools`. Use `-ToolsDir` to put them somewhere else, and pass the same folder to `build.ps1`. See [BUILD.md](./BUILD.md) for more.
4. Start `RecapLauncher.exe` from inside `DarksporeBin`. Starting it from anywhere else fails with error 267, so use a shortcut with "Start in" set to that folder. Keep the server and game open while playing.
5. Wait for **Singleplayer** to enable. The first launch extracts files from your game, which takes a few minutes.
6. Register a local account. Use a throwaway name, email and password, because the game's protocols are not secure.

### Two things to set up

- **Web port:** `SERVER_HTTP_PORT` in `Server\config.xml` must match `port` in `RecapHooks.toml`. This server defaults to 8081, and the ReCap release expects 80. Change either one so that they match.
- **Starting level:** new accounts start at 7-1, which does not load. Close the server, then edit your account in `Server\storage\user\` and set:
  ```xml
  <tutorial_completed>Y</tutorial_completed>
  <chain_progression>0</chain_progression>
  ```

## Known issues

- The game can crash on the vote screen after a level.
- The game can crash while loading a level. The cause is unknown, and retrying usually works.
- Abilities that need modifiers or other missing functions stop partway.

## Roadmap

The game's scripts hold the real combat logic. The main goal is to finish the API they call, starting with modifiers, so that buffs and debuffs work.

## Contributing

Commits use the `feat(scope):` and `fix(scope):` style. Never add game files, including anything extracted from them.

For the wider project, see the [ReCap repository](https://github.com/Resurrection-Capsule/Resurrection-Capsule) and [Discord](https://discord.gg/btfTw62).

## Credits
 * dalkon, for migrating the project from Python to C++, and developing the majority of the ReCap server code from 2019 to 2022
 * Splitwirez, for being our main consultant regarding the way Darkspore used to work, our designer, and the developer of the Resurrection Capsule Hub
 * r0ptr, for fixing the CMake build process, allowing us to continue the project after a huge hiatus, and for helping with coding in general
 * VitorMM (vitor251093), JeanxPereira and benjaminhottell, for their work on the upstream server
 * All the members from our Discord server that helped testing and developing the server!

## Special Thanks

* Emd (Discord)
* haradons (Discord)
* nonchip (Github)
* Otomachi Una (Discord)
* Snek (Discord)

## Licenses

[![GNU GPLv3 Image](https://www.gnu.org/graphics/gplv3-127x51.png)](http://www.gnu.org/licenses/gpl-3.0.en.html)

Resurrection Capsule server is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
