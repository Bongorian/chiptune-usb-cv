# v0.10 engine and verification

## Model and ownership

`model.h` is the only Patch/Track/Project/System definition. A Patch composes chip, source, hardware envelope, layers and numerical modulation. Track adds mode, voice budget and performance controls. Project owns routes and event generators. Settings contains only the saved snapshot/library; `Engine.system` is the live System. Patch/Track/Project saves cannot accidentally persist unsaved brightness or calibration.

`resources.c` reserves 14 physical circuits: SAA 1–6, SN L1–3, SN R1–3, SN L noise, SN R noise. `outputs` describes audible circuits; `resources` includes silent clocks. Allocation happens transactionally on edits, outside the audio tick. Existing tracks with unchanged structure retain their allocation; failed edits leave voices and project intact. A complete project load explicitly starts a new plan and silences playback. POLY FOCUS is the explicit same-chip consolidation action.

Voice ownership is `(source, channel, note, 32-bit token)`. Per-input pressed/released counters distinguish repeated equal notes and USB/TRS. Held keys survive an in-track steal, DIRECT restores the latest held key, and LEGATO preserves its envelope. Sustain belongs to its input/channel. All Sound Off and PANIC clear ownership so old Note Off messages do not terminate later notes. Resource slots are reserved per Track, never stolen across Track boundaries.

`sequence.c` creates/retires notes through that same engine; it never writes registers. CV and audition also use the same voice path. Modulation changes only continuous parameters; it cannot allocate circuits inside a tick. Unsupported or structurally incompatible morph targets are rejected.

## Register transport and timing

`render.c` computes desired register state every nominal 1 ms. `register_bus.c` services at most one bounded transaction per main-loop call. The queue is the latest value of each register, not an expanding event list. Tone divider byte pairs are atomic snapshots; newer desired values coalesce until the current pair finishes. SAA ENV and SN noise reset epochs preserve retriggers even when register values are unchanged. Mute takes priority over stale pitches.

SN /WE remains LOW across loop iterations for the configured conservative 16–136 µs; the main loop can process MIDI, CV and OLED during this interval. Shared data pins are not changed during the hold. A slowing clock extends an in-flight hold to the safer deadline. This eliminates accumulated synchronous SN write waits, at the cost of delayed/coalesced fast control changes under bus saturation. It does not promise that every 1 ms modulation sample reaches the IC at a 250 kHz clock. Actual hold/setup/throughput still needs logic-analyzer confirmation.

SAA writes remain short, bounded bus operations. The SN PIO clock and 250 µs integer modulation IRQ remain separate from synthesis. OLED transfers are split into 16-byte chunks with retry/reinitialization; CV/OLED services get a forced opportunity after 4 ms. The foreground UI reuses one static menu buffer rather than stacking large item arrays across resource/persistence calls. Tick catch-up uses elapsed time rather than running an unbounded list of missed ticks. Explicit flash saving stops playback and can interrupt MIDI; there is no background autosave.

Pitch ratio, SAA pitch registers, sine and exponential decay use generated LUTs. The render path has no `pow/exp/sin/log` calls. Constant floating-point divisions were changed to reciprocal multiplication without enabling unsafe fast-math. Variable ADSR/glide/divider calculations still use RP2040 ROM float operations. The compositional morph uses bounded integer interpolation.

## Tests and what the numbers mean

- Eight ASan/UBSan host suites cover model/resource constraints, all factories, MIDI ownership/steals/sustain/legato, stereo PAN, SAA ENV/AM/layers/noise and SN metal/sweeps, CV freshness/reroute/0V, transport/arp, torn writes/rollback/independent saves, strict SysEx version gating, bounded bus transport, UI fuzzing and OLED recovery.
- Node tests exchange actual protocol frames with the portable C engine. Browser tests run the same C bridge through mock Web MIDI: offline JSON, connect/read/write, old-version write lock, resource rejection, POLY FOCUS, PAN, morph, sequence/CV/global saves, asynchronous LIVE slider preservation and mobile overflow. No physical MIDI devices are used by these tests.
- The linked ARM Release ELF executes through startup and stressed engine ticks in Unicorn. Only platform/ROM/SIO boundaries are functionally stubbed. Boot tests reject flash writes, validate storage startup and execute the real SDK capacity guard. Runtime probes record instruction counts, stubbed arithmetic calls and observed stack depth.
- `Performance.json` is host timing over 10,000 ticks each at 10 and 14 logical voices with envelopes, glide, LFO, morph and eight modulation routes. It is not RP2040 timing or an exhaustive maximum over all project states.
- `Runtime_Verification.json` excludes ROM/SIO implementation latency, XIP stalls, interrupts, GPIO timing and peripheral effects. Counts are useful for regression and locating expensive operations, not a cycle estimate or WCET certificate. `Verification.json` records ELF/BIN/UF2 checks and RAM usage for the exact build.

The earlier 14-voice probe had 320 stubbed float divisions per held tick; reciprocal conversion and conditional noise-Hz evaluation reduced that baseline to 60. The final stress case also exercises attack and eight MIDI modulation sources; its current counts are in the packaged report. This is optimization evidence, not proof of the 1 ms deadline.

**On-device worst-case timing is an open hardware validation item.** The firmware records `tick_max_us` and `tick_overruns` (both exposed by SysEx diagnostics); overruns include a missed update interval. Measure 14 independent voices, both inputs at MIDI saturation, CV/Gates, OLED editing, all clock modes, and 250 kHz SN writes. If the measured tick exceeds 1 ms, reduce update work/voice cost before declaring a production real-time guarantee. Do not infer hardware headroom from host microseconds.

## Removed design

The old `studio.c`, `studio_types.h` and parallel engine have been removed. There are no role/model-number dispatch tables, old setup/profile migrations, raw-register editor protocol, old scene format, or legacy test engine. Original versions remain recoverable in Git history. Twenty-three factories now use the same compositional Patch model as user sounds. Old waveform cartoons were replaced by a labeled ADSR illustration; it is not a measurement or chip waveform simulation.

## Flash integrity

Physical flash stays 2 MiB in SDK guards; only the linker limits application space to `0x1f0000`. The two 32 KiB settings banks start at offsets `0x1f0000` and `0x1f8000`. Only version 10 with exact size, CRC, commit marker and semantic/resource validity is accepted. The new bank is erased/programmed while the previous bank survives; the first-page commit marker is programmed last. Software tests interrupt every pre-commit page and corrupt CRC/versions, including sequence wrap. Real flash brownout behavior is not emulated.
