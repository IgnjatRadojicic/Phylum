# Ability costs

What a hero's cast costs in mana and cooldown, and where each rule comes from. The code is
`AbilityCost` (`abilitycost.h`, the rules), `CastPayment` (`castpayment.h`, checking and paying
for one cast) and `Instance::PayForCast` (the reply to the client).

## Where the numbers come from

Each ability script sets these, usually as a table with one value per rank:

| Field | Meaning |
| --- | --- |
| `manaCost` | The base cost. A script can replace it with its own `GetManaCost`. |
| `manaCoefficient` | Extra mana per point of the ability's attribute. |
| `cooldown` | Seconds. |
| `cooldownType` | `nCooldownType.Manual` means the script pays later, with `nAbility.PayCooldownAndMana`. |

## Mana

Cost = `nAbility.GetManaCost(namespace, agent, rank)` + `manaCoefficient` × `GetAbilityAttributeValue`.

- `nAbility.GetManaCost` is the game's own function, from `lua/global.lua`. It uses the ability's
  `GetManaCost` if it has one, and its ranked `manaCost` otherwise.
- The coefficient part is added on top for every ability. The evidence is
  `ability_firetempest_active`, whose own `GetManaCost` subtracts that part again at the end,
  which only works out if the engine adds it back.
- The attribute is the ability's `scalingAttribute`, or the caster's primary attribute when it has
  none (`Object::GetAbilityAttributeValue`).
- In overdrive a cast is free (`modifier_overdrive` sets `costReduction` to 1).

There is no mana regeneration in the game data or the scripts: mana comes back from orbs, which are
not built yet. Until they are, `TEST_FREE_MANA` set to 1 in `config.xml` makes every cast free.

## Cooldown

The client works cooldowns out itself (`Darkspore.exe` 0x9dfc70, behind its `ResetAbilityCooldown`),
and the server does the same (`AbilityCost`):

1. The ranked `cooldown`, randomly longer or shorter by up to the ranked `cooldownVariance` share of
   it. One roll picks how much, another picks longer or shorter. Only enemy abilities set a variance.
2. A basic attack's is divided by 1 + `AttackSpeedScale`, which never counts below -0.9.
   Any other's is multiplied by 1 - `CooldownScale`. Overdrive adds 0.4 to `CooldownScale`.
   A negative `CooldownScale` lengthens cooldowns.

Scripts change running cooldowns with four functions. Each does what the client's does
(`CooldownTable`):

| Function | What it does |
|---|---|
| `ResetAbilityCooldown(agent, ability, target, rank)` | Starts the ability's full cooldown at that rank, from now |
| `AddCooldownTime(agent, ability, seconds)` | Moves the end of a cooldown the ability already has |
| `RemoveCooldownTime(agent, ability)` | Ends that cooldown now. Without an ability, ends every one |
| `ScaleCooldownTime(agent, scale, ability)` | Keeps `scale` of what is left of a running cooldown. Without an ability, of every one |

Each change is sent to the client as a cooldown update.

Still approximate:

- **No global cooldown.** The client has one (it stops `ScaleCooldownTime` from going below it),
  but nothing here applies one yet.
- **A player's cooldown** may go through one more client step (0x9de410) that is not read yet.
- **The basic flag.** The client picks the attack speed rule by a flag on the ability; the server
  uses the `IsBasic` descriptor, which should be the same thing.
- **A refused cast** (still cooling down, or not enough mana) gets a reply with no cooldown and does
  not run. The client normally stops these itself, so this only happens around lag.
- **Abilities with no script** keep the old fixed reply: 500 ms, and 2000 ms for the squad ability.
- **Enemies** do not pay yet. That comes with the enemy AI in Stage 2.

## Script functions

- `nAbility.PayCooldownAndMana()` pays for the running cast of a manual ability, once, and sends
  the cooldown to the client. For any other ability it does nothing, because that cast paid when it
  began. Three abilities call it without being manual (the lightning and missile tempests and the
  sleep mushroom); they pay at the cast instead of at their first shot, which is approximate.
- `nAbility.GetAbilityAttributeValue(agent, ability)` gives the attribute described above.
- Still missing: `RemoveCooldownTime`, `ResetAbilityCooldown`, `AddCooldownTime` and
  `ScaleCooldownTime`.

## Creatures from scripts

Pets, drones, summons, clones and boss adds come from `nObjectManager.CreateCreature`. It reads its
arguments the way the client's own version does (`Darkspore.exe` 0xa0aa00):

`CreateCreature(noun, creatureId, x, y, z, [value, dx, dy, dz, extra])`

- `noun` is a noun name such as `NomadDrone.Noun`, or the id `nUtil.GetAsset` gives for one.
- `x, y, z` is where it appears. The new creature is shown to every client, and its passive starts.
- `dx, dy, dz` turns it to face that way. Which way the engine counts as forward is not known,
  so +Y is taken as forward (approximate, only how it looks).
- The return value is the new creature's id, or `kObjIDNone`. Scripts then give it an owner and team.

Not used yet:
- `creatureId`, which the client reads as a 64-bit number. Every hero script passes `"0"`.
- `value`, which the client only uses when it is not 0.
- `extra`, which the client only reads for a player's creature.

`nLocomotion.GetClosestSpawnPosition` gives back the position it is asked about. The game moves it to
the nearest free spot on the navigation mesh, which the server does not have yet (Stage 2).


## Trigger volumes

Auras, charges, healing zones, thin ice and fire walls use trigger volumes: a shape that calls a
script when something touches it. About 30 of the game's scripts make them through `nObjectManager`:

| Function | Arguments |
|---|---|
| `AttachTriggerVolume` | object, radius, enter, exit, stay. The sphere moves with the object. |
| `CreateTriggerVolume` | x, y, z, radius, enter, exit, stay. |
| `CreateTriggerVolumeBox` | x, y, z, width, length, height, facing x, y, z, enter, exit, stay. |
| `DetachTriggerVolume` | object, volume. Ends the volume. |
| `DestroyTriggerVolume` | volume. |

Each one returns the volume's handle, and enter, exit and stay may each be left out.

In the client a volume is a physics phantom, not a game object, so the server never sends one to a
client (`ScriptTriggerVolumes`). What the client does (`Darkspore.exe`):

- Enter runs once when an object starts touching the volume, exit once when it stops, and stay on
  each tick in between, but only when the script gave a stay function (0xa24f90, 0xa16810,
  0xa168a0, 0xa16960).
- Each one gets the volume's handle and the object's id (0xa15f00).
- Any object counts. The hero-only rules and "once only" are settings for triggers placed in a
  level, and scripts cannot set them (0xa16130, 0xa161d0).
- The true or false a callback returns only matters for those once-only triggers. A script volume
  ignores it, so an aura that turns a target down on the way in does not try again until the
  target leaves and comes back.
- Detaching or destroying a volume calls nothing: the client drops its functions first (0xa15d10).
- An object that leaves the world exits every volume it is in first (0xa270a0).
- The functions run in the script that made the volume, so they can still ask about its cast.

Approximate:
- A body is a sphere at the object's position, of half the width of its size class: 2.25 m for
  most bosses, 0.75 to 1 m for heroes. The physics used the creature's real collision shape.
  The footprint radius reaches the corner of that square, about 40% further, which let bosses
  touch volumes a metre early. Darkspin tests the centre point only, which would almost never
  meet the 0.1 m fire wall.
- The box sizes are half sizes, as physics box shapes take them. The firewall gives its scale as
  the length and 0.1 as the width and height.
- A volume's own object counts like any other. Hostile auras skip it through their filter.
- A volume goes when its object does. The client may keep it until the script detaches it.
