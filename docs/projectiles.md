# How projectiles work

What we learned from the game's scripts, the client and the [darkspin](https://github.com/darkspinnet/darkspin) project.

## The client flies them

The server does not send a projectile's position while it flies. The client flies its own copy from one locomotion update. The server flies a copy too, only to work out hits and landings.

For each shot the client gets:

1. `ObjectCreate`, at the launch position
2. `ServerEvent`, attaching the trail effect, which is what you see because projectile nouns have no model
3. `LocomotionDataUpdate`, with the projectile or lob parameters
4. `ObjectDelete`, when the script finishes

Sending teleports instead pins the shot at the hero. Every new object starts with goal flag `0x20`, which means teleport, so projectiles skip that rule in `Instance::SendObjectUpdate`.

## What scripts expect

| Call | Arguments | Gives back |
| --- | --- | --- |
| `nThread.WaitForProjectile` | agent, projectile, parameters, target, validator | hit id, hit ground, x, y, z, range left |
| `nThread.WaitForLobbedProjectile` | agent, projectile, x, y, z, height, flight time, speed, strikes ground only, bounces, restitution, stop bounce on creatures | x, y, z, and a fourth value we do not send |
| `nThread.CreateThreadForObject` | object, function, arguments | nothing |
| `nGameObject.GetCenterPoint` | object | x, y, z |
| `nGameObject.GetOrientation` | object | four values, only ever handed back to the engine |
| `nUtil.GetAsset` | name | an id, the hash of the full name |

Projectile parameters use the engine's field names, such as `mSpeed` and `mRange`. `mDirection` is a plain list of three numbers.

A script loops on `WaitForProjectile`. A normal shot stops at its first hit. A piercing shot passes the range left into its next call, so hits wait in a queue and come back one at a time.

A shot's caster data travels on the projectile through `SetAttributeSnapshot`, and `TakeDamage` needs it when the shot lands.

## Hits

`ProjectileSweep` tests the whole path of a tick, so a fast shot cannot jump over a mob. It sweeps the projectile's box, from its noun's `creatureCollisionVolume`, against a box around each mob, so a shot can pass over a short mob. A mob's box comes from its footprint and height.

Hits are ordered by distance along the path, then the intended target, then lowest object id. A shot stops at its range, and a contact right at the end of its range is a miss.

## Lobs

`LobArc` holds the curve: steady speed along the ground, with height as `start + linear * distance + quadratic * distance * distance`. The lob data block is 84 bytes and carries the start, destination, height and duration as well as the curve.

## Approximate or missing

- Shots ignore walls, because the server cannot test level geometry.
- Homing turns straight at the target, ignoring turn rate and delay.
- Bounces are approximate on the server: each keeps its ground speed and is shorter by the restitution. Only `Trapper_Grenade` bounces.
- A lob never hits a creature in flight, and bounces do not stop on creatures on the server, whatever the script asks.
- The fourth result of a lob is not sent. In the toss template it only reaches `ProjectileLanded`, which is empty there.
- The smallest mob radius (0.5) and the script tick time (0.05 seconds) are chosen values.
- Velocity reaches the client in an `ObjectUpdate` just after `ObjectCreate`. Darkspin puts it in the create packet, which would mean reordering the launch, since the effect needs the object to exist first.

## Script data worth knowing

`BoulderToss` has a `damageCoefficient` of 0, and the toss template only deals damage above 0, so it lands without hurting anything.

Basic attacks use the caster's weapon damage, so a test caster needs one. `Trapper_Grenade` and `VoodooTempestBasic` do nothing without it.
