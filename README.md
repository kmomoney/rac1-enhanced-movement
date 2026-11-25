# Enhanced movement (Going Commando)

Ratchet moves like he does in **Ratchet & Clank: Going Commando**, in the first
Ratchet & Clank on [OpenRAC](https://github.com/OpenRAC/OpenRAC). He runs,
turns, jumps, strafes and swings his wrench the Going Commando way, the camera
turns faster, and Clank handles better on his own. Jump heights and damage stay
as they were, so every level plays the way it was designed. Each part has its
own setting, so you can turn off what you don't want.

## Install

In the OpenRAC launcher, open **Mods**, choose **Add from a link** and paste
`https://github.com/kmomoney/rac1-enhanced-movement`.

**Discs:** Ratchet & Clank PAL v2.00 (SCES-50916) and NTSC-U v1.00
(SCUS-97199). Other versions, such as PAL v1.00 or the Japanese disc, aren't
supported. The mod plays the same on both discs.

## What it changes

### Running

- Speed follows how far you push the stick, instead of a fixed walk and run.
  At full stick he's slightly faster than before.
- He gets up to speed and slows down about twice as fast.
- He turns like he does in Going Commando. Push the stick the other way and he
  turns around sharply, slowing as he turns. From a slow start, he moves where
  the stick points before he has finished turning to face it.
- Let go of the stick and he stops on the spot, with no skid.
- He also comes to rest quicker when you stop or crouch.
- X while running gives the running jump only with the stick pushed nearly all
  the way and no strafe held. Otherwise you get the normal jump.

### In the air

A little more control in every jump, and much more in the double jump. Jump
heights and gravity stay the same, pack jumps included.

### Strafe

Hold **L2 or R2** to strafe:

- Ratchet turns to face where the camera looks. The left stick moves him in
  any direction without turning him, and the right stick turns the camera and
  him with it.
- The camera slides along with him instead of swinging round behind him.
- When you switch from one side to the other, he stops before moving the new
  way.
- When you stand still, he turns to face the camera.
- Moving back plays his walk or run in reverse: the first game has no
  backpedal animation.
- **Strafe flips:** X with the stick pushed fully left, right or back does a
  side flip or back flip that way.
- **Hip twist:** his hips turn toward where he's going while his upper body
  keeps facing ahead.
- **L1** still looks in first person and **R1** still crouches. If you strafe
  on one button only, the other works as in the first game (L2 looks, R2
  crouches).

### Wrench

The wrench keeps its moves and damage but uses Going Commando's timing:

- A Square pressed a little early still counts, for a swing or a slam.
- In the combo, the second swing comes a little sooner and the third one's
  finish lasts a little longer.
- With the stick pushed, you run out of a swing sooner.
- After a throw (R1 + Square), you can run or jump while the wrench flies
  back, instead of standing still until you catch it. Square does nothing
  until it's back.
- He turns toward a locked-on enemy a little faster.
- A slam from high up keeps driving forward.
- Square while strafing swings toward the stick, then he goes back to
  strafing.

Damage stays the same, so enemies and bosses take the hits they were built
for. Hit timing and reach are the same in both games anyway.

### Camera

How fast the right stick turns the camera behind Ratchet (and Clank), in
degrees a second at full stick:

| The game's camera speed option | Original | Going Commando | Enhanced (default) |
|---|---|---|---|
| Slowest | 60 | 60 | 102 |
| Middle (the game's default) | 78 | 78 | 132 |
| Fastest | 96 | 132 | 162 |

- **Original** leaves the camera alone.
- **Going Commando** uses that game's speeds. Only its fastest is quicker than
  the first game's.
- **Enhanced** makes every speed faster, so the game's default turns at Going
  Commando's top speed and the game's option still picks slower or faster.

First person (L1) and the game's camera invert options work as before with
every choice.

### Clank

Clank goes on alone on Orxon and Blarg. In the first game he's slow to get
going and to turn there. Pick how he handles:

- **Original:** Clank as the game has him.
- **Going Commando:** he starts, stops and turns quicker, turns around and
  heads the new way at once, leaves the ground sooner when he jumps, turns to
  a target faster and recovers from a punch sooner.
- **Enhanced** (the default): all of that, and he's about 15% faster (still
  much slower than Ratchet), has more control in his jumps, a Square pressed
  early still counts, and with the stick pushed he runs out of a punch sooner.

In every choice, his jump heights, glide, fall, punch damage and reach, and
the Gadgebot commands stay the same. Giant Clank is never changed.

### Swimming

Swimming is the same in both games, so it doesn't change.

## Settings

| Setting | Default | What it does |
|---|---|---|
| Going Commando running | On | Running, turning and stopping, as above |
| Going Commando air control | On | More control in the air |
| Strafe button | L2 or R2 | The button you hold to strafe: L2 or R2, L2 only, R2 only, or Off |
| Strafe flips | On | X while strafing flips that way |
| Hip twist | On | His hips turn while he strafes |
| Going Commando wrench | On | The wrench timing |
| Camera turn speed | Enhanced | Original, Going Commando or Enhanced |
| Clank | Enhanced | Original, Going Commando or Enhanced |
| Trace | Off | For bug reports: writes what the mod does to the game's log, every frame. Each player sets it for themselves |

Settings take effect the next time you start the game. With every switch off,
**Strafe button** on Off, and the camera and Clank on Original, the mod does
nothing at all and the game plays exactly as on the PlayStation 2.

In a multiplayer party, the mod and its settings are each player's own: it
only changes how your own character moves.

## Other mods

- **lock-strafe** also strafes on L2 and R2. The two can't be on together, so
  turn one off.
- **wrench-throw** throws the wrench with R2 + Square, but this mod strafes on
  R2 by default. To use both, set **Strafe button** to **L2 only**. If both
  use R2, one of them won't work.
- **modern-controls** turns the camera with the right stick at its own speed.
  To use both, set **Camera turn speed** to **Original**, so only one mod sets
  the camera's speed. It also changes how fast Ratchet moves; that combination
  hasn't been tested.
- **steam-deck-pack** makes the gyro and right trackpad act as the right
  stick, so this mod's camera speed applies to them too. Lower the pack's
  sensitivity if that feels too fast.
- **difficulty:** its **Ratchet's speed** slider still works and scales this
  mod's speeds too, Clank's included.

## Known limits

- **Jump heights are the first game's** on purpose. Going Commando's normal
  jump is lower, and some ledges in the first game need the higher one.
- **Right after you start running,** X can give the normal jump where Going
  Commando would give the running jump.
- **No backpedal animation:** moving back while strafing plays the walk or run
  in reverse.
- **No strafing** under a level's fixed camera (such as Kalebo III's arena),
  in the Magneboots' own walk on magnetic surfaces, or in the long jump (as in
  Going Commando). With a mod that plays magnetic surfaces as plain ground,
  such as Going Commando Magneboots, you strafe there too.
