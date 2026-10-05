# PP-23 reference and fidelity ledger

Target: Nintendo **Popeye, Wide Screen, PP-23 (1981)**, MAME `gnw_popeye`.
This is a deterministic watch adaptation with generated art, not a ROM emulator
or a timing-accurate reconstruction. The owner's 2026-10-05 references supersede
the earlier invented freighter layout and Lucky Tide rules.

## Sources actually inspected

- [MAME handheld driver](https://github.com/mamedev/mame/blob/master/src/mame/handheld/hh_sm510.cpp): PP-23 definition, input wiring, custom LCD segments and the note about movement after game over. The 1604 × 1080 size and 60 Hz are emulator rendering settings, not game timing.
- [Original manual scan](https://handheldempire.com/manuals/11_1467239843743.pdf), discovered through [Handheld Empire](https://handheldempire.com/game.jsp?game=11). All eight PDF pages inspected; **PP-23 is printed on the cover**. PDF pages below are one-based.
- [Old80s MAME recording](https://www.youtube.com/watch?v=7r-XsTXNuNU): inspected extracted frames at the timestamps below. These establish visible states, not durations, input transitions, or a complete side-switch sequence.
- Owner's reference photograph: Olive beside the left car, right ship, orange boat, food and upper-right score/MISS.

Driver artifact identity, checked against the source:

| Artifact | Identifier |
|---|---|
| Program | `pp-23`, 0x0740 / 1,856 bytes |
| CRC32 | `49987769` |
| SHA-1 | `ad90659a3ce7169a4df16367c5307435d9f9d956` |
| LCD | `gnw_popeye.svg`, 218,587 bytes, CRC32 `4740bcd5` |

The ROM and SVG contents were not obtained. Their identifiers alone cannot
establish a gameplay algorithm. No Table Top/Panorama, arcade or NES mechanics
are used as evidence. Spinach is food here, not a combat power-up.

## Observation table

| Source / location | Observation | Confidence / consequence |
|---|---|---|
| Manual p1 | Cover identifies PP-23 | High; correct manual |
| Manual p4, rules 3–5 | One point per catch; two dropped items or a Brutus hit give one miss; third miss ends play | High; implemented |
| Manual p5, rule 6 | Miss marks clear at 200/500 | High; implemented; pending-drop detail remains open |
| Manual p5, rule 7 | Food quantity and speed return to normal every 100 points | High; implemented cycle reset, numerical table provisional |
| Manual p5, score note | Display rolls over beyond 999 | High; display wraps, app retains true total for records |
| Manual p5, Game B | Brutus moves between right boat and left pier | High; one rival, side changes in B |
| MAME source, general note | Controls remain active after game over | High for continued movement; exact original poses/input mapping still open |
| Recording 00:20 | Clock scene: Olive at car, left hammer, food, upper-right time | High for visible composition only |
| Recording 01:00 / 03:00 / 05:00 | Game A; left hammer, food arcs, can-shaped miss marks | High for visible states only |
| Recording 10:00 | Game A; hammer extended toward Popeye | High for pose; no duration inferred |
| Recording 20:00 | Game B; Brutus left, food and splash visible | High for visible state |
| Recording 23:00 | Game B; Brutus right, fist raised | High for visible state; switch timing unmeasured |

## Implemented watch behavior versus open questions

Rendering uses fixed segment IDs over a white field with saturated red/orange
and blue/turquoise scenery. Olive is fixed beside the car. Four authored food
arcs each have five positions. Brutus has hammer poses on the left and fist
poses on the right, with only one active rival. The score and MISS cans occupy
the upper right. Ghosts are confined to the digit/miss registers for PT2 clarity.
These are newly authored drawings, not the original SVG shapes.

Known rule corrections: one point per catch, no Lucky Tide/double-score mode,
food difficulty reset every 100 points, one side-changing Brutus in B, movement
after game over. Saved settings, UUID and high-score record format are unchanged;
old records may contain scores earned under the previous adaptation's rules.

| Topic | Current implementation; evidence still needed |
|---|---|
| Input and poses | Five poses, one move per press, no hold repeat/automatic recenter. Measure original press/hold/release transitions and every pose. |
| Food paths | Four authored five-position arcs; resolve on final segment. Distinct launch silhouettes identify each route. Measure actual path IDs, step count and catch window. |
| Spawning/difficulty | Seeded xorshift plus reachability reservations. Within each 100 points: 560/440/340/240 ms at 0/25/50/75; capacity 1/2/3 at 0/10/60. Same food curve in A/B. These numbers are provisional, not measured Nintendo values. Existing airborne food finishes across a reset. |
| Brutus timing | 4–9 s idle, two warning steps, one strike step; far pose on active side vulnerable. In B, alternate sides after each strike. Exact switching sequence, hit phases and durations await tracing. |
| Pending drop | A catch preserves it; a hit adds a miss without consuming it; a 200/500 bonus clears it. Manual does not settle these edge cases. |
| Bonus with zero misses | Remains one point per catch. No unsupported extra reward. Verify original zero-miss behavior. |
| Miss sequence | Clear food; freeze attack/food timers for 1,500 ms; third miss stops timers. Measure original pause/reset behavior. |
| Simultaneous events | Attack resolves before food and ends that tick on a hit. Needs original trace. |
| Rollover | Three-digit display wraps, true score/high score continues. Verify original internal reset and milestone repetition beyond 999. |
| Game over/restart | Movement remains enabled, score/timers remain stopped. Select restarts immediately. Original hold-to-show-high-score/release-to-start and ~5-minute return to clock are not reproduced. |

## Reference replay work remaining

The host suite verifies this implementation and its fairness; it does not prove
Nintendo equivalence. Before claiming close fidelity, record original state
transitions and timings for these scenarios, then replay the same input edges:

1. Start A with no input; hold/release each direction and test recentering.
2. Catch one food item, then alternate catches/drops and take a Brutus hit.
3. Repeat in B across a side switch, noting warning/strike/recovery boundaries.
4. Cross 100, 200 and 500 with/without misses and a pending drop; pass 999.
5. Cause a catch/attack overlap and a miss during multiple airborne items.
6. Reach game over, move, and restart with press/hold/release controls.

[Physical PP-23 recording](https://www.youtube.com/watch?v=wqVdknR8228) remains a
reference to inspect; it was not used for input/timing claims in this change.
If a matching ROM becomes available, use the [SM5A source](https://github.com/mamedev/mame/blob/master/src/devices/cpu/sm510/sm5a.cpp),
[SM500 operations](https://github.com/mamedev/mame/blob/master/src/devices/cpu/sm510/sm500op.cpp),
[disassembler](https://github.com/mamedev/mame/blob/master/src/devices/cpu/sm510/sm510d.cpp)
and [MAME debugger](https://docs.mamedev.org/debugger/general.html) to resolve
these tables. Keep platform buttons and pixel scaling outside `game.c`.
