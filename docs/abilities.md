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

Cooldown = ranked `cooldown` × (1 − `CooldownScale`), clamped to between none and the full cooldown.
A basic attack's cooldown is also divided by 1 + `AttackSpeedScale`, as its waits are.

Approximate:

- **CooldownScale as a fraction off.** Overdrive adds 0.4 to it and calls that `cooldownReduction`,
  so a positive value shortens cooldowns. Whether the engine takes it off as a fraction (0.4 means 40%
  shorter) or divides by 1 + scale (29% shorter) is not known. The fraction reads closer to the name.
- **No global cooldown.** `noGlobalCooldown` is read but nothing applies one.
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