- **On ice and on conveyors,** Ratchet runs as in the first game.
- **Heli-pack and falls:** a few small Going Commando differences in the
  hover, the glide and how fast he starts to fall are out of the mod's reach.
- **Wrench:**
  - In Going Commando, the combo carries on after a short run. Here, a swing
    after a run starts a new combo.
  - Running out of a swing takes one frame longer than in Going Commando.
  - A few small differences stay as in the first game: in Going Commando the
    thrown wrench carries a bit more of his speed and comes back if it gets
    too far away, the slam keeps driving on some kinds of ground, and a swing
    stops a little later at a ledge.
- **Camera:** only the turn speed changes. First person, the stick's dead zone,
  and how the camera swings back behind him or toward points of interest stay
  as in the first game.
- **Clank:**
  - Giant Clank (Quartu) stays as he is: Going Commando has no walking Giant
    Clank to copy, and his boss fight is built around his slowness.
  - His punch keeps its reach and hit timing. Going Commando's punch hits a
    little sooner and reaches less far; only its quicker recovery is used.
  - L2 still looks around, as in the first game (Going Commando uses L1 only).

**Testing.** The whole game has been played through from start to finish with
the mod installed. The mod was also tested with scripted runs on both discs:
Ratchet on all 19 levels, Clank on Orxon, and the wrench, the camera and Clank
side by side with Going Commando itself. The scripted runs don't cover these:
ice, swimming, the hoverboard, Kalebo III's arena, grind rails, the Swingshot,
the wrench on ice, magnetic surfaces or grind rails, Giant Clank, Blarg's
Clank section, Orxon past its first enemy, the camera with a real stick or a
gyro, and playing alongside modern-controls. The slam from high up and the
faster lock-on turn are checked by the automated tests only. Results:
[PLAYTEST.md](PLAYTEST.md).

