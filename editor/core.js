import { fields } from "./schema.js";
export { fields };
export const models = [
  "SAA PULSE",
  "SAA NOISE",
  "SAA TONE + NOISE",
  "SAA ENV OSC",
  "SAA ENV × TONE",
  "SN STEREO PULSE",
  "SN LEFT PULSE",
  "SN RIGHT PULSE",
  "SN STEREO NOISE",
  "SN TONE + PERIODIC",
  "SN LEFT NOISE",
  "SN RIGHT NOISE",
  "SAA ENV × NOISE",
  "SAA ENV × MIX",
  "SAA LAYER",
  "SN STEREO STACK",
];
export const shapes = [
  "ZERO",
  "MAX",
  "DECAY ONCE",
  "SAW DOWN",
  "TRIANGLE ONCE",
  "TRIANGLE",
  "ATTACK ONCE",
  "SAW UP",
];
export const parameters = [
  "OFF",
  "WAVE",
  "STEPS",
  "R INVERT",
  "PAN",
  "VIBRATO",
  "DETUNE",
  "TONE MIX",
  "ATTACK",
  "DECAY",
  "RELEASE",
  "TREMOLO",
  "LEVEL",
  "PITCH",
  "SN BASE ALL",
  "SN DEPTH ALL",
  "LAYER LEVEL",
  "LAYER DECAY",
  "ENV RATIO",
  "LAYER PITCH",
];
export const isSaa = (m) => m <= 4 || (m >= 12 && m <= 14);
export const isEnv = (m) => [3, 4, 12, 13, 14].includes(m);
export const isNoise = (m) => [1, 2, 8, 9, 10, 11, 12, 13].includes(m);
export const limit = (m) =>
  m === 15 || (m >= 8 && m <= 11) ? 1 : isEnv(m) ? 2 : isSaa(m) ? 6 : 3;
export const roleFor = (m) =>
  isEnv(m) ? 2 : isSaa(m) ? 1 : isNoise(m) ? 4 : 3;
