# Damage

How one hit is worked out (`Object::TakeDamage`, with the rules in `HitRoll`). Basic weapon attacks
and script calls to `nGameObject.TakeDamage` both go through it.

## Order

1. **Immune.** If the target's `Immune` attribute is above 0, nothing happens. The client is told `Immune`.
2. **Dodge or resist.** Physical hits roll against the target's `PhysicalDefense`, energy hits against
   `EnergyDefense`. Damage over time (`IsDoT`) skips this step.
   - chance = min(defense / DefenseRatingPerPercent, cap × 100) %
   - DefenseRatingPerPercent is the `RatingConversion` value for the current level (10 at 1-1).
   - The cap is `DodgePercentCap` or `ResistPercentCap` from MagicNumbers (both 0.8).
   - A dodged or resisted hit does no damage. The client is told `Dodge` or `Resist`.
3. **Base damage.** A random value in the damage range, times the damage multiplier (never below 0).
4. **Critical.** `AutoCrit` always crits. Otherwise the chance is `CriticalRating / (perPercent × 100)`.
   - For heroes, perPercent is the level's rating table value.
   - For anything else, the rating is counted in whole percents.
   - A critical hit does `CriticalDamageBonus` (2 in MagicNumbers) plus the attacker's
     `CriticalDamageIncrease`, never less than ×1.
5. **Reductions.** Each one is a fraction from 0 to 1, and each takes its share of what is left:
   - `DamageReduction`, always
   - `PhysicalDamageReduction` or `EnergyDamageReduction`, by the hit's source
   - `AoEResistance`, for area hits (`IsAoE`)
6. Health goes down, and the target dies at 0.

The Lua binding still returns `landed, damage, critical`. `landed` is false for a hit that was
dodged, resisted or stopped by immunity.

## Where the numbers come from

- **The client's stat tooltip** (`modifier_binarysentinel_passive`) shows dodge and resist percent
  with the step 2 formula. Ghostform turns a dodge percent into rating the other way around.
- **MagicNumbers.MagicNumbers.xml** holds the caps and the critical bonus.
- **The client** has `Dodge`, `Resist`, `Immune` and `Critical` combat flags.
- **Scripts** set the reductions as fractions: 0.15, 0.25 and 0.5, and 1 for a full physical shield.

## The attacker's side (from the client's tooltip)

The client works out the damage its ability tooltips show in `0x9e5b10`, called from the token
translator `0x43a9d0` once for the minimum token (t = 0, rounded to nearest) and once for the
maximum (t = 1, rounded up). That is the best record of the lost server's formula, so the server
uses the same steps (`AttackerDamage`):

1. **Weapon floor.** A basic attack that is not over time takes at least the weapon's minimum or
   maximum damage (character asset `+0xe8`, `+0xec`).
2. **Primary stat.** × (1 + (primary − `PrimaryAttributeIgnoreAmount`) × the ability's damage
   coefficient) (`0x9e4e60`). The ignored amount is MagicNumbers `+0x34`, the file's only integer, 8.
3. **Bonuses.** × the multiplier from `0x9e4f40`. It starts at 1, or for a basic attack at
   1 + (`PhysicalDefense` + `EnergyDefense`) × `DefenseBoostBasicDamage`. Then it adds `DamageBuff`,
   the damage type's `…TypeDamage` (not for Generic), `ProjectileDamageIncrease` (`IsProjectile`),
   `EnergyDamageBuff` (energy source), `AoEDamage` (`IsAoE`), and `DoTDamageDoneIncrease` (`IsDoT`)
   or else `DirectAttackDamagePercent`. For `IsPhysicalDamage` it adds `PhysicalDamageDoneIncrease`,
   plus `PhysicalDamageDoneByAbilityIncrease` when the hit is not basic. Energy works the same way.
4. **Direct attack.** + `DirectAttackDamage`, except for `IsDoT` and `IsHoT` (`0x9e4ef0`).
5. **AutoCrit.** × (`CriticalDamageIncrease` + `CriticalDamageBonus`, MagicNumbers `+0x30`). The
   tooltip only shows the AutoCrit case; the random critical roll is step 4 above.
6. Never below 0.

`0x9c0530` scales the tooltip down for a hero below the level (game modes 2 and 3 only, with ×0.25
in mode 3). It is left out, because the modes are not identified.

Bit 2 of the descriptors is `IsBasic`. Darkspin reads it as Resonance, and replaces step 3's
defense start value, so its numbers differ there and in the ignored amount (it uses −1).

## Approximate

These are judgement calls. The client cannot settle them: it never reads the rating `Decrease`
values or the leech values from MagicNumbers, so the damage roll was server code that did not ship.
The attacker's side follows the tooltip, as above, which may differ from what the server rolled.

- The reductions multiply. They might add up instead, or have a cap.
- Damage over time is never dodged or resisted.
- `CriticalDamageIncrease` is added to the bonus, not multiplied.
- The attacker's side is applied to the range before the roll, so the minimum and maximum are each
  scaled as the tooltip scales its two tokens.
- A critical hit from a creature uses whole percents.
- `Deflect` is never sent.

## Hero stats

The client works out a hero's stats as class base + primary stat × the MagicNumbers per-point value
(`Darkspore.exe` 0x9e2580). The server does the same (`StatShares`):

| Attribute | Adds |
|---|---|
| MaxHealth | Strength × HealthPerPointofStrength (5) |
| MaxMana | Mind × ManaPerPointofMind (1) |
| PhysicalDefense | Dexterity × PhysicalDefensePerPointofDexterity (6) |
| EnergyDefense | Mind × EnergyDefensePerPointofMind (6) |
| CriticalRating | Dexterity × CritRatingPerPointofDexterity (4) |

The class base comes from the hero's ClassAttributes file. A change to Strength, Dexterity or Mind,
such as a buff, works the shares out again, as the client does each time it reads a stat.
The weapon's damage range comes from the hero's playerclass file.

Not done yet:
- Part stats. Parts carry them in their prefix and suffix, but the server cannot equip parts yet,
  and every stored part has none.
- The client then caps a stat at the class file's `max` value times a scale (0x9e2020) and at a
  per-attribute table (0x1165128). Most classes set no `max`, so this is left out for now.
- Mobs keep their class values as they are. The client's rule does not check for heroes, but mob
  stats also scale with level and difficulty, which is Stage 2.
- Darkspin uses (Strength − 10) × 5 for health. The client code reads Strength × 5, which is what
  the server uses.
