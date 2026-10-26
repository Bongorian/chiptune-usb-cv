/* Shared units and wire format for firmware 0.11 / protocol 3.
   Values describe configured PIO timing, not a frequency measurement. */
export const SYS_HZ = 128000000,
  DIV_MIN = 256,
  DIV_MAX = 16777216;
export const MIN_HZ = SYS_HZ / (2 * (0xffffffff + 5) * 65536),
  MAX_HZ = SYS_HZ / 10;
export const fxNames = [
  "Off",
  "Manual",
  "Rise",
  "Triangle",
  "Square",
  "Random",
  "Sequence",
  "Dive",
  "Sine",
  "Saw",
  "Random walk",
  "Slew random",
  "Wow",
  "Flutter",
  "Curve envelope",
  "Ratchet",
  "Burst",
  "Audio-rate",
  "Saw down",
];
export const syncClocks = [0, 1, 2, 3, 4, 6, 8, 12, 24];
export const domainNames = [
  "MUSIC · cents",
  "PHYSICAL · Hz",
  "PHYSICAL · ratio ppm",
  "PHYSICAL · PIO half-period",
];
export function clockDefault() {
  return {
    base_hz: 2000000,
    period: 27,
    div256: 256,
    direct: 0,
    stopped: 0,
    follow: 1,
    mod: Array.from({ length: 2 }, () => ({
      mode: 0,
      domain: 0,
      sync: 0,
      retrigger: 1,
      curve: 0,
      offset: 0,
      depth: 2400,
      rate_millihz: 4000,
    })),
  };
}
export function directPlan(period, div256) {
  const available =
    Number.isInteger(period) &&
    period >= 0 &&
    period <= 0xffffffff &&
    Number.isInteger(div256) &&
    div256 >= DIV_MIN &&
    div256 <= DIV_MAX;
  const hz = available ? (SYS_HZ * 256) / (2 * (period + 5) * div256) : 0;
  return { period, div256, hz, available, experimental: hz > 4000000 };
}
export function clockPlan(hz) {
  if (!Number.isFinite(hz) || hz < MIN_HZ || hz > MAX_HZ)
    return { available: false, hz: 0, period: 0, div256: 0 };
  const half = SYS_HZ / (2 * hz),
    cycles = Math.max(5, Math.ceil(half / 65536));
  const x = Math.min(0xffffffff, cycles - 5),
    div = Math.max(
      DIV_MIN,
      Math.min(DIV_MAX, Math.round((half * 256) / (x + 5))),
    );
  let p = directPlan(x, div);
  const integer = Math.round(half);
  if (integer >= 5 && integer <= 0xffffffff + 5) {
    const alt = directPlan(integer - 5, 256);
    if (Math.abs(alt.hz - hz) < Math.abs(p.hz - hz)) p = alt;
  }
  return p;
}
export const basePlan = (c) =>
  c.direct ? directPlan(c.period, c.div256) : clockPlan(c.base_hz);
const u32 = (v) =>
  Array.from(
    { length: 5 },
    (_, i) => Math.floor((v >>> 0) / 2 ** (i * 7)) % 128,
  );
const word = (v) => [v & 127, v >> 7];
function real(v) {
  const b = new Uint8Array(8);
  new DataView(b.buffer).setFloat64(0, v, true);
  return [...b].flatMap(word);
}
function readReal(d, o) {
  const b = new Uint8Array(8);
  for (let i = 0; i < 8; i++) {
    const n = d[o + i * 2] + d[o + i * 2 + 1] * 128;
    if (n > 255) throw Error("Clock float encoding");
    b[i] = n;
  }
  return new DataView(b.buffer).getFloat64(0, true);
}
function wide(d, o) {
  if (d[o + 4] > 15) throw Error("Clock integer encoding");
  return d.slice(o, o + 5).reduce((a, n, i) => a + n * 2 ** (7 * i), 0);
}
function integer(v, min, max) {
  if (!Number.isInteger(v) || v < min || v > max)
    throw Error("Clock parameter range");
}
export function validateClock(c) {
  if (!basePlan(c).available)
    throw Error(`PIOで生成できません（${MIN_HZ}〜${MAX_HZ} Hz）`);
  integer(c.direct, 0, 1);
  integer(c.stopped, 0, 1);
  integer(c.follow, 0, 2);
  if (c.mod?.length !== 2) throw Error("MOD A/B required");
  for (const m of c.mod) {
    integer(m.mode, 0, fxNames.length - 1);
    integer(m.domain, 0, 3);
    integer(m.sync, 0, 8);
    integer(m.retrigger, 0, 1);
    integer(m.curve, 0, 2);
    integer(m.offset, -2147483648, 2147483647);
    integer(m.depth, -2147483648, 2147483647);
    integer(m.rate_millihz, 0, 0xffffffff);
  }
  return c;
}
export function encodeClock(c) {
  validateClock(c);
  return [
    ...real(c.base_hz),
    ...u32(c.period),
    ...u32(c.div256),
    c.direct,
    c.stopped,
    c.follow,
    ...c.mod.flatMap((m) => [
      m.mode,
      m.domain,
      m.sync,
      m.retrigger,
      m.curve,
      ...u32(m.offset),
      ...u32(m.depth),
      ...u32(m.rate_millihz),
    ]),
  ];
}
export function decodeClock(d) {
  if (d.length !== 69) throw Error("Clock schema mismatch");
  const c = {
    base_hz: readReal(d, 0),
    period: wide(d, 16),
    div256: wide(d, 21),
    direct: d[26],
    stopped: d[27],
    follow: d[28],
    mod: [],
  };
  for (let i = 0; i < 2; i++) {
    const o = 29 + i * 20;
    c.mod.push({
      mode: d[o],
      domain: d[o + 1],
      sync: d[o + 2],
      retrigger: d[o + 3],
      curve: d[o + 4],
      offset: wide(d, o + 5) | 0,
      depth: wide(d, o + 10) | 0,
      rate_millihz: wide(d, o + 15),
    });
  }
  return validateClock(c);
}
export function decodeClockStatus(d) {
  if (d.length !== 171) throw Error("Clock diagnostics schema mismatch");
  const floats = new ArrayBuffer(4),
    view = new DataView(floats);
  return {
    fx_flags: d[170],
    base_hz: readReal(d, 0),
    hz: readReal(d, 16),
    period: wide(d, 32),
    div256: wide(d, 37),
    state: ["VALID", "DEGRADED", "UNAVAILABLE", "STOPPED"][d[42]],
    experimental: !!d[43],
    follow: d[44],
    stop_pending: !!d[45],
    clips: d[46],
    pending: d[47],
    wait_us: readReal(d, 48),
    max_wait_us: readReal(d, 64),
    age_us: readReal(d, 80),
    updates: wide(d, 96),
    late: wide(d, 101),
    max_us: wide(d, 106),
    restarts: wide(d, 111),
    desired: Array.from(
      { length: 6 },
      (_, i) => d[116 + i * 2] + 128 * d[117 + i * 2],
    ),
    sent: Array.from(
      { length: 6 },
      (_, i) => d[128 + i * 2] + 128 * d[129 + i * 2],
    ),
    targets: Array.from({ length: 6 }, (_, i) => {
      view.setUint32(0, wide(d, 140 + i * 5), true);
      return view.getFloat32(0, true);
    }),
  };
}
/* Stateful, time-stepped preview. Callback spacing is illustrative; firmware's
   cooperative 250us schedule and chip transfers must be inspected via diagnostics. */
