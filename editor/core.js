import { fields } from "./schema.js";
export { fields };
export const SCHEMA = 10,
  PROTOCOL = 2;
export const chips = ["SAA1099", "SN76489"];
export const sources = ["Tone", "Noise", "Tone + Noise"];
export const envelopes = ["Off", "ENV oscillator", "Amplitude modulation"];
export const outputs = ["L/R + PAN", "Left only", "Right only"];
export const shapes = [
  "Zero",
  "Maximum",
  "Decay once",
  "Saw down",
  "Triangle once",
  "Triangle",
  "Rise once",
  "Saw up",
];
export const destinations = [
  "Off",
  "Level",
  "Pan",
  "Pitch",
  "Vibrato",
  "Tremolo",
  "Layer level",
  "ENV ratio",
  "Noise pitch",
  "SN clock base",
  "SN clock depth",
  "Morph",
];
export function cleanName(s) {
  return String(s)
    .toUpperCase()
    .replace(/[^ -Z]/g, " ")
    .padEnd(8, " ")
    .slice(0, 8);
}
export function validatePatch(p) {
  for (const f of fields)
    if (!Number.isInteger(p[f.name]) || p[f.name] < f.min || p[f.name] > f.max)
      throw Error(`${f.label}: 範囲外です`);
  if (![8, 16].includes(p.env_steps)) throw Error("ENV steps は8/16です");
  if (p.chip === 0) {
    if (p.env === 1 && p.source !== 0)
      throw Error("ENV発振はToneを選んでください");
    if (p.layers > 1 && (p.layers !== 2 || p.env !== 1))
      throw Error("SAA layerにはENV oscillatorが必要です");
  } else if (
    p.env ||
    (p.source !== 0 && p.layers !== 1) ||
    (p.source === 2 && p.noise_rate !== 3)
  )
    throw Error("SNのENV/Layer/Noise設定が不正です");
  if (
    (p.noise_pitch || p.noise_sweep) &&
    (p.noise_rate !== 3 ||
      (p.source === 0 && !(p.chip === 0 && p.layers > 1 && p.layer_source)))
  )
    throw Error("Noise sweepにはKey Trackが必要です");
  return p;
}
export function normalizePatch(p, changed) {
  if (changed === "source" && p.source !== 0) {
    p.layers = 1;
    if (p.env === 1) p.env = 2;
  }
  if (changed === "env" && p.env !== 1 && p.chip === 0) p.layers = 1;
  if (p.chip === 1) {
    p.env = 0;
    if (p.source !== 0) p.layers = 1;
    if (p.source === 2) p.noise_rate = 3;
  }
  if (p.chip === 0 && p.layers > 1) {
    p.layers = 2;
    p.env = 1;
    p.source = 0;
  }
  if (p.env === 1) p.source = 0;
  if (changed === "source" && p.source !== 0 && p.env === 1) p.env = 2;
  if (
    p.noise_rate !== 3 ||
    (p.source === 0 && !(p.chip === 0 && p.layers > 1 && p.layer_source !== 0))
  )
    p.noise_pitch = p.noise_sweep = 0;
  return validatePatch(p);
}
export function sameStructure(a, b) {
  if (
    !["chip", "source", "env", "output", "layers"].every((k) => a[k] === b[k])
  )
    return false;
  if (a.chip === 0 && a.layers > 1 && a.layer_source !== b.layer_source)
    return false;
  if (
    a.env &&
    !["shape", "env_steps", "env_invert"].every((k) => a[k] === b[k])
  )
    return false;
  const noise =
    a.source !== 0 || (a.chip === 0 && a.layers > 1 && a.layer_source !== 0);
  return (
    !noise ||
    (a.noise_rate === b.noise_rate &&
      (a.chip === 0 || a.noise_type === b.noise_type))
  );
}
export const Wave = Object.freeze({
  SQUARE: 0,
  PULSE: 1,
  ENV: 2,
  TONE_NOISE: 3,
  NOISE: 4,
  METAL: 5,
  AM: 6,
});
export const waveNames = [
  "SQUARE 50%",
  "PULSE ~6.7%",
  "ENV WAVE",
  "TONE + NOISE",
  "NOISE",
  "SN METAL",
  "SAA AM",
];
export const Feature = Object.freeze({
  PITCH: 1,
  PAN: 2,
  ENV: 4,
  ENV_RATIO: 8,
  NOISE: 16,
  NOISE_TYPE: 32,
  NOISE_TRACK: 64,
  NOISE_MIX: 128,
  LAYERS: 256,
  LAYER_PITCH: 512,
  STACK: 1024,
  SAA_LAYER: 2048,
  DETUNE: 4096,
  PULSE: 8192,
});
export function waveform(p) {
  if (p.chip === 0 && p.env) return p.env === 1 ? Wave.ENV : Wave.AM;
  if (p.source === 2) return p.chip === 0 ? Wave.TONE_NOISE : Wave.METAL;
  if (p.source === 1)
    return p.chip === 1 && p.noise_rate === 3 && !p.noise_type
      ? Wave.PULSE
      : Wave.NOISE;
  return Wave.SQUARE;
}
export function availableWaves(chip) {
  if (chip === 0)
    return [Wave.SQUARE, Wave.ENV, Wave.TONE_NOISE, Wave.NOISE, Wave.AM];
  if (chip === 1) return [Wave.SQUARE, Wave.PULSE, Wave.NOISE, Wave.METAL];
  return [];
}
export function selectWave(p, wave) {
  if (!availableWaves(p.chip).includes(wave))
    throw Error("このチップでは使えない発音です");
  validatePatch(p);
  if (waveform(p) === wave) return p;
  const q = {
    ...p,
    env: 0,
    source: 0,
    layers: 1,
    noise_pitch: 0,
    noise_sweep: 0,
  };
  if (wave === Wave.PULSE || wave === Wave.METAL) {
    q.source = wave === Wave.PULSE ? 1 : 2;
    q.noise_rate = 3;
    q.noise_type = 0;
    q.noise_mix = 50;
  } else if (wave === Wave.ENV || wave === Wave.AM) {
    q.env = wave === Wave.ENV ? 1 : 2;
    if (q.shape < 2) q.shape = 5;
  } else if (wave === Wave.TONE_NOISE || wave === Wave.NOISE) {
    q.source = wave === Wave.TONE_NOISE ? 2 : 1;
    q.noise_rate = 1;
    q.noise_type = 1;
  }
  validatePatch(q);
  Object.assign(p, q);
  return p;
}
export function featureMask(p) {
  const noise =
    p.source !== 0 || (p.chip === 0 && p.layers > 1 && p.layer_source !== 0);
  const tracked = noise && p.noise_rate === 3;
  const layerPitch =
    p.layers > 1 && (p.chip === 1 || p.layer_source !== 1 || tracked);
  const pitch =
    (p.source !== 1 && p.env !== 1) ||
    (p.env !== 0 && p.shape >= 2) ||
    tracked ||
    layerPitch;
  let f = pitch ? Feature.PITCH : 0;
  if (p.output === 0) f |= Feature.PAN;
  if (p.chip === 0 && p.env) f |= Feature.ENV;
  if (p.chip === 0 && p.env === 2 && p.shape >= 2) f |= Feature.ENV_RATIO;
  if (noise) f |= Feature.NOISE;
  if (noise && p.chip === 1) f |= Feature.NOISE_TYPE;
  if (tracked) f |= Feature.NOISE_TRACK;
  if (p.chip === 1 && p.source === 2) f |= Feature.NOISE_MIX;
  if ((p.chip === 1 && p.source === 0) || (p.chip === 0 && p.env === 1))
    f |= Feature.LAYERS;
  if (layerPitch) f |= Feature.LAYER_PITCH;
  if (p.chip === 1 && p.layers === 3) f |= Feature.STACK;
  if (p.chip === 0 && p.layers > 1) f |= Feature.SAA_LAYER;
  if (p.chip === 1 && p.output === 0 && pitch) f |= Feature.DETUNE;
  if (waveform(p) === Wave.PULSE) f |= Feature.PULSE;
  return f;
}
export function description(p) {
  return `${chips[p.chip]} · ${waveNames[waveform(p)]}${p.env === 2 ? ` · ${sources[p.source]}` : ""}${p.layers > 1 ? ` + ${p.layers - 1} layer` : ""}${p.output ? ` · ${outputs[p.output]}` : ""}`;
}
export function resourceInfo(p, mode = 0, voices = 1) {
  const n = mode === 1 ? voices : 1;
  let per =
    p.chip === 0
      ? (p.env ? 2 : 1) + (p.layers > 1 ? 1 : 0)
      : p.source === 0
        ? p.layers * (p.output === 0 ? 2 : 1)
        : (p.output === 0 ? 2 : 1) *
          (p.noise_rate === 3 || p.source === 2 ? 2 : 1);
  if (
    p.chip === 0 &&
    (p.source !== 0 || (p.layers > 1 && p.layer_source)) &&
    p.noise_rate === 3
  ) {
    if (!(p.layers > 1)) per++;
  }
  return {
    cost: `${n}音 · 最大${per * n}回路`,
    detail:
      p.chip === 0
        ? "SAAノイズ速度は3声単位で共有。ENVは出力3/6とクロック2/5を使用します。"
        : "SNの外部クロックは左右の全トーン・ノイズで共有。Key Track / MetalはTone 3も使用します。",
  };
}
export function supported(p, name) {
  const f = featureMask(p);
  if (
    [
      "chip",
      "source",
      "output",
      "attack",
      "decay",
      "release",
      "sustain",
      "level",
      "tremolo",
      "lfo_rate",
      "lfo_shape",
    ].includes(name)
  )
    return true;
  if (
    [
      "pitch_decay",
      "pitch_amount",
      "glide",
      "transpose",
      "fine",
      "vibrato",
    ].includes(name)
  )
    return !!(f & Feature.PITCH);
  if (name === "env") return p.chip === 0;
  const native = {
    shape: Feature.ENV,
    env_steps: Feature.ENV,
    env_invert: Feature.ENV,
    retrigger: Feature.ENV,
    env_ratio: Feature.ENV_RATIO,
    layers: Feature.LAYERS,
    layer_source: Feature.SAA_LAYER,
    layer_level: Feature.SAA_LAYER,
    layer_decay: Feature.SAA_LAYER,
    layer_interval: Feature.LAYER_PITCH,
    stack_interval: Feature.STACK,
    noise_type: Feature.NOISE_TYPE,
    noise_mix: Feature.NOISE_MIX,
    noise_rate: Feature.NOISE,
    noise_pitch: Feature.NOISE_TRACK,
    noise_sweep: Feature.NOISE_TRACK,
    detune: Feature.DETUNE,
  };
  return !!(f & (native[name] || 0));
}
export function musicalParameters(p) {
  const names = ["shape", "env_steps", "env_invert", "retrigger", "env_ratio"];
  if (waveform(p) !== Wave.PULSE) names.push("noise_type", "noise_rate");
  names.push(
    "noise_mix",
    "noise_pitch",
    "noise_sweep",
    "layers",
    "layer_source",
    "layer_interval",
    "stack_interval",
    "layer_level",
    "layer_decay",
    "attack",
    "decay",
    "sustain",
    "release",
    "level",
    "pitch_amount",
    "pitch_decay",
    "glide",
    "vibrato",
    "tremolo",
    "lfo_rate",
    "lfo_shape",
    "detune",
  );
  return names.filter((n) => supported(p, n));
}
export function word(n) {
  if (!Number.isInteger(n) || n < 0 || n > 16383) throw Error("14-bit range");
  return [n & 127, n >> 7];
}
export const signed = (n) => word(n + 8192);
export const readWord = (d, i = 0) => d[i] | (d[i + 1] << 7);
export const maskBytes = (n) => [n & 127, (n >> 7) & 127, (n >> 14) & 3];
export function frame(cmd, id, payload = []) {
  if (
    !Number.isInteger(cmd) ||
    cmd < 0 ||
    cmd > 63 ||
    !Number.isInteger(id) ||
    id < 0 ||
    id > 127 ||
    payload.some((x) => !Number.isInteger(x) || x < 0 || x > 127)
  )
    throw Error("Invalid SysEx payload");
  const b = [125, 67, 84, PROTOCOL, id, cmd, ...payload];
  return Uint8Array.from([240, ...b, -b.reduce((a, v) => a + v, 0) & 127, 247]);
}
export function parseFrame(bytes) {
  const b = Array.from(bytes);
  if (
    b.length < 10 ||
    b[0] !== 240 ||
    b.at(-1) !== 247 ||
    b[1] !== 125 ||
    b[2] !== 67 ||
    b[3] !== 84 ||
    b[4] !== PROTOCOL ||
    b.slice(1, -1).some((x) => x > 127) ||
    b.slice(1, -1).reduce((a, v) => a + v, 0) % 128
  )
    throw Error("対応外の応答、またはチェックサム不一致");
  return { id: b[5], command: b[6] & 63, status: b[7], data: b.slice(8, -2) };
}
export function verifyHello(h) {
  if (
    h.length !== 11 ||
    h[0] !== 0 ||
    h[1] !== 10 ||
    h[3] !== PROTOCOL ||
    h[4] !== SCHEMA ||
    h[10] !== fields.length
  )
    throw Error(
      "v0.10 / protocol 2 / schema 10 専用です。書き込みを無効化しました。",
    );
  return h;
}
export function patchBytes(p) {
  validatePatch(p);
  return [fields.length, ...fields.flatMap((f) => signed(p[f.name]))];
}
export function decodePatch(d, index = 0) {
  if (d[index] !== fields.length || d.length < index + 1 + fields.length * 2)
    throw Error("音色スキーマ不一致");
  return validatePatch(
    Object.fromEntries(
      fields.map((f, i) => [f.name, readWord(d, index + 1 + i * 2) - 8192]),
    ),
  );
}
export function validateTrack(t) {
  const controls = {
    track: [0, 13],
    enabled: [0, 1],
    mode: [0, 2],
    voices: [1, 6],
    level: [0, 100],
    pan: [0, 100],
    mute: [0, 1],
    low: [0, 127],
    high: [0, 127],
    velocity_curve: [0, 3],
    morph_target: [0, 16],
    morph: [0, 100],
  };
  for (const [k, [lo, hi]] of Object.entries(controls))
    if (!Number.isInteger(t[k]) || t[k] < lo || t[k] > hi)
      throw Error(`Track ${k}: 範囲外`);
  if (t.low > t.high) throw Error("音域の上下が逆です");
  for (const k of ["usb", "trs"])
    if (!Number.isInteger(t[k]) || t[k] < 0 || t[k] > 65535)
      throw Error("MIDI route mask");
  if (
    t.macro_dest?.length !== 4 ||
    t.macro?.length !== 4 ||
    t.macro_dest.some(
      (x) => !Number.isInteger(x) || x < 0 || x >= destinations.length,
    ) ||
    t.macro.some((x) => !Number.isInteger(x) || x < -100 || x > 100)
  )
    throw Error("Macro設定");
  validatePatch(t.patch);
  return t;
}
export function encodeTrack(t) {
  validateTrack(t);
  return [
    t.track,
    t.enabled,
    t.mode,
    t.voices,
    t.level,
    t.pan,
    t.mute,
    t.low,
    t.high,
    t.velocity_curve,
    t.morph_target,
    t.morph,
    ...[...cleanName(t.name)].map((c) => c.charCodeAt(0)),
    ...t.macro_dest,
    ...t.macro.flatMap(signed),
    ...maskBytes(t.usb),
    ...maskBytes(t.trs),
    ...patchBytes(t.patch),
  ];
}
export function decodeTrack(d) {
  if (d.length !== 43 + fields.length * 2)
    throw Error(`Track response length ${d.length}`);
  const t = {
    track: d[0],
    enabled: d[1],
    mode: d[2],
    voices: d[3],
    level: d[4],
    pan: d[5],
    mute: d[6],
    low: d[7],
    high: d[8],
    velocity_curve: d[9],
    morph_target: d[10],
    morph: d[11],
    name: String.fromCharCode(...d.slice(12, 20)).trim(),
    macro_dest: d.slice(20, 24),
    macro: Array.from({ length: 4 }, (_, i) => readWord(d, 24 + i * 2) - 8192),
    usb: readWord(d, 32) | (d[34] << 14),
    trs: readWord(d, 35) | (d[37] << 14),
    patch: decodePatch(d, 38),
    outputs: readWord(d, 39 + fields.length * 2),
    resources: readWord(d, 41 + fields.length * 2),
  };
  return validateTrack(t);
}
export function newTrack(index, factories) {
  const f = factories[index < 6 ? 20 : index < 9 ? 21 : 22];
  return {
    track: index,
    enabled: index < 10 ? 1 : 0,
    mode: 0,
    voices: 1,
    level: 100,
    pan: 50,
    mute: 0,
    low: 0,
    high: 127,
    velocity_curve: 0,
    morph_target: 0,
    morph: 0,
    name: f.name.trim(),
    patch: structuredClone(f.patch),
    macro_dest: [4, 5, 2, 1],
    macro: [0, 0, 0, 0],
    usb: index < 10 ? 1 << index : 0,
    trs: index < 10 ? 1 << index : 0,
  };
}
export function newSequence(track) {
  return {
    track,
    enabled: 0,
    length: 16,
    division: 6,
    direction: 0,
    arp: 0,
    arp_octaves: 1,
    note: Array.from({ length: 16 }, (_, i) => 45 + (i % 4) * 3),
    gate: Array(16).fill(70),
    velocity: Array(16).fill(100),
    motion: Array(16).fill(0),
  };
}
export function encodeSequence(q) {
  const ranges = {
    track: [0, 13],
    enabled: [0, 1],
    length: [1, 16],
    division: [1, 24],
    direction: [0, 2],
    arp: [0, 5],
    arp_octaves: [1, 4],
  };
  for (const [k, [lo, hi]] of Object.entries(ranges))
    if (!Number.isInteger(q[k]) || q[k] < lo || q[k] > hi)
      throw Error(`Sequence ${k}`);
  for (const [k, lo, hi] of [
    ["note", 0, 127],
    ["gate", 0, 100],
    ["velocity", 0, 127],
    ["motion", -100, 100],
  ])
    if (
      q[k]?.length !== 16 ||
      q[k].some((x) => !Number.isInteger(x) || x < lo || x > hi)
    )
      throw Error(`Sequence ${k}`);
  return [
    q.track,
    q.enabled,
    q.length,
    q.division,
    q.direction,
    q.arp,
    q.arp_octaves,
    ...q.note,
    ...q.gate,
    ...q.velocity,
    ...q.motion.flatMap(signed),
  ];
}
export function decodeSequence(d) {
  if (d.length !== 87) throw Error("Sequence response");
  return {
    track: d[0],
    enabled: d[1],
    length: d[2],
    division: d[3],
    direction: d[4],
    arp: d[5],
    arp_octaves: d[6],
    note: d.slice(7, 23),
    gate: d.slice(23, 39),
    velocity: d.slice(39, 55),
    motion: Array.from(
      { length: 16 },
      (_, i) => readWord(d, 55 + i * 2) - 8192,
    ),
  };
}
export const newCv = (i) => ({
  input: i,
  mode: 0,
  track: i,
  base_note: 45,
  scale: 0,
  root: 0,
  glide: 0,
  range: 0,
  gate: 0,
  gate_track: i,
  parameter: 0,
  gate_note: 45,
  amount: 100,
});
export function encodeCv(c) {
  const max = {
    input: 1,
    mode: 2,
    track: 13,
    base_note: 127,
    scale: 7,
    root: 11,
    glide: 7,
    range: 1,
    gate: 4,
    gate_track: 13,
    parameter: 11,
    gate_note: 127,
  };
  for (const [k, hi] of Object.entries(max))
    if (!Number.isInteger(c[k]) || c[k] < 0 || c[k] > hi)
      throw Error(`CV ${k}`);
  if (!Number.isInteger(c.amount) || Math.abs(c.amount) > 100)
    throw Error("CV amount");
  return [
    c.input,
    c.mode,
    c.track,
    c.base_note,
    c.scale,
    c.root,
    c.glide,
    c.range,
    c.gate,
    c.gate_track,
    c.parameter,
    c.gate_note,
    ...signed(c.amount),
  ];
}
export function decodeCv(d) {
  if (d.length !== 14) throw Error("CV response");
  const keys = [
    "input",
    "mode",
    "track",
    "base_note",
    "scale",
    "root",
    "glide",
    "range",
    "gate",
    "gate_track",
    "parameter",
    "gate_note",
  ];
  return {
    ...Object.fromEntries(keys.map((k, i) => [k, d[i]])),
    amount: readWord(d, 12) - 8192,
  };
}
export function exportTrack(t, q) {
  return {
    schema: SCHEMA,
    kind: "chiptune-track",
    track: structuredClone(validateTrack(t)),
    sequence: structuredClone(q),
  };
}
export function importTrack(data, index) {
  if (data.schema !== SCHEMA || data.kind !== "chiptune-track")
    throw Error("v0.10 Track JSON専用です");
  const t = structuredClone(data.track),
    q = structuredClone(data.sequence);
  t.track = q.track = index;
  validateTrack(t);
  encodeSequence(q);
  return { track: t, sequence: q };
}
export function outputNames(mask) {
  return (
    Array.from({ length: 14 }, (_, v) => v)
      .filter((v) => mask & (1 << v))
      .map((v) =>
        v < 6
          ? `SAA ${v + 1}`
          : v < 9
            ? `SN L${v - 5}`
            : v < 12
              ? `SN R${v - 8}`
              : v === 12
                ? "SN L noise"
                : "SN R noise",
      )
      .join(" / ") || "なし"
  );
}

export function encodeGlobal(v) {
  if (
    !Array.isArray(v) ||
    v.length !== 25 ||
    v.some(
      (n, i) => !Number.isInteger(n) || n < 0 || n > (i === 10 ? 210 : 127),
    )
  )
    throw Error("Global settings");
  return [...v.slice(0, 10), ...signed(v[10] - 8192), ...v.slice(11)];
}
export function decodeGlobal(d) {
  if (d.length !== 26) throw Error("Global schema mismatch");
  const v = [...d.slice(0, 10), readWord(d, 10), ...d.slice(12)];
  encodeGlobal(v);
  return v;
}

export function supportedMod(t, dest) {
  const p = t.patch;
  if (dest === 2) return p.output === 0;
  if (dest === 3 || dest === 4) return supported(p, "pitch_amount");
  if (dest === 6) return p.chip === 0 && p.layers > 1;
  if (dest === 7) return supported(p, "env_ratio");
  if (dest === 8) return supported(p, "noise_pitch");
  if (dest === 11) return !!t.morph_target;
  return true;
}
