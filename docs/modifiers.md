# How modifiers work

What we learned from the game's scripts, the client and the [darkspin](https://github.com/darkspinnet/darkspin) project.

A modifier is any effect that sits on an object for a while: a slow, a stun, a damage over time, a passive. Modifiers are scripts, built the same way as abilities.

## Who does what

| Part | Job |
| --- | --- |
| `AttributeStack` | Combines the changes on one object's attributes |
| `ModifierManager` | Decides what a request does, and when a modifier ends. Knows nothing about scripts or packets |
| `ModifierSystem` | Turns those decisions into script calls, attribute changes and packets |
| `scriptapi.cpp` | Translates the `nModifier` and `nAttribute` calls |
| `ModifierCheck` | Runs real modifier scripts in `verify.ps1`, and writes `script-modifier.txt` |

## A modifier's life

1. A script calls `nModifier.RequestModifier(agent, initiator, id, snapshot, rank, ...)`. Anything after the rank comes back through `GetFloatProperty`, numbered from zero.
2. `CanUseModifier(agent, initiator, rank)` can refuse it.
3. The activation type decides what happens, as the table below shows.
4. A new modifier gets its own thread, which runs `activate`, then `tick`. Most ticks call `nThread.WaitForever`.
5. When its time is up, or something marks it, the thread stops, `deactivate` runs, and any attribute changes it still holds come off.

Scripts only mark a modifier, because one may be ending itself. It ends at the next update.

The functions can be set by name or by their `nAbilityFns` number, and scripts add them after registering, so they are looked up when the modifier runs.

| Activation type | A second request |
| --- | --- |
| `Default` | Runs beside the first |
| `Unique` | Replaces it |
| `CasterUnique` | Replaces the one from the same caster |
| `CasterUniqueIrreplaceable` | Refused if the same caster has one |
| `UniqueIrreplaceable` | Refused |
| `Stacks` | Sends the script a `StackModifier` event |
| `StacksAndCasterUnique` | The same, per caster |
| `UniqueResets` | Starts its time again |

Stacking scripts count for themselves: their handler calls `IncrementStackCount` and `ResetDuration`, and returns true. If no handler says so, the server does both.

## Attributes

`nAttribute.AddAttributeModifier(id, attribute, value, operation)` gives back a handle. Inside a modifier the change ends with the modifier, which `AutoDestructSpeedModifier` relies on.

| Operation | Value | Used for |
| --- | --- | --- |
| `kAdd` | 1 | The default |
| `kPercentChange` | 2 | Stats such as `MaxHealth` |
| `kClamp` | 3 | A limit, such as the tunnel teleporter's walk speed |
| `kAddBest` | 4 | Only the best one counts |
| `kPercentChangeBest` | 5 | Only the best one counts |

**Script attribute numbers are not `AttributeType` values.** Scripts use the game's original order, which `OriginalAttributes` matches. `AttributeType` leaves out four attributes the client does not see, so numbers from 74 upwards differ. Everything from a script goes through `AttributeFromScript`.

## Packets

All fields are little endian, after the packet id.

| Packet | Fields |
| --- | --- |
| `ModifierCreated` (0xA2) | object u32, modifier id u32, instance u32, duration ms u32, overdrive u32, stacks u32, start ms u64, NPC affix asset u32, bound u8 |
| `ModifierUpdated` (0xA3) | object u32, instance u32, start ms u64 (all bits set keeps the old one), stacks u32, bound u8 |
| `ModifierDeleted` (0xA4) | object u32, instance u32 |

Bound means the modifier blocks actions, not only movement.

## Script data worth knowing

- `IsChainGame` means the campaign, as opposed to the arena. Immunities such as `ImmuneToSlow` only refuse a modifier in a chain game, so the adapter answers true.
- The five hashed files in `lua/` are libraries no script can require by name, so they load at startup. `AutoDestructSpeedModifier` is in one.
- Not every modifier is required by an ability, so the whole `Modifiers` folder loads at startup.
- A handler takes one argument and uses it as the event, so it gets a table that also reads as the modifier.

## Lua traps

- A `sol::table` remembers the thread it came from. Anything that outlives the call must be a `sol::main_table`, or releasing it touches a thread that is gone.
- A pooled thread can be reused or deleted. Keep the ticket from `TrackThread`, not the pointer.
- The ability self-check turns waits into stubs, so a looping tick would never end there. `RequestModifier` is a stub in that test, and `ModifierCheck` covers modifiers.

## Approximate or missing

- The order operations combine in is our best reading. The game's files name them, but the rule lived on EA's server: `(base + adds + best add) * (1 + percents + best percent)`, then the lowest clamp.
- What each activation type does comes from its name.
- Which debuffs count as crowd control, and which are bound, is chosen from their names.
- Durations follow the client's order, which darkspin recovered. Which object supplies each attribute is chosen: the caster for `BuffDuration`, the target for the others. `DebuffDurationIncrease` is not used, and the client's diminishing returns are not done.
- Only `StackModifier` reaches scripts. `TookDamage`, `DealtDamage` and the other events do not, so modifiers that react to them do nothing yet.
- Passive modifiers are not started for heroes and mobs.
- `Default` deactivation keeps a modifier through death until its time is up or its object is destroyed.
- Overdrive and the NPC affix asset are always 0.
- Attribute numbers from 74 upwards have not been confirmed on the wire.
- Slows reach the client as attribute changes. Whether it moves heroes slower has not been checked in the game.
- `script-modifier.txt` lists the scripts that fail. Most need engine functions outside modifiers, such as `AttachTriggerVolume`, or properties the check does not pass.