export function fromValues(values) {
  return Object.fromEntries(fields.map((f, i) => [f.name, values[i]]));
}
export function valuesOf(sound) {
  return fields.map((f) => sound[f.name]);
}
export function validateSound(sound) {
  for (const f of fields)
    if (
      !Number.isInteger(sound[f.name]) ||
      sound[f.name] < f.min ||
      sound[f.name] > f.max
    )
      throw Error(`${f.label}: 範囲外です`);
  if (![8, 16].includes(sound.steps)) throw Error("ENV levels は8または16です");
  if (
    fields
      .filter((f) => f.name.startsWith("macro_dest"))
      .some((f) => sound[f.name] >= parameters.length)
  )
    throw Error("Macro destination が不正です");
  return sound;
}
export function word(value) {
  if (!Number.isInteger(value) || value < 0 || value > 16383)
    throw Error("14-bit range");
  return [value & 127, value >> 7];
}
export function signed(value) {
  return word(value + 8192);
}
export function readWord(bytes, index = 0) {
  if (index + 1 >= bytes.length) throw Error("Truncated word");
  return bytes[index] | (bytes[index + 1] << 7);
}
export function frame(command, id, payload = []) {
  if (
    command < 0 ||
    command > 127 ||
    id < 0 ||
    id > 127 ||
    payload.some((v) => !Number.isInteger(v) || v < 0 || v > 127)
  )
    throw Error("Invalid SysEx byte");
  const b = [0x7d, 0x43, 0x54, 1, id, command, ...payload];
  return new Uint8Array([
    0xf0,
    ...b,
    -b.reduce((a, v) => a + v, 0) & 127,
    0xf7,
  ]);
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
    b[4] !== 1 ||
    b.slice(1, -1).some((v) => v < 0 || v > 127) ||
    b.slice(1, -1).reduce((a, v) => a + v, 0) % 128
  )
    throw Error("不正な応答です");
  return { id: b[5], command: b[6], status: b[7], data: b.slice(8, -2) };
}
export function soundBytes(sound) {
  validateSound(sound);
  return [fields.length, ...valuesOf(sound).flatMap(signed)];
}
export function decodeSound(data, offset = 0) {
  if (
    data[offset] !== fields.length ||
    data.length < offset + 1 + 2 * fields.length
  )
    throw Error("音色形式が違います。v0.8以降のファームが必要です");
  return validateSound(
    fromValues(fields.map((_, i) => readWord(data, offset + 1 + 2 * i) - 8192)),
  );
}
export function cleanName(name) {
  return name
    .toUpperCase()
    .replace(/[^ -Z]/g, " ")
    .slice(0, 8)
    .padEnd(8, " ");
}
export function encodePart(p) {
  if (
    !Number.isInteger(p.part) ||
    p.part < 0 ||
    p.part > 13 ||
    ![0, 1].includes(p.enabled) ||
    !Number.isInteger(p.poly) ||
    p.poly < 1 ||
    p.poly > limit(p.sound.model) ||
    !Number.isInteger(p.role) ||
    p.role < 0 ||
    p.role > 4 ||
    !Number.isInteger(p.channel) ||
    (p.channel > 15 && p.channel !== 127) ||
    p.channel < 0
  )
    throw Error("パート設定が不正です");
  return [
    p.part,
    p.enabled,
    p.poly,
    p.role,
    p.channel,
    ...[...cleanName(p.name)].map((c) => c.charCodeAt(0)),
    ...soundBytes(p.sound),
  ];
}
export function decodePart(data) {
  if (data.length !== 18 + 2 * fields.length)
    throw Error("パート応答の長さが違います");
  return {
    part: data[0],
    enabled: data[1],
    poly: data[2],
    role: data[3],
    channel: data[4],
    name: String.fromCharCode(...data.slice(5, 13)).trim(),
    sound: decodeSound(data, 13),
    outputs: readWord(data, 14 + 2 * fields.length),
    resources: readWord(data, 16 + 2 * fields.length),
  };
}
export function visibleFields(sound) {
  const m = sound.model,
    env = isEnv(m),
    noise = isNoise(m),
    sn = !isSaa(m);
  return fields.filter((f) => {
    const n = f.name;
    if (n === "model") return false;
    if (n === "pan" && [6, 7, 10, 11].includes(m)) return false;
    if (["shape", "steps", "invert", "retrigger"].includes(n))
      return env || (n === "shape" && sn && noise);
    if (n === "noise_rate")
      return noise || (m === 14 && sound.layer_source > 0);
    if (n === "noise_mix") return m === 9;
    if (n === "detune") return [5, 8, 9, 15].includes(m);
    if (n === "env_ratio") return [4, 12, 13].includes(m);
    if (n === "layer_source" || n === "layer_level" || n === "layer_decay")
      return m === 14;
    if (n === "layer_interval")
      return (
        m === 15 ||
        (m === 14 && (sound.layer_source !== 1 || sound.noise_rate === 3))
      );
    if (n === "stack_size") return m === 15;
    if (n === "stack_interval") return m === 15 && sound.stack_size === 3;
    return true;
  });
}
export function resourceInfo(sound, poly) {
  const m = sound.model,
    noise = isNoise(m),
    voices = sound.mode ? 1 : poly;
  if (m === 15)
    return {
      cost: `SN ${sound.stack_size}組 / stereo × 1音`,
      detail: `左右で${sound.stack_size * 2}トーンを使用。3組ではトーン3連動ノイズと競合します。`,
    };
  if (m === 14)
    return {
      cost: `SAA ${3 * poly}/6回路 · ${voices}音`,
      detail:
        "1音につきENV出力＋ENVクロック＋レイヤー。レイヤーの減衰は全体ADSRに追加でかかります。",
    };
  if (isEnv(m)) {
    const cost = [12, 13].includes(m) && sound.noise_rate === 3 ? 3 : 2;
    return {
      cost: `SAA ${cost * poly}/6回路 · ${voices}音`,
      detail:
        "ENVクロック用の回路を予約します。純ENVの最高音は形状・ステップ数で変わります。",
    };
  }
  if (isSaa(m))
    return {
      cost: `SAA ${poly}/6出力 · ${voices}音`,
      detail: noise
        ? "ノイズのレートは3チャンネルのグループで共有。KEY TRACKは追加クロック回路を使う場合があります。"
        : "1回路で左右の音量を独立制御できます。",
    };
  if (noise)
    return {
      cost: "SN stereo noise × 1音",
      detail:
        "KEY TRACK / METALは左右のトーン3も予約します。全SNの外部クロックは共通です。",
    };
  return {
    cost: `SN ${m === 5 ? poly + "組" : poly + "回路"} · ${voices}音`,
    detail: "外部クロック操作は左右の全トーン・ノイズに影響します。",
  };
}
export function envValue(shape, phase, steps = 16) {
  const p = Math.max(0, Math.min(1, phase));
  let v;
  switch (shape) {
    case 0:
      v = 0;
      break;
    case 1:
      v = 1;
      break;
    case 2:
    case 3:
      v = 1 - p;
      break;
    case 4:
    case 5:
      v = 1 - Math.abs(2 * p - 1);
      break;
    default:
      v = p;
  }
  return Math.round(v * (steps - 1)) / (steps - 1);
}
export function waveform(sound, phase, right = false) {
  const m = sound.model,
    p = phase % 1;
  const density = [127, 71, 37, 71][sound.noise_rate];
  let noise = Math.sin(Math.floor(phase * density) * 127.1 + 311.7) > 0 ? 1 : 0;
  if ([8, 9, 10, 11].includes(m) && sound.shape === 0)
    noise = p < 1 / 15 ? 1 : 0;
  const carrier =
    isEnv(m) && ![3, 14].includes(m)
      ? (phase *
          (sound.shape === 4 || sound.shape === 5
            ? sound.steps
            : sound.steps / 2)) /
        2 ** (sound.env_ratio / 12)
      : phase;
  const pulse = carrier % 1 < 0.5 ? 1 : 0;
  let env = envValue(sound.shape, p, sound.steps);
  if (right && sound.invert) env = 1 - env;
  if (m === 3 || m === 14) return env;
  if (m === 4) return pulse * env;
  if (m === 12) return noise * env;
  if (m === 13) return ((pulse + noise) / 2) * env;
  if (m === 1 || [8, 10, 11].includes(m)) return noise;
  if (m === 2) return (pulse + noise) / 2;
  if (m === 9)
    return (
      (pulse * sound.noise_mix) / 100 + noise * (1 - sound.noise_mix / 100)
    );
  if (m === 15) {
    let v = 0;
    for (let i = 0; i < sound.stack_size; i++)
      v +=
        (phase *
          2 **
            ((i === 1
              ? sound.layer_interval
              : i === 2
                ? sound.stack_interval
                : 0) /
              12)) %
          1 <
        0.5
          ? 1
          : 0;
    return v / sound.stack_size;
  }
  return pulse;
}
export function validateMotion(m) {
  for (const [k, lo, hi] of [
    ["part", 0, 13],
    ["length", 1, 16],
    ["division", 1, 24],
    ["direction", 0, 2],
    ["sequence", 0, 1],
    ["arp", 0, 5],
    ["arp_octaves", 1, 4],
    ["running", 0, 1],
  ])
    if (!Number.isInteger(m[k]) || m[k] < lo || m[k] > hi)
      throw Error("Motion 設定が不正です");
  for (const [k, lo, hi] of [
    ["note", 0, 127],
    ["gate", 0, 100],
    ["velocity", 0, 127],
    ["motion", -100, 100],
  ])
    if (
      !Array.isArray(m[k]) ||
      m[k].length !== 16 ||
      m[k].some((v) => !Number.isInteger(v) || v < lo || v > hi)
    )
      throw Error("Motion ステップが不正です");
  return m;
}
export function encodeMotion(m) {
  validateMotion(m);
  return [
    m.part,
    m.length,
    m.division,
    m.direction,
    m.sequence,
    m.arp,
    m.arp_octaves,
    m.running,
    ...m.note,
    ...m.gate,
    ...m.velocity,
    ...m.motion.flatMap(signed),
  ];
}
export function decodeMotion(d) {
  if (d.length !== 88) throw Error("Motion 応答が不正です");
  return validateMotion({
    part: d[0],
    length: d[1],
    division: d[2],
    direction: d[3],
    sequence: d[4],
    arp: d[5],
    arp_octaves: d[6],
    running: d[7],
    note: d.slice(8, 24),
    gate: d.slice(24, 40),
    velocity: d.slice(40, 56),
    motion: Array.from(
      { length: 16 },
      (_, i) => readWord(d, 56 + i * 2) - 8192,
    ),
  });
}
export function exportPatch(part, motion) {
  return {
    format: "chiptune-wave-lab",
    version: 1,
    part: { ...part, outputs: undefined, resources: undefined },
    motion,
  };
}
export function importPatch(data) {
  if (data?.format !== "chiptune-wave-lab" || data.version !== 1)
    throw Error("対応しないファイル形式です");
  encodePart(data.part);
  if (data.motion) validateMotion(data.motion);
  return structuredClone(data);
}

export function layerWaveform(sound, phase, note = 48) {
  const hz = 440 * 2 ** ((note - 69) / 12),
    elapsed = (phase / hz) * 1000;
  const p = (phase * 2 ** (sound.layer_interval / 12)) % 1;
  const noise =
    Math.sin(
      Math.floor(phase * [127, 71, 37, 71][sound.noise_rate]) * 127.1 + 311.7,
    ) > 0
      ? 1
      : 0;
  const pulse = p < 0.5 ? 1 : 0;
  const value =
    sound.layer_source === 0
      ? pulse
      : sound.layer_source === 1
        ? noise
        : (pulse + noise) / 2;
  return (
    ((value * sound.layer_level) / 100) *
    (sound.layer_decay ? Math.exp(-elapsed / sound.layer_decay) : 1)
  );
}