## Release notes

- **1.0.1:** Shows up in the launcher's list of mods. It plays the same as
  1.0.0. If you still have Better movement (Going Commando), this mod's old
  version, remove it.
- **1.0.0:** First stable release, after a full playthrough. New **Enhanced**
  camera choice, now the default: faster on every camera speed, 132 degrees a
  second on the game's default. **Going Commando** now uses that game's own
  speeds. If you picked Going Commando before and want the faster camera, pick
  Enhanced.
- **0.4.1:** Plainer README, settings and log messages. It plays the same as 0.4.0.
- **0.4.0:** New **Clank** setting: Going Commando's controls for Clank, or
  **Enhanced** (the default), which goes a little further. Jump heights, punch
  damage and Giant Clank don't change. Ratchet plays as in 0.3.1.
- **0.3.1** (released with 0.4.0): Removed the camera's **Modern** choice,
  which turned far too fast. If you had it, you get **Going Commando**, the
  default.
- **0.3.0:** New **Going Commando wrench** setting, on by default (damage
  unchanged), and a camera turn speed setting. Square and X pressed together
  while strafing now swing, instead of sometimes throwing.
- **0.2.1:** New name, Enhanced movement.
- **0.2.0:** Adds the PAL v2.00 disc.
- **0.1.0:** First version, for the NTSC-U disc.

## For developers

How each part works, and where every number comes from in Going Commando's
code: [DESIGN.md](DESIGN.md). The plan the mod was built to:
[PLAN.md](PLAN.md). The play-test results: [PLAYTEST.md](PLAYTEST.md).

```sh
python3 tools/fetch_sdk.py                 # the SDK sdk.lock pins, into .sdk/
cmake -S . -B build -G Ninja
cmake --build build && ctest --test-dir build --output-on-failure
python3 tools/cpp_style.py                 # layout (clang-format and the blank-line rules)
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/rac1_enhanced_movement -o dist
```

- To try a build, add `build/package/rac1_enhanced_movement` in the launcher's
  **Mods** tab.
- `OPENRAC_ENHANCED_MOVEMENT_TRACE=1` in the environment turns on the trace,
  like the **Trace** setting.
- `tools/playtest.sh` plays a scripted route in OpenRAC (hidden, with and
  without the mod), and `tools/playtest_analyze.py` compares two runs.
- `tools/rac2_constants.py` reads Going Commando's numbers out of
  [rac2-decomp](https://github.com/OpenRAC/rac2-decomp) into
  `tests/data/rac2_constants.tsv`, and the tests check `src/rac1/rac2.h`
  against it.
- Every number of Clank's **Enhanced** choice is in `src/rac1/clank_tune.h`,
  with the reason for it.
- The mod never calls the game's state changes. For one frame it sets what the
  game's own code reads, then puts it back (DESIGN.md, section 1).
- The hook points it shares with other mods are listed in `openrac-mod.json`.
  Each one runs the game's code (or the next mod's) when the mod has nothing
  to do.

Nothing from the game's discs is in this repository: only addresses, offsets
and numbers worked out from the decompilations. Licensed GPL-3.0-or-later.
