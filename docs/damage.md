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

## Approximate

These are judgement calls, to be replaced by reading the client's own damage code.

- The reductions multiply. They might add up instead, or have a cap.
- Damage over time is never dodged or resisted.
- `CriticalDamageIncrease` is added to the bonus, not multiplied.
- The damage coefficient is not used yet. It probably adds coefficient × the attacker's stat, as
  the mana coefficient does, but that is not confirmed.
- A critical hit from a creature uses whole percents.
- `Deflect` is never sent.
- The `Dodged` ability event is not raised yet.
