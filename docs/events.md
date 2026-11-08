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

- **BreakRoot (2).** Sent (the scripts' own sends, and `TeleportObject`). Raised by actions, not
  by a root ending.
  - Quantum Blink, Lightning Rogue Active and Time Ravager Support send it to their own agent on
    activate (L7428, L52095, L95830).
  - `nLocomotion.TeleportObject` (0xa09a90) raises it on the object before it moves (0xa09b2c).
    `TeleportAndFace` does not.
  - No slots. `nModifier_Root_Template`'s `handleEvent` returns false on it, but no root ends:
    every root that uses the template (Entangle, BioEntangle, EntanglingRush, DrainRoot,
    AC_Freeze, SupportHealer_Root, VerdanthBasicRootmob_Root) sets a numbered `HandleEvent` that
    calls `handleEvent` and drops its answer (`CALL 1 3 1`, `RETURN 0 1`). The client calls only
    the numbered function (it has no "handleEvent" string), so a nil answer keeps the root. The
    server does the same; a teleport out of a root leaves it on.
- **InterruptModifier (16).** Only scripts send it. Gravity Storm's Deactivate sends it to each
  lifted target while the storm is still interruptible (L47908). `nModifier_GravityStorm` then skips
  its slam. No slots.
- **StartedNewAction (256).** Sent: at a cast's start, by MoveToObject, MoveToCircleEdge and
  MoveToPointWithinRange, and by the player's move order and the walk of an attack order
  (MoveTowardObject is not on the server yet). No slots.
  - The server raises it with UsedAbility before the script runs, so a new cast never hears its
    own. Sleeping Cloud's channel answers false to every one, so if its own reached it, the channel
    could never run; in the client the new cast must miss it too (not traced in 0x9e0660).
  - The client also raises UsedAbility only for an ability that needs its agent (the same `+0x5a`
    test). The server still raises UsedAbility for every cast: what an ability that does not set
    `requiresAgent` reads there is not confirmed.
  - Raised after a cast starts, right after UsedAbility, when the ability definition's byte `+0x5a`
    is set (0xa67920 → 0x9e1540, raise at 0x9e15e6).
  - Raised by the move-goal setters 0xa148c0 and 0xa149c0 (MoveToObject, MoveTowardObject,
    MoveToCircleEdge, MoveToPointWithinRange, and the player's move and attack orders).
  - Sleeping Cloud's channel (the ability, L13388) returns false on it, so walking or casting ends
    the channel.
- **LostModifierOfType (4096).** Sent, from `ModifierSystem::Finish` once the modifier is gone (not
  to an agent marked for deletion). Raised when a modifier leaves an agent that has no other modifier
  with the same GUID (0x9e1261, in 0x9e11f0). GUID 1 is the modifier's GUID, not its id.
  `GainedModifierOfType` (2048) is the mirror (0x9e0fe8). Stealth Attack (the ability, L89490) ends
  when its stealth modifier goes.
- **PlayerSwap (32768).** Sent, from `Instance::SwapCharacter` before the outgoing hero's cast is
  cancelled. Not in the client or any script, so it was the server's. The Field Medic
  health buff ends on it (L158238). No slots. Sent to the hero leaving (judgement call; Darkspin
  keeps the buff instead).
- **Incorporeal (262144).** Sent (`ProjectilePassList`, from the projectile's flight). In a
  projectile's hit test (0xa2f7f0), a target already on the projectile's pass list lets it pass;
  otherwise one with `Incorporeal` (0x37) above a [0, 1) roll, or with any `Intangible` (0x1d),
  lets it pass and is written down (an 8-entry list at 0xa2f7c0; with the list full it is not, and
  no event goes out). The event goes out only when a target is written down, so at most once per
  projectile and target. The server rolls only for a creature the shot really reaches, not every
  creature its sweep comes near. Raise at 0xa2f763: GUID 1 the target, GUID 2 the projectile. Ghostform's buff, ranks
  7 and 8, renews itself on it.

## For enemies (stage 2)

`ModifierInstanceAdded` (512), `ModifierInstanceRemoved` (1024) and `NearbyDamage` (128), used by
the Nomad, Citadel, NPC affix and Trapper scripts. `CrystalBonus` (16384) waits for stage 5.
