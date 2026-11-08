# Phylum
<p align="center"><img src="docs/images/phylum-logo.png" width="280" alt="Phylum logo"></p>

Phylum is a local server for Darkspore. Its goal is to bring back the closest one-to-one copy of the original game that can be made: the same combat, enemies, bosses, loot and progression, behaving the way they did when EA's servers were still running.

Phylum comes from ReCap, the [Resurrection Capsule](https://github.com/Resurrection-Capsule/Resurrection-Capsule) project. It started as a fork of [recap_server_cpp](https://github.com/Resurrection-Capsule/recap_server_cpp), and that repository's full commit history is kept here. Tested on the Steam Demo (5.3.0.103).

This is an unofficial community project, not affiliated with EA or Maxis. You need your own copy of Darkspore. Game scripts are extracted from your own install. Some data and images inherited from ReCap are still in this repository, and the aim is to remove them too.

## Where we come from

We fully respect all the work that has been done before us. ReCap's developers and community spent years keeping Darkspore alive: they rebuilt the login and network protocols, moved the server from Python to C++, mapped how the game used to work, and kept a community together after the official servers closed. Phylum would not exist without them. Their names stay in the credits below, and their history stays in this repository.

## Goal

The aim is a server that plays like the original, as close to one-to-one as the evidence allows. EA's server code is gone, so an exact copy cannot be promised, but every rule should come from the game itself wherever the game holds it. Where it does not, the rule is reconstructed and marked as approximate, so it can be replaced when better evidence turns up.

The first milestone is the campaign played alone from start to finish. Multiplayer comes after that.

## How we do it

- **The game's own scripts run unchanged.** Darkspore's abilities, effects, enemy conditions and objectives are compiled Lua scripts. Phylum runs that original bytecode on Lua 5.1, instead of decompiled or rewritten copies, so the logic inside them is the original logic.
- **C++ rebuilds the engine underneath.** The scripts call engine functions, such as dealing damage, starting an effect, finding targets or moving an object. Phylum's C++ server provides those functions, built as small classes with one job each. An adapter in `scriptapi.cpp` only translates between the scripts and the server.
- **Mapping the scripts.** Every game script is disassembled into one listing, so we can read exactly what a script does. A contract check compares every engine function the scripts call against what the server provides, and a gap log records any missing function a script reaches in play.
- **Reading the client.** Where the game client holds a rule the server needs, such as a packet layout, a data table or an engine function, we read it from `Darkspore.exe` and cite the address in the code and docs.
- **The game's data files.** Numbers come straight from the game: tuning tables, level configurations, class attributes and AI definitions.
- **Marking what is approximate.** Where nothing in the game settles a rule, it is reconstructed from gameplay videos and other community work, and recorded as approximate in [docs](./docs).
- **Checking every change.** Unit tests, a self-check that runs every ability script, modifier and projectile checks, and testing in the real game.

## Status

| Area | State |
| --- | --- |
| Login, registration, entering levels | Working |
| Levels in worlds 1 to 6 | Should load, only 1-1 has been tested |
| Levels 7-1 and above | Do not load, the server's level table stops at world 6 |
| Scripts | The game's original bytecode runs on Lua 5.1, with 452 ability scripts registered |
| Abilities | 238 of 278 pass the self-check, and basic attacks work in play |
| Effects | Modifiers are built, and 381 of 483 modifier scripts start in the check. The rest need other engine functions |
| Damage | Armour, resistances and damage types apply. Some details are approximate, see [docs/damage.md](./docs/damage.md) |
| Hero stats | Built as the client does, from class base, primary stat shares and the weapon's damage range |
| Cooldowns and mana | Come from the scripts |
| Enemies | Only one spawns per spawn group, and enemy AI is not built yet |

## What this project changes

- **Hero abilities:** read from the game's player classes, because `creature_templates.json` has them all set to 0.
- **Attack crash:** the game crashes on an unknown ability ID in an ability reply, so replies now carry valid IDs.
- **Stuck actions:** ability replies now echo the sequence number of the client's command, and send a real cast start time.
- **False level end:** squad abilities no longer tell the game the boss is dead.
- **Ability scripts:** the server runs the game's original compiled scripts on Lua 5.1, instead of decompiled copies on LuaJIT. An adapter in `scriptapi.cpp` connects them to server functions.
- **Effects:** a modifier system applies buffs, debuffs and passives, and sends them to the client. See [docs/modifiers.md](./docs/modifiers.md).
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
- The game can crash while loading a level, on about half of recent loads. The cause is unknown, and retrying usually works.
- Abilities that need functions the server does not have yet stop partway.

## Roadmap

The game's scripts hold the real combat logic, so the work is mostly finishing the API they call and then building the systems around them. The full checklist, risks and open decisions are in [PLAN.md](./PLAN.md).

| Stage | Done when | Progress |
| --- | --- | --- |
| 1. Combat foundation | Every hero's abilities run in play, effects apply and expire, and loading is reliable | In progress, 13 of 18 items done |
| 2. A level that fights back | 1-1 has its full enemy population, enemies fight back, and you can win or lose | Not started |
| 3. Bosses and finishing a level | A level can be completed through its boss, with objectives and medals tracked | Not started |
| 4. Campaign flow | A full run through all six worlds is possible in one session | Not started |
| 5. Rewards and progression | Playing earns gear and progress that persist between sessions | Not started |
| 6. Polish | Someone else can follow this README and finish the campaign | Not started |

## Contributing

- Run the game's own scripts and data. Do not rewrite game rules that the files already contain.
- `scriptapi.cpp` only translates. Real behaviour goes in C++, in small classes with one job.
- Every change passes `scripts/windows/verify.ps1`, and new logic comes with tests.
- Commits use the `feat(scope):` and `fix(scope):` style.
- Never add game files, including anything extracted from them.

For the project Phylum comes from, see the [ReCap repository](https://github.com/Resurrection-Capsule/Resurrection-Capsule) and its [Discord](https://discord.gg/btfTw62).

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

Phylum, like the Resurrection Capsule server it comes from, is free software: you can
redistribute it and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the License, or (at your
option) any later version.

The full text is in [LICENSE](./LICENSE). Third-party library licenses are in [LICENSES.md](./LICENSES.md).
