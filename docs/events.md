# Ability events

The events scripts subscribe to with `handledEvents` (`nAbilityEventFlags` in GlobalDefinitions.lua),
how the client raises them and what they carry. "L" is a line in the luac listing of all scripts.

## Delivery (client 0x9df330 → 0x9deca0)

- `nAbility.SendAbilityEvent(event, agentId)` delivers at once (0x9df330), then frees the event.
- An event goes to one object. First its running abilities (the agent's list at `+8`), then its
  modifiers (`+0x3c`), each if its `handledEvents` has the type.
- The handler's result starts as `true`. Only an explicit `false` ends that ability or modifier.
  A handler that returns nothing keeps it running. The end goes through `0x9de9c0` (abilities)
  and `0x9dea20` (modifiers): an instance that is busy (`+0x164`) is only marked (`+0x165`) and
  ends later; any other ends at once through `0x9e11f0`.
- All data types share one slot array: slot n is at `+4n`, whatever its type.

The server (`AbilityEventDelivery`) does the same. The casts come from `RunningCasts`, filled as a
cast starts. A cast that is running when it is told to end (it sent the event to its own agent)
ends at its next wait, and a modifier whose own thread is running ends at the next update.

## Sent by the server (done)

| Event | Value | Raised | Slots |
| --- | --- | --- | --- |
| TookDamage | 1 | a hit lands | see damageevents.h |
| UsedAbility | 4 | a cast starts (client 0x9e15de) | Int 2 descriptors, slot 3 the ability id |
| StackModifier | 32 | a modifier is requested again | |
| NearbyDeath | 64 | something dies nearby | GUID 1 the dead, Float 2 distance |
| DealtDamage | 8192 | a hit lands | see damageevents.h |
| Dodged | 65536 | a hit is dodged | GUID 1 itself, GUID 2 the attacker |

## For heroes, still to send

- **BreakRoot (2).** Raised by actions, not by a root ending.
  - Quantum Blink, Lightning Rogue Active and Time Ravager Support send it to their own agent on
    activate (L7428, L52095, L95830).
  - `nLocomotion.TeleportObject` (0xa09a90) raises it on the object before it moves (0xa09b2c).
    `TeleportAndFace` does not.
  - No slots. `nModifier_Root_Template` returns false on it, so the root ends (Entangle and the
    other roots inherit that).
- **InterruptModifier (16).** Only scripts send it. Gravity Storm's Deactivate sends it to each
  lifted target while the storm is still interruptible (L47908). `nModifier_GravityStorm` then skips
  its slam. No slots.
- **StartedNewAction (256).** No slots.
  - Raised after a cast starts, right after UsedAbility, when the ability definition's byte `+0x5a`
    is set (0xa67920 → 0x9e1540, raise at 0x9e15e6).
  - Raised by the move-goal setters 0xa148c0 and 0xa149c0 (MoveToObject, MoveTowardObject,
    MoveToCircleEdge, MoveToPointWithinRange, and the player's move and attack orders).
  - Sleeping Cloud's channel (the ability, L13388) returns false on it, so walking or casting ends
    the channel.
- **LostModifierOfType (4096).** Raised when a modifier leaves an agent that has no other modifier
  with the same GUID (0x9e1261, in 0x9e11f0). GUID 1 is the modifier's GUID, not its id.
  `GainedModifierOfType` (2048) is the mirror (0x9e0fe8). Stealth Attack (the ability, L89490) ends
  when its stealth modifier goes.
- **PlayerSwap (32768).** Not in the client or any script, so it was the server's. The Field Medic
  health buff ends on it (L158238). No slots. Sent to the hero leaving (judgement call; Darkspin
  keeps the buff instead).
- **Incorporeal (262144).** In a projectile's hit test (0xa2f7f0), a target with `Incorporeal`
  above 0 that wins a roll lets it pass, at most once per projectile and target (an 8-entry list at
  0xa2f7c0). Raise at 0xa2f763: GUID 1 the target, GUID 2 the projectile. Ghostform's buff, ranks
  7 and 8, renews itself on it.

## For enemies (stage 2)

`ModifierInstanceAdded` (512), `ModifierInstanceRemoved` (1024) and `NearbyDamage` (128), used by
the Nomad, Citadel, NPC affix and Trapper scripts. `CrystalBonus` (16384) waits for stage 5.
