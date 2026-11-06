# Solo campaign plan

Goal: play the Darkspore campaign alone from start to finish, with working combat, enemies, bosses, loot and progression.

The result should play recognisably like the original. It cannot be exact, because EA's server code is gone. Where the game's files hold a rule, we use it. Where they do not, we reconstruct it from the client and from videos, and mark it as approximate.

Multiplayer, PvP and public hosting are out of scope.

## How we work

- Run the game's own scripts and data. Do not rewrite game rules that the files already contain.
- `scriptapi.cpp` only translates. Real behaviour goes in C++, in small classes with one job.
- Every change passes the unit tests, which `build.ps1` runs, and `check-scripts.ps1`.
- Add tests with new logic. The game is for judging look and feel, not routine checks.
- Name server crashes from dumps and the linker map. Do not guess.
- Never add game files to the repository.
- Commit small steps as `feat(scope):` or `fix(scope):`.

## Where we are

| Area | State |
| --- | --- |
| Original script bytecode on Lua 5.1 | Done |
| Script adapter | Working, some placeholders remain |
| Ability scripts registered | 451, with 23 failing |
| Abilities passing the self-check | 164 of 277 |
| Basic attacks in play | Working |
| Client crash while loading a level | Open, hits about half of recent loads |
| Client crash on the vote screen | Open |

## Stage 1: Combat foundation

Make every hero's abilities work.

- [ ] Log calls to functions the adapter does not have
- [ ] Find the cause of the load crash, starting with the data sent at login
- [ ] Build `ModifierManager`, proven first on one simple effect such as a slow
- [ ] Work out the `ModifierCreated`, `ModifierUpdated` and `ModifierDeleted` packets
- [ ] Implement `AddAttributeModifier` and the `nModifier` functions
- [ ] Reconstruct how attribute changes combine, using the client code and `magicnumbers`
- [ ] Fix projectile abilities (72 fail with nil arithmetic)
- [ ] Implement `CreateThreadForObject` and the remaining `nAbility` functions
- [ ] Work out `TransformVector`, and replace adapter placeholders such as `IsPet`
- [ ] Apply cooldowns and mana from scripts, instead of the fixed reply cooldown
- [ ] Apply armour, resistances and damage types in `TakeDamage`

Done when: every hero's abilities run in play, effects apply and expire, and loading is reliable.

## Stage 2: A level that fights back

Make one level, 1-1, play properly.

- [ ] Spawn every enemy from the level's marker sets, removing the temporary one per group limit
- [ ] Read how `aidefinition` and `phase` data drive enemies
- [ ] Build the enemy AI: aggro, movement and using abilities
- [ ] Use `directortuning` and `difficultytuning` for pacing and scaling
- [ ] Hero death, squad swapping and level failure
- [ ] Health obelisks and orb drops
- [ ] Replace placeholder enemy hit reactions

Done when: 1-1 has its full enemy population, enemies fight back, and you can win or lose.

## Stage 3: Bosses and finishing a level

- [ ] Run the `lua/objective_*` scripts, replacing placeholder objectives
- [ ] Remove the objective update sent on every tick
- [ ] Boss spawning, phases and boss obelisks
- [ ] Real level completion, ending the level from the game's rules
- [ ] Fix `template_ability_cone` dependents and the 23 failing scripts, or confirm the demo lacks their files

Done when: a level can be completed through its boss, with objectives and medals tracked.

## Stage 4: Campaign flow

- [ ] Fix the vote-screen crash
- [ ] Chain voting between levels, and moving to the next level
- [ ] Support all campaign levels in the level table
- [ ] Fix new accounts starting at 7-1
- [ ] Start new accounts properly, including the tutorial if the demo allows
- [ ] Handle levels beyond world 6, or remove them from the choice
- [ ] Test every level in worlds 1 to 6

Done when: a full run through all six worlds is possible in one session.

## Stage 5: Rewards and progression

- [ ] Read `lua/LootRules.lua` and work out the `lootpreferences` tables
- [ ] Generate items from `lootprefix`, `lootsuffix` and `lootrigblock`
- [ ] Item drops and pickup
- [ ] Real cash-out, replacing the fixed 50 DNA and medals
- [ ] Crystals from `crystaltuning`
- [ ] XP, levels and `unlockstuning`
- [ ] Save rewards to the account
- [ ] Finish hero editing and equipment

Done when: playing earns gear and progress that persist between sessions.

## Stage 6: Polish

- [ ] Balance against gameplay videos and player memory
- [ ] Test each hero through the campaign
- [ ] Decide whether the full 5.3.0.127 game is needed
- [ ] Remove the remaining upstream game data from this repository
- [ ] Update the setup guide for a first-time player

Done when: someone else can follow the README and finish the campaign.

## Risks

- **Enemy AI:** the largest unknown. It may need much of what EA's server did.
- **Packet formats:** a wrong field can crash the client, as the ability reply did.
- **Lost formulas:** drop rates look recoverable, but cash-out and stat maths may be approximate.
- **The demo:** it may lack some content, which would mean moving to the full game.
- **Unverified data:** several files here have only been identified by name so far.

## Open decisions

- Whether the fork should enable GitHub issues.
- Whether to send fixes to the ReCap project.
- When to move from the demo to the full game, if a legitimate copy can be found.