export function clockSimulator(
  c,
  sequence = [0, 50, 100, 25, 75, 100, 50, 0],
  bpm = 120,
) {
  const mods = c.mod.map((_, i) => ({
    phase: 0,
    sweep: 0,
    held: 0,
    smooth: 0,
    walk: 0,
    random: (0x534e7648 + i * 997) >>> 0,
  }));
  function random(m) {
    let n = m.random;
    n ^= n << 13;
    n ^= n >>> 17;
    n ^= n << 5;
    m.random = n >>> 0;
    return ((m.random >>> 16) - 32768) / 32768;
  }
  mods.forEach((m) => {
    m.held = random(m);
  });
  return (dt) => {
    let hz = basePlan(c).hz,
      modulated = false;
    c.mod.forEach((v, i) => {
      const m = mods[i],
        rate = v.sync
          ? (bpm * 24) / 60 / syncClocks[v.sync]
          : v.rate_millihz / 1000;
      const before = m.phase;
      m.phase = (m.phase + rate * dt) % 1;
      m.sweep = Math.min(1, m.sweep + rate * dt);
      if (m.phase < before || rate * dt >= 1) {
        m.held = random(m);
        m.walk = Math.max(-1, Math.min(1, m.walk + m.held / 4));
      }
      m.smooth += (m.held - m.smooth) * Math.min(1, rate * dt);
      const p = m.phase,
        e = m.sweep;
      let w = 0;
      switch (v.mode) {
        case 2:
          w = 2 * e - 1;
          break;
        case 3:
          w = 1 - 4 * Math.abs(p - 0.5);
          break;
        case 4:
          w = p < 0.5 ? -1 : 1;
          break;
        case 5:
          w = m.held;
          break;
        case 6:
          w = sequence[Math.floor(p * 8)] / 50 - 1;
          break;
        case 7:
          w = 1 - 2 * e;
          break;
        case 8:
        case 17:
          w = Math.sin(p * 2 * Math.PI);
          break;
        case 9:
          w = 2 * p - 1;
          break;
        case 10:
          w = m.walk;
          break;
        case 11:
          w = m.smooth;
          break;
        case 12:
          w =
            0.8 * Math.sin(p * 2 * Math.PI) + 0.2 * Math.sin(p * 14 * Math.PI);
          break;
        case 13:
          w = 0.6 * Math.sin(p * 2 * Math.PI) + 0.4 * Math.sin(p * 6 * Math.PI);
          break;
        case 14:
          w =
            2 *
              (v.curve === 1
                ? e * e
                : v.curve === 2
                  ? e * e * (3 - 2 * e)
                  : e) -
            1;
          break;
        case 15:
          w = (p * 8) % 1 < 0.125 ? 1 : -1;
          break;
        case 16:
          w = e >= 1 ? 0 : (p * 8) % 1 < 0.125 ? 1 : -1;
          break;
        case 18:
          w = 1 - 2 * p;
      }
      if (v.mode === 0) return;
      const amount = v.offset + v.depth * w;
      modulated ||= amount !== 0;
      if (v.domain === 0) hz *= 2 ** (amount / 1200);
      else if (v.domain === 1) hz += amount;
      else if (v.domain === 2) hz *= 1 + amount / 1e6;
      else
        hz =
          SYS_HZ /
          (2 * (SYS_HZ / (2 * hz) + (amount * basePlan(c).div256) / 256));
    });
    if (c.stopped) return { hz: 0, available: true, stopped: true };
    return modulated ? clockPlan(hz) : basePlan(c);
  };
}
