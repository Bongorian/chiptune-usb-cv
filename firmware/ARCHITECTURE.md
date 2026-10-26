# v0.11 engine and verification

## Model and ownership

`model.h` is the only Patch/Track/Project/System definition. A Patch composes chip, source, hardware envelope, layers and numerical modulation. Track adds mode, voice budget and performance controls. `PatchWave` is a derived musical selector, not another stored discriminator. `patch_features` drives chip-specific OLED controls; the generated catalog exports the same wave/features and canonical conversions for Web contract tests. Project owns routes and event generators. Settings contains only the saved snapshot/library; `Engine.system` is the live System. Patch/Track/Project saves cannot accidentally persist unsaved brightness or calibration.

`resources.c` reserves 14 physical circuits: SAA 1–6, SN L1–3, SN R1–3, SN L noise, SN R noise. `outputs` describes audible circuits; `resources` includes silent clocks. Allocation happens transactionally on edits, outside the audio tick. Existing tracks with unchanged structure retain their allocation; failed edits leave voices and project intact. A complete project load explicitly starts a new plan and silences playback. POLY FOCUS is the explicit same-chip consolidation action.

Voice ownership is `(source, channel, note, 32-bit token)`. Per-input pressed/released counters distinguish repeated equal notes and USB/TRS. Held keys survive an in-track steal, DIRECT restores the latest held key, and LEGATO preserves its envelope. Sustain belongs to its input/channel. All Sound Off and PANIC clear ownership so old Note Off messages do not terminate later notes. Resource slots are reserved per Track, never stolen across Track boundaries.

`sequence.c` creates/retires notes through that same engine; it never writes registers. CV and audition also use the same voice path. Modulation changes only continuous parameters; it cannot allocate circuits inside a tick. Unsupported or structurally incompatible morph targets are rejected.

## Register transport and timing

`render.c` computes desired register state every nominal 1 ms. `register_bus.c` services at most one bounded transaction per main-loop call. The queue is the latest value of each register, not an expanding event list. Tone divider byte pairs are atomic snapshots; newer desired values coalesce until the current pair finishes. SAA ENV and SN noise reset epochs preserve retriggers even when register values are unchanged. Mute takes priority over stale pitches.

SN /WE remains LOW asynchronously until a conservative budget of 40 input clocks has completed. READY is unconnected on the routed board. The shared data pins remain unchanged, so SAA register writes also wait while an SN byte is held. MIDI, CV, OLED and synthesis continue. PIO configuration-derived frequency and elapsed time supply conservative clock credit; timing reconfiguration discards uncertain phase. A stop finishes the in-flight divider pair before disabling the clock. No 16–136µs timeout or 250kHz–4MHz clamp remains. Slow or rapidly changing clocks can delay/coalesce updates for an arbitrarily long interval; clock diagnostics expose this. Actual setup/hold and completion margins require measurement.

SAA writes remain short, bounded bus operations. The SN PIO clock runs autonomously. The two-slot modulation engine targets a cooperative 250 µs update interval outside interrupts; RAW/BASE/FULL select pitch reference. PIO half-period and divider share a single frequency equation. Time-sensitive USB/CV/OLED continue receiving loop service, but device throughput and jitter require measurement. OLED transfers are split into 16-byte chunks with retry/reinitialization; CV/OLED services get a forced opportunity after 4 ms. The foreground UI reuses one static menu buffer rather than stacking large item arrays across resource/persistence calls. Tick catch-up uses elapsed time rather than running an unbounded list of missed ticks. Explicit flash saving stops playback and can interrupt MIDI; there is no background autosave.

Pitch ratio, SAA pitch registers, sine and exponential decay use generated LUTs. The render path has no `pow/exp/sin/log` calls. Constant floating-point divisions were changed to reciprocal multiplication without enabling unsafe fast-math. Variable ADSR/glide/divider calculations still use RP2040 ROM float operations. The compositional morph uses bounded integer interpolation.

## Tests and what the numbers mean

- Nine ASan/UBSan host suites cover model/resource constraints, all factories, MIDI ownership/steals/sustain/legato, stereo PAN, SAA ENV/AM/layers/noise and SN metal/sweeps, CV freshness/reroute/0V, transport/arp, torn writes/rollback/independent saves, strict SysEx version gating, bounded bus transport, UI fuzzing and OLED recovery.
- Node tests exchange actual protocol frames with the portable C engine. Browser tests run the same C bridge through mock Web MIDI: offline JSON, connect/read/write, old-version write lock, resource rejection, POLY FOCUS, PAN, morph, sequence/CV/global saves, asynchronous LIVE slider preservation and mobile overflow. No physical MIDI devices are used by these tests.
- The linked ARM Release ELF executes through startup and stressed engine ticks in Unicorn. Only platform/ROM/SIO boundaries are functionally stubbed. Boot tests reject flash writes, validate storage startup and execute the real SDK capacity guard. Runtime probes record instruction counts, stubbed arithmetic calls and observed stack depth.
- `Performance.json` is host timing over 10,000 ticks each at 10 and 14 logical voices with envelopes, glide, LFO, morph and eight modulation routes. It is not RP2040 timing or an exhaustive maximum over all project states.
- `Runtime_Verification.json` excludes ROM/SIO implementation latency, XIP stalls, interrupts, GPIO timing and peripheral effects. Counts are useful for regression and locating expensive operations, not a cycle estimate or WCET certificate. `Verification.json` records ELF/BIN/UF2 checks and RAM usage for the exact build.

The packaged runtime report records the instruction counts and stubbed runtime calls for the current stress configuration. It does not establish the hardware deadline.

**On-device worst-case timing is an open hardware validation item.** The firmware records `tick_max_us` and `tick_overruns` (both exposed by SysEx diagnostics); overruns include a missed update interval. Measure 14 independent voices, both inputs at MIDI saturation, CV/Gates, OLED editing, all clock modes, and low/experimental-high SN clocks, pending transfers and stop/resume. If the measured tick exceeds 1 ms, reduce update work/voice cost before declaring a production real-time guarantee. Host timing does not establish RP2040 timing margin.

## Removed design

The old `studio.c`, `studio_types.h` and parallel engine have been removed. There are no role/model-number dispatch tables, old setup/profile migrations, raw-register editor protocol, old scene format, or legacy test engine. Original versions remain recoverable in Git history. Twenty-three factories now use the same compositional Patch model as user sounds. Old waveform cartoons were replaced by a labeled ADSR illustration; it is not a measurement or chip waveform simulation.

## Flash integrity

Physical flash stays 2 MiB in SDK guards; only the linker limits application space to `0x1f0000`. The two 32 KiB settings banks start at offsets `0x1f0000` and `0x1f8000`. Only version 11 with exact size, CRC, commit marker and semantic/resource validity is accepted. The new bank is erased/programmed while the previous bank survives; the first-page commit marker is programmed last. Software tests interrupt every pre-commit page and corrupt CRC/versions, including sequence wrap. Real flash brownout behavior is not emulated.

The ARM clock-control probe executes the linked initialization, MMIO writes, low-clock transfer/stop, high-clock resume and PANIC path. MMIO alias behavior and ROM/time boundaries are modeled; PIO edges are checked separately from assembled instructions. `Clock_Verification.json` is not a device timing test.
