# Spooky Stable v2: what changed from v1

v1's single `app/app.c` (2,220 lines) is now an app unity build of modules (about 7,400 lines in all); the
platform calls are v1's exactly, and nothing in `base/` is changed. See README.md for the controls and for every
place v2 departs from the design document.

## New files

| File | Design § | What it is |
|---|---|---|
| `app/util.h` | — | random numbers, maths, RGB565 colour helpers, the universal and content colours |
| `app/draw.h`, `app/draw.c` | 2.6, 2.7 | v1's scanline primitives (moved out of app.c) plus capsules, arcs, stars, hearts, the 16×16 glow sprite, silhouettes |
| `app/fx.h`, `app/fx.c` | 2.1–2.3, 2.9, 11 | easing library, trauma shake, hitstop, impact frames, the flash limiter (v1's), rings, six transitions |
| `app/mood.h`, `app/mood.c` | 2.4, 2.5, 3.1 | the 8 moods, scales, `quantize_to_scale()`, the dithered sky column, parallax props (blast / suck / return) |
| `app/particles.h`, `app/particles.c` | 2.7 | the 200-slot particle pool, 19 types, mood-driven defaults |
| `app/patch.h`, `app/patch.c` | 3.4–3.6 | `patch_t`, all 60 patches (reference patches + variants), the 5 drum kits |
| `app/voice.h`, `app/voice.c` | 3.2, 3.3, 6 | 12-voice FM synth chain, stealing with a 5 ms fade, delay/chorus/bitcrush/duck, limiter, the drum sequencer |
| `app/unicorn.h`, `app/unicorn.c` | 2.6 | the one procedural unicorn rig, its secondary motion, 12 accessories, 6 manes |
| `app/toys.h` | 8.1 | `toy_def_t`, the registry's types |
| `app/knobs.h`, `app/knobs.c` | 4 | the knob framework, overlay icons, settings persistence |
| `app/ui.h`, `app/ui.c` | 2.10, 3.7, 7 | LED choreography, chaos balloon, charge, mash, idle (wave / sleep / dream), overlays, replay, dancers |
| `app/toys/common.c` | — | what every toy shares: the unicorn's pose plumbing, impacts, wardrobe, the mini ghost prop |
| `app/toys/fart.c` | 1, 2.1 | Unicorn Fart (ported from v1; v1's timing and cloud kept) |
| `app/toys/poop.c` | 1 | Unicorn Poop (ported from v1; pile, tower dance and giggling ghost kept) |
| `app/toys/pee.c` | 1 | Unicorn Pee (ported from v1; stream, pool, reflection kept) |
| `app/toys/burp.c` | 2.8 | Unicorn Burp (new) |
| `app/toys/sneeze.c` | 2.8 | Unicorn Sneeze (new) |
| `app/toys/drumjam.c` | 6 | Drum Jam with loop recorder and auto-drummer (new) |
| `app/toys/toilet.c` | 2.8 | Toilet Flush (new) |
| `app/toys/booger.c` | 2.8 | Booger Flick (new) |
| `tests/make_soak.py` | 10 | writes an N-minute random-play soak script |
| `CHANGES.md` | — | this file |

## Modified files

| File | Change |
|---|---|
| `app/app.c` | rewritten: now the registry, input (tap / long press), mood landing, toy switching, the frame loop, test hooks |
| `app/app.h` | `APP_DIM_LEVELS` (breathing key LEDs) and `APP_OVERLOAD` (the synth sheds voices if the render runs late) |
| `app/tests/basic.txt`, `app/tests/stress.txt` | rewritten for v2's definition of done |
| `tests/run_tests.sh` | runs basic, stress and a 5-minute soak |
| `app/words.tsv`, `app/fm1.toml`, `firmware.json` | the eight toys' names and the new description |
| `README.md` | v2's controls, code map, and the differences from the design document |
| `setup.sh` | `git submodule add -f`: v1's failed on a fresh unzip because `base/` is in `.gitignore` |
| `.github/workflows/release.yml` | the comment's example tag (v2.0) |
| `app/docs/img/fart.png`, `poop.png`, `pee.png` | new screenshots |

New screenshots: `app/docs/img/burp.png`, `sneeze.png`, `drum.png`, `toilet.png`, `booger.png`, `moods.png`.

## Removed

- The Ghost and Monster toys, their buttons (FX and ENV now open Burp and Sneeze), GLO's surprise glitch and Mega Spook,
  and the K2 animation-speed setting (K2 is each toy's own now). v1's ghost drawing survives only as the tiny
  giggling poop ghost behind `PROPS`.
- `app/docs/img/ghost_oops.png`, `ghosts.png`, `monster_roar.png`, `mega.png`.

Settings use a new magic ("SST2"): a v1 unit starts v2 with defaults.
