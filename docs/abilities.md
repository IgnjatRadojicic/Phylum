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
