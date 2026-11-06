# Resurrection Capsule Server (combat fork)

A fork of the [Resurrection Capsule](https://github.com/Resurrection-Capsule/Resurrection-Capsule) (ReCap) server, a local server that lets you play Darkspore offline. This fork focuses on making combat work, tested on the Steam Demo (5.3.0.103).

This is an unofficial community project. It is not affiliated with EA or Maxis, and it contains no game files. You need your own copy of Darkspore.

## Status

| Area | State |
| --- | --- |
| Login, registration, entering levels | Working |
| Levels in worlds 1 to 6 | Should load, only 1-1 has been tested |
| Levels 7-1 and above | Do not load, the server's level table stops at world 6 |
| Basic attacks | Swing, land on time and kill enemies |
| Other abilities | Accepted by the game, but have no effect yet |
| Damage | Random roll in the hero's weapon range, with no armour or resistances |
| Enemies | Only one spawns per spawn group |

## What this fork changes

- **Hero abilities:** read from the game's player classes, because `creature_templates.json` has them all set to 0.
- **Attack crash:** the game crashes on an unknown ability ID in an ability reply, so replies now carry valid IDs.
- **Stuck actions:** ability replies now echo the sequence number of the client's command, and send a real cast start time.
- **False level end:** squad abilities no longer tell the game the boss is dead.
- **Basic attacks:** swings and hit timing are read from the game's own ability scripts.
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
- The game crashed once while loading 1-1. The cause is unknown.
- Basic attacks have no range check.
- Other abilities have no animation or effect.

## Roadmap

The game's ability scripts hold the real combat logic, but they call a Lua API this server does not have yet. The main goal is to build that API so the scripts run as they are, starting with `template_ability_melee`.

## Contributing

Commits use the `feat(scope):` and `fix(scope):` style. Never commit game files, including anything extracted from them.

For the wider project, see the [ReCap repository](https://github.com/Resurrection-Capsule/Resurrection-Capsule) and [Discord](https://discord.gg/btfTw62).

## Credits
 * dalkon, for migrating the project from Python to C++, and developing the majority of the ReCap server code from 2019 to 2022
 * Splitwirez, for being our main consultant regarding the way Darkspore used to work, our designer, and the developer of the Resurrection Capsule Hub
 * r0ptr, for fixing the CMake build process, allowing us to continue the project after a huge hiatus, and for helping with coding in general
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
