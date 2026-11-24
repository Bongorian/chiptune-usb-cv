import {
  fields,
  SCHEMA,
  PROTOCOL,
  chips,
  sources,
  envelopes,
  outputs,
  shapes,
  destinations,
  cleanName,
  encodeGlobal,
  decodeGlobal,
  validatePatch,
  normalizePatch,
  description,
  resourceInfo,
  supported,
  supportedMod,
  Wave,
  waveform,
  waveNames,
  availableWaves,
  selectWave,
  musicalParameters,
  frame,
  parseFrame,
  verifyHello,
  decodePatch,
  encodeTrack,
  decodeTrack,
  newTrack,
  newSequence,
  encodeSequence,
  decodeSequence,
  newCv,
  encodeCv,
  decodeCv,
  exportTrack,
  importTrack,
  outputNames,
  signed,
  readWord,
} from "./core.js";
const $ = (id) => document.getElementById(id);
const catalog = await fetch("./factory.json").then((r) => {
  if (!r.ok) throw Error("Factory catalog");
  return r.json();
});
if (catalog.schema !== SCHEMA || catalog.protocol !== PROTOCOL)
  throw Error("Factory catalog version mismatch");
const factory = catalog.factories;
const cache = new Map(
  Array.from({ length: 14 }, (_, t) => [
    t,
    {
      track: newTrack(t, factory),
      sequence: newSequence(t),
      dirty: false,
      sequenceDirty: false,
      rev: 0,
    },
  ]),
);
let selected = 0,
  ready = false,
  access,
  input,
  output,
  pending = null,
  requestId = 1,
  serial = Promise.resolve(),
  liveTimer = null,
  sending = false;
let global = [
  1, 0, 0, 24, 4, 0, 1, 7, 0, 0, 90, 0, 70, 100, 100, 100, 0, 50, 100, 25, 75,
  100, 50, 0, 0,
];
let cvs = [newCv(0), newCv(1)],
  matrix = Array.from({ length: 8 }, () => ({
    source: 0,
    track: 0,
    parameter: 0,
    amount: 0,
  }));
let globalDirty = false,
  cvDirty = false,
  matrixDirty = false,
  replaceAction = null;
const entry = () => cache.get(selected);
function message(text, error = false) {
  $("message").textContent = text;
  $("message").classList.toggle("error", error);
}
function connected() {
  $("connection-state").textContent = ready
    ? "実機接続中 · v0.10"
    : "実機未接続（無音）";
  $("connection-state").classList.toggle("connected", ready);
  for (const id of [
    "poly-focus",
    "read",
    "send",
    "save-open",
    "live",
    "load-user",
    "load-project",
    "apply-template",
    "read-sequence",
    "send-sequence",
    "read-global",
    "send-global",
    "read-cv",
    "send-cv",
    "read-matrix",
    "send-matrix",
    "send-system",
    "diagnostics",
  ])
    $(id).disabled = !ready;
  if (!ready) $("live").checked = false;
}
function invalidate(reason) {
  ready = false;
  clearTimeout(liveTimer);
  if (pending) {
    clearTimeout(pending.timer);
    pending.reject(Error(reason));
    pending = null;
  }
  connected();
}
function received(event) {
  const bytes = event.data;
  if (bytes[0] !== 240) return;
  let r;
  try {
    r = parseFrame(bytes);
  } catch (error) {
    invalidate(error.message);
    message(error.message, true);
    return;
  }
  if (!pending || r.id !== pending.id || r.command !== pending.command) return;
  const p = pending;
  pending = null;
  clearTimeout(p.timer);
  if (r.status) {
    let text =
      {
        1: "通信形式が不正です",
        2: "パラメータまたは回路の競合",
        3: "本体で編集中、または音源の電源がOFFです",
        4: "保存に失敗しました。保存済みデータは保持されています",
        5: "スキーマ不一致。v0.10との再接続が必要です",
      }[r.status] || "本体エラー";
    if (r.status === 2 && r.data.length >= 6) {
      const n = r.data[4];
      text = String.fromCharCode(...r.data.slice(5, 5 + n));
      const k = 5 + n,
        m = r.data[k] || 0;
      const alternative = String.fromCharCode(...r.data.slice(k + 1, k + 1 + m));
      if (alternative) text += `\n代替案: ${alternative}`;
    }
    const error = Error(text);
    error.status = r.status;
    p.reject(error);
  } else p.resolve(r.data);
}
function request(command, payload = []) {
  const run = () =>
    new Promise((resolve, reject) => {
      if (!output || !input || (command !== 0 && !ready)) {
        reject(Error("v0.10本体へ接続してください。書き込みは無効です。"));
        return;
      }
      const id = requestId++ % 128;
      pending = {
        id,
        command,
        resolve,
        reject,
        timer: setTimeout(() => {
          pending = null;
          reject(
            Error(
              "応答がありません。入出力ポートとファームウェアを確認してください。",
            ),
          );
        }, 2500),
      };
      try {
        output.send(frame(command, id, payload));
      } catch (error) {
        clearTimeout(pending.timer);
        pending = null;
        reject(error);
      }
    });
  const result = serial.catch(() => {}).then(run);
  serial = result;
  return result;
}
async function guard(fn) {
  try {
    await fn();
  } catch (error) {
    message(error.message, true);
  }
}
async function selectPorts() {
  invalidate("MIDIポート変更");
  if (input) input.onmidimessage = null;
  input = access?.inputs.get($("midi-in").value);
  output = access?.outputs.get($("midi-out").value);
  if (!input || !output) {
    message(
      "実機のMIDI入出力を両方選択してください。未接続の間、鍵盤を弾いても鳴りません。",
    );
    return;
  }
  await input.open();
  await output.open();
  input.onmidimessage = received;
  try {
    verifyHello(await request(0, [SCHEMA]));
    ready = true;
    connected();
    if (entry().dirty || entry().sequenceDirty) {
      render();
      message(
        "接続しました。オフラインの編集を保持しています。反映か本体から読み出すを選べます。鍵盤は実機へも送ります。",
      );
    } else await readSelected();
  } catch (error) {
    invalidate(error.message);
    throw error;
  }
}
$("connect").onclick = () =>
  guard(async () => {
    if (!navigator.requestMIDIAccess)
      throw Error(
        "このブラウザはWeb MIDI非対応のため実機に接続できません。このページは実機専用で、未接続時は鳴りません。実機接続にはAndroid / PCのChrome系ブラウザを使ってください。",
      );
    access = await navigator.requestMIDIAccess({ sysex: true });
    for (const [id, ports] of [
      ["midi-out", access.outputs],
      ["midi-in", access.inputs],
    ]) {
      $(id).replaceChildren(new Option("ポートを選択", ""));
      for (const p of ports.values()) $(id).add(new Option(p.name || p.id, p.id));
      const preferred = [...ports.values()].find((p) =>
        /chiptune|saa1099/i.test(p.name || ""),
      );
      if (preferred) $(id).value = preferred.id;
    }
    access.onstatechange = () => {
      if (input?.state === "disconnected" || output?.state === "disconnected") {
        invalidate("本体が切断されました");
        message("切断しました。ローカルの編集と演奏は保持します。");
      }
    };
    await selectPorts();
  });
$("midi-in").onchange = $("midi-out").onchange = () => guard(selectPorts);
function mark() {
  const e = entry();
  e.dirty = true;
  e.rev++;
  renderMeta();
  renderTracks();
  scheduleLive();
}
function scheduleLive() {
  clearTimeout(liveTimer);
  if (ready && $("live").checked && !sending)
    liveTimer = setTimeout(() => guard(sendSelected), 100);
}
$("live").onchange = scheduleLive;
async function sendSelected() {
  if (sending) return;
  const id = selected,
    e = cache.get(id),
    rev = e.rev,
    payload = encodeTrack(e.track);
  sending = true;
  try {
    const got = decodeTrack(await request(2, payload));
    if (e.rev === rev) {
      e.track = got;
      e.dirty = false;
    } else {
      e.track.outputs = got.outputs;
      e.track.resources = got.resources;
    }
    if (selected === id) {
      renderMeta();
      renderTracks();
    }
    message("本体RAMへ反映しました。保持する場合は保存してください。");
  } finally {
    sending = false;
    if (entry().dirty && (selected !== id || e.rev !== rev)) scheduleLive();
  }
}
async function readProjectTracks() {
  for (let id = 0; id < 14; id++) {
    const e = cache.get(id),
      rev = e.rev;
    if (e.dirty) continue;
    const got = decodeTrack(await request(1, [id]));
    if (e.rev === rev && !e.dirty) {
      e.track = got;
      e.rev++;
    }
  }
  renderTracks();
  renderMeta();
}
async function readSelected() {
  const id = selected,
    e = cache.get(id);
  e.track = decodeTrack(await request(1, [id]));
  e.sequence = decodeSequence(await request(8, [id]));
  e.dirty = e.sequenceDirty = false;
  e.rev++;
  if (selected === id) render();
  message("本体のTrackとSequenceを読み出しました。");
}
$("send").onclick = () => guard(sendSelected);
$("read").onclick = () => guard(readSelected);
function renderTracks() {
  $("tracks").replaceChildren();
  for (let t = 0; t < 14; t++) {
    const e = cache.get(t),
      b = document.createElement("button");
    b.dataset.track = t;
    b.classList.toggle("active", selected === t);
    b.classList.toggle("muted", !e.track.enabled || e.track.mute);
    b.classList.toggle("dirty", e.dirty);
    const num = document.createElement("b");
    num.textContent = String(t + 1).padStart(2, "0");
    const info = document.createElement("span");
    info.textContent = e.track.name;
    const sub = document.createElement("small");
    sub.textContent = `${e.track.patch.chip ? "SN" : "SAA"} · CH${t < 10 ? t + 1 : "—"} · ${e.track.mode === 1 ? "POLY" : e.track.mode === 2 ? "LEGATO" : "DIRECT"}`;
    info.append(sub);
    b.append(num, info);
    b.onclick = () =>
      guard(async () => {
        clearTimeout(liveTimer);
        allNotesOff();
        selected = t;
        render();
        if (ready && !e.dirty) await readSelected();
      });
    $("tracks").append(b);
  }
}
function control(
  parent,
  label,
  value,
  { min = 0, max = 100, step = 1, options = null, id = null, disabledOptions = [] } = {},
  change,
) {
  const box = document.createElement("div");
  box.className = "control";
  const l = document.createElement("label"),
    row = document.createElement("span");
  row.className = "row";
  const title = document.createElement("span");
  title.textContent = label;
  const out = document.createElement("output");
  out.textContent = options ? "" : value;
  row.append(title, out);
  l.append(row);
  let el;
  if (options) {
    el = document.createElement("select");
    for (const [i, v] of options.entries()) {
      const option = new Option(v, i);
      option.disabled = disabledOptions.includes(i);
      el.add(option);
    }
    el.value = value;
    el.onchange = () => change(Number(el.value));
  } else {
    el = document.createElement("input");
    el.type = "range";
    el.min = min;
    el.max = max;
    el.step = step;
    el.value = value;
    el.oninput = () => {
      out.textContent = el.value;
      change(Number(el.value));
    };
  }
  if (id) el.id = id;
  el.setAttribute("aria-label", label);
  l.append(el);
  box.append(l);
  parent.append(box);
  return el;
}
const optionsFor = {
  chip: chips,
  source: sources,
  env: envelopes,
  output: outputs,
  shape: shapes,
  noise_type: ["Periodic", "White"],
  noise_rate: ["Fast", "Mid", "Slow", "Key Track"],
  lfo_shape: ["Sine", "Triangle", "Square", "Random"],
  lfo_rate: ["0.5 Hz", "1 Hz", "2 Hz", "4 Hz", "8 Hz", "16 Hz", "32 Hz", "64 Hz"],
  layer_source: sources,
  env_invert: ["Same", "Inverse"],
  retrigger: ["Off", "On"],
};
const labels = {
  attack: "アタック · ms",
  decay: "ディケイ · ms",
  sustain: "サステイン · %",
  release: "リリース · ms",
  level: "音色レベル · %",
  pitch_amount: "ピッチ変化 · 半音",
  pitch_decay: "ピッチ変化時間 · ms",
  glide: "グライド · ms",
  vibrato: "ビブラート · cent",
  tremolo: "トレモロ · %",
  detune: "左右デチューン · cent",
  noise_mix: "ノイズの割合 · %",
  noise_rate: "ノイズ速度",
  env: "SAAの音色",
  shape: "ENV波形",
  layers: "レイヤー数",
  transpose: "移調 · 半音",
  fine: "微調整 · cent",
};
function setPatch(name, value) {
  const t = entry().track;
  if (name === "chip") {
    t.patch = structuredClone(factory[value ? 21 : 20].patch);
    t.name = value ? "SN MONO" : "SAA TONE";
    t.morph_target = t.morph = 0;
  } else {
    t.patch[name] = value;
    normalizePatch(t.patch, name);
  }
  mark();
  const structural = [
    "chip",
    "source",
    "env",
    "layers",
    "layer_source",
    "noise_rate",
    "noise_type",
    "shape",
    "env_steps",
    "env_invert",
    "output",
  ];
  if (structural.includes(name)) {
    t.morph_target = t.morph = 0;
    render();
    return;
  }
  for (const el of document.querySelectorAll(`[data-param="${name}"]`)) {
    el.value = value;
    const out = el.closest("label").querySelector("output");
    if (out && !optionsFor[name]) out.textContent = value;
  }
  drawEnvelope();
}
function patchControl(parent, name, advanced = false) {
  const t = entry().track,
    p = t.patch,
    f = fields.find((f) => f.name === name);
  if (!supported(p, name)) return;
  const el = control(
    parent,
    waveform(p) === Wave.PULSE && ["noise_pitch", "noise_sweep"].includes(name)
      ? name === "noise_pitch"
        ? "PULSE移調 · 半音"
        : "PULSEスイープ · 半音"
      : advanced
        ? f.label
        : labels[name] || f.label,
    p[name],
    {
      min: f.min,
      max: f.max,
      step: name === "env_steps" ? 8 : f.type === "u16" ? 5 : 1,
      options:
        name === "source" && p.chip === 1
          ? ["Tone", "Noise", "Tone 3 + Noise · Metal"]
          : name === "noise_rate" && p.chip === 1
            ? [
                "Fast · CLK/512",
                "Mid · CLK/1024",
                "Slow · CLK/2048",
                "Tone 3 · Key Track",
              ]
            : optionsFor[name] || null,
      id: advanced ? `adv-${name}` : `patch-${name}`,
    },
    (v) => guard(() => setPatch(name, v)),
  );
  el.dataset.param = name;
}
function renderPerformance() {
  const parent = $("performance-controls");
  parent.replaceChildren();
  const t = entry().track;
  for (const [name, label, opts] of [
    ["mode", "演奏モード", { options: ["DIRECT · 単音", "POLY · 複数音", "LEGATO · 単音"] }],
    ["voices", "POLY声数", { min: 1, max: 6 }],
    ["level", "トラック音量", {}],
    ["pan", "PAN · L ↔ R", {}],
    ["mute", "ミュート", { options: ["Off", "On"] }],
    ["enabled", "Track", { options: ["Off", "On"] }],
  ]) {
    if (name === "pan" && t.patch.output !== 0) continue;
    control(parent, label, t[name], { ...opts, id: `track-${name}` }, (v) => {
      t[name] = v;
      mark();
      if (name === "level" || name === "pan") sendPerformanceCc();
    });
  }
  const channel = !t.usb
    ? 0
    : !(t.usb & (t.usb - 1))
      ? Math.log2(t.usb) + 1
      : 17;
  control(
    parent,
    "受信MIDI ch（鍵盤の送り先）",
    channel,
    {
      options: [
        "Off",
        ...Array.from({ length: 16 }, (_, i) => `CH ${i + 1}`),
        "Multi · Advanced",
      ],
      id: "receive-channel",
    },
    (v) => {
      if (v === 17) return;
      t.usb = t.trs = v ? 1 << (v - 1) : 0;
      mark();
    },
  );
}
function renderPatch() {
  const normal = $("musical-controls");
  normal.replaceChildren();
  const p = entry().track.patch;
  patchControl(normal, "chip");
  const waves = availableWaves(p.chip);
  control(
    normal,
    "発音 / WAVE",
    waves.indexOf(waveform(p)),
    { options: waves.map((w) => waveNames[w]), id: "patch-wave" },
    (v) =>
      guard(() => {
        selectWave(entry().track.patch, waves[v]);
        entry().track.morph_target = entry().track.morph = 0;
        mark();
        render();
      }),
  );
  if (waveform(p) === Wave.AM) patchControl(normal, "source");
  for (const name of musicalParameters(p)) patchControl(normal, name);
  const tips = [
    "通常トーンは各発振器のデューティ比50%固定です。",
    "SN Periodic NoiseをTone 3で駆動します。パルス幅は周期の約1/15（6.7%）固定。Noise回路とTone 3を予約し、高音域は分周の量子化誤差が大きくなります。",
    "SAAのハードウェアENVを波形として出力します。出力3/6とクロック2/5を使用します。Zero / Maximumは周期波形ではありません。",
    "SAAのToneとNoiseを同じ出力へ混合します。Noise速度は3声単位で共有し、両成分の連続ミックス比はありません。",
    p.chip === 0
      ? "SAA Noiseの速度は3声単位で共有します。音程操作はKey Trackで有効です。"
      : "SNのWhite / Periodic Noiseです。固定レートでは音程・グライド・デチューンを使用しません。Key TrackではTone 3を予約します。",
    "SNのTone 3と連動Noiseを出力します。両成分は同じTone 3周波数に従い、Noise Mixで音量比を調整します。",
    "SAAのTone / NoiseをハードウェアENVで振幅変調します。ENV RATIOはENVクロックとノートの半音差です。",
  ];
  $("wave-hint").textContent = tips[waveform(p)];
  const advanced = $("advanced-controls");
  advanced.replaceChildren();
  for (const f of fields) patchControl(advanced, f.name, true);
  const morph = $("morph-controls");
  morph.replaceChildren();
  const t = entry().track;
  control(
    morph,
    "B / USER patch",
    t.morph_target,
    { options: ["Off", ...Array.from({ length: 16 }, (_, i) => `USER ${i + 1}`)], id: "morph-target" },
    (v) => {
      t.morph_target = v;
      mark();
      renderPatch();
    },
  );
  control(morph, "A ↔ B", t.morph, { id: "morph-amount" }, (v) => {
    t.morph = v;
    mark();
  });
  const macros = $("macro-controls");
  macros.replaceChildren();
  for (let i = 0; i < 4; i++) {
    control(
      macros,
      `Macro ${i + 1} destination`,
      t.macro_dest[i],
      {
        options: destinations.map((n, d) =>
          supportedMod(t, d) ? n : `${n} · inactive`,
        ),
        disabledOptions: destinations.flatMap((_, d) =>
          supportedMod(t, d) ? [] : [d],
        ),
      },
      (v) => {
        t.macro_dest[i] = v;
        mark();
      },
    );
    control(macros, `Macro ${i + 1}`, t.macro[i], { min: -100, max: 100 }, (v) => {
      t.macro[i] = v;
      mark();
    });
  }
  for (const [key, title] of [
    ["usb", "USB receive channels"],
    ["trs", "TRS receive channels"],
  ]) {
    const label = document.createElement("label");
    label.textContent = title;
    const input = document.createElement("input");
    input.type = "text";
    input.placeholder = "1, 2, 16 / empty = Off";
    input.setAttribute("aria-label", title);
    input.value = Array.from({ length: 16 }, (_, i) => i + 1)
      .filter((c) => t[key] & (1 << (c - 1)))
      .join(", ");
    input.onchange = () =>
      guard(() => {
        const channels = input.value.trim()
          ? input.value.split(",").map((x) => Number(x.trim()))
          : [];
        if (channels.some((c) => !Number.isInteger(c) || c < 1 || c > 16))
          throw Error("MIDI chは1〜16をカンマで区切ってください。");
        t[key] = channels.reduce((m, c) => m | (1 << (c - 1)), 0);
        mark();
        renderPerformance();
      });
    label.append(input);
    macros.append(label);
  }
  for (const [k, label, lo, hi] of [
    ["low", "Low note", 0, 127],
    ["high", "High note", 0, 127],
    ["velocity_curve", "Velocity curve", 0, 3],
  ])
    control(macros, label, t[k], { min: lo, max: hi }, (v) => {
      t[k] = v;
      mark();
    });
}
function renderMeta() {
  const t = entry().track,
    r = resourceInfo(t.patch, t.mode, t.voices);
  $("track-label").textContent = `TRACK ${selected + 1} · ${
    t.usb === 0
      ? "NO MIDI"
      : Array.from({ length: 16 }, (_, i) => i)
          .filter((i) => t.usb & (1 << i))
          .map((i) => `CH${i + 1}`)
          .join(" / ")
  }${entry().dirty ? " · 未反映" : ""}`;
  $("patch-description").textContent = description(t.patch);
  $("resource-cost").textContent = r.cost;
  $("resource-detail").textContent = r.detail;
  $("resource-actual").textContent =
    t.resources === undefined
      ? "回路配置は本体への反映時に検証します。"
      : `本体の出力: ${outputNames(t.outputs)} · 専用クロック: ${outputNames(t.resources & ~t.outputs)}`;
}
function drawEnvelope() {
  const canvas = $("envelope"),
    ctx = canvas.getContext("2d"),
    p = entry().track.patch,
    w = canvas.width,
    h = canvas.height;
  ctx.clearRect(0, 0, w, h);
  ctx.strokeStyle = "#253b52";
  for (let i = 0; i < 10; i++) {
    ctx.beginPath();
    ctx.moveTo((i * w) / 10, 0);
    ctx.lineTo((i * w) / 10, h);
    ctx.stroke();
  }
  const duration = p.attack + p.decay + p.release + 400,
    x = (ms) => 20 + (ms / duration) * (w - 40),
    y = (level) => h - 15 - (level / 100) * (h - 30);
  ctx.strokeStyle = "#77c8bc";
  ctx.lineWidth = 3;
  ctx.beginPath();
  ctx.moveTo(20, y(0));
  ctx.lineTo(x(p.attack), y(100));
  ctx.lineTo(x(p.attack + p.decay), y(p.sustain));
  ctx.lineTo(x(p.attack + p.decay + 400), y(p.sustain));
  ctx.lineTo(w - 20, y(0));
  ctx.stroke();
}
function renderSequence() {
  const q = entry().sequence,
    parent = $("sequence-controls");
  parent.replaceChildren();
  for (const [k, label, opts] of [
    ["enabled", "Sequence", { options: ["Off", "On"] }],
    ["length", "Length", { min: 1, max: 16 }],
    ["division", "MIDI clocks / step", { min: 1, max: 24 }],
    ["direction", "Direction", { options: ["Forward", "Reverse", "Random"] }],
    ["arp", "Arpeggio", { options: ["Off", "Up", "Down", "Up-down", "Random", "Chord"] }],
    ["arp_octaves", "Octaves", { min: 1, max: 4 }],
  ])
    control(parent, label, q[k], opts, (v) => {
      q[k] = v;
      entry().sequenceDirty = true;
    });
  $("steps").replaceChildren();
  for (let i = 0; i < 16; i++) {
    const box = document.createElement("div");
    box.className = "step";
    const title = document.createElement("strong");
    title.textContent = `STEP ${i + 1}`;
    box.append(title);
    for (const [k, label, lo, hi] of [
      ["note", "NOTE", 0, 127],
      ["gate", "GATE %", 0, 100],
      ["velocity", "VELOCITY", 0, 127],
      ["motion", "MACRO 1", -100, 100],
    ]) {
      const l = document.createElement("label");
      l.textContent = label;
      const input = document.createElement("input");
      input.type = "number";
      input.min = lo;
      input.max = hi;
      input.value = q[k][i];
      input.setAttribute("aria-label", `${label} step ${i + 1}`);
      input.onchange = () => {
        q[k][i] = Math.max(lo, Math.min(hi, Number(input.value)));
        entry().sequenceDirty = true;
      };
      l.append(input);
      box.append(l);
    }
    $("steps").append(box);
  }
}
function renderGlobal() {
  const parent = $("clock-controls");
  parent.replaceChildren();
  const specs = [
    ["Reference", ["4 MHz", "2 MHz", "1 MHz"]],
    ["FX", ["Off", "Manual", "Rise", "Triangle", "Square", "Random", "Sequence", "Dive"]],
    ["Base drop · 半音", 0, 48],
    ["Depth · 半音", 0, 48],
    ["Rate · Hz", ["0.25", "0.5", "1", "2", "4", "8", "16", "32", "50", "80", "100", "160", "250", "400", "600", "800"]],
    ["MIDI sync", 0, 8],
    ["Note retrigger", ["Off", "On"]],
    ["Trigger source", ["Any SN", "MIDI SN", "CV1", "CV2", "CH10", "Tone", "Noise", "Melody/CV"]],
    ["Clock input", ["USB", "TRS"]],
    ["Clock policy", ["Auto", "External", "Internal", "Gate"]],
    ["Tempo · BPM", 0, 210],
    ["TRS → USB", ["Off", "On"]],
    ["Master", 0, 100],
    ["SAA level", 0, 100],
    ["SN L level", 0, 100],
    ["SN R level", 0, 100],
    ...Array.from({ length: 8 }, (_, i) => [`Clock step ${i + 1}`, 0, 100]),
    ["Gate clock input", ["Gate 1", "Gate 2"]],
  ];
  specs.forEach(([label, lo, hi], i) =>
    control(
      parent,
      label,
      i === 10 ? global[i] + 30 : global[i],
      Array.isArray(lo) ? { options: lo } : { min: i === 10 ? 30 : lo, max: i === 10 ? 240 : hi },
      (v) => {
        global[i] = i === 10 ? v - 30 : v;
        globalDirty = true;
      },
    ),
  );
}
function renderMatrix() {
  const parent = $("matrix-controls");
  parent.replaceChildren();
  const sources = [
    "Off",
    "CV1",
    "CV2",
    "CC1 / Mod wheel",
    "Pressure",
    ...Array.from({ length: 14 }, (_, i) => `Track ${i + 1} LFO`),
    ...Array.from({ length: 14 }, (_, i) => `Track ${i + 1} Step`),
  ];
  matrix.forEach((m, i) => {
    const row = document.createElement("div");
    row.className = "matrix-row";
    for (const [k, label, opts] of [
      ["source", `Source ${i + 1}`, { options: sources }],
      ["track", "Track", { options: Array.from({ length: 14 }, (_, i) => `Track ${i + 1}`) }],
      [
        "parameter",
        "Parameter",
        {
          options: destinations.map((n, d) =>
            supportedMod(cache.get(m.track).track, d) ? n : `${n} · inactive`,
          ),
          disabledOptions: destinations.flatMap((_, d) =>
            supportedMod(cache.get(m.track).track, d) ? [] : [d],
          ),
        },
      ],
      ["amount", "Amount", { min: -100, max: 100 }],
    ])
      control(row, label, m[k], opts, (v) => {
        m[k] = v;
        matrixDirty = true;
        if (k === "track") renderMatrix();
      });
    parent.append(row);
  });
}
function renderCv() {
  const parent = $("cv-controls");
  parent.replaceChildren();
  for (const c of cvs) {
    const box = document.createElement("div");
    const heading = document.createElement("h3");
    heading.textContent = `CV / GATE ${c.input + 1}`;
    box.append(heading);
    const grid = document.createElement("div");
    grid.className = "controls";
    for (const [k, label, opts] of [
      ["mode", "CV mode", { options: ["Off", "1V/oct note", "Modulation"] }],
      ["track", "CV track", { options: Array.from({ length: 14 }, (_, i) => `Track ${i + 1}`) }],
      ["base_note", "0V NOTE", { max: 127 }],
      ["scale", "Scale", { options: ["Off", "Chromatic", "Major", "Minor", "Dorian", "Minor pentatonic", "Major pentatonic", "Whole tone"] }],
      ["root", "Root", { max: 11 }],
      ["glide", "Glide", { max: 7 }],
      ["range", "Input range", { options: ["0–5V", "0–10V"] }],
      ["gate", "Gate mode", { options: ["Off", "Note", "Retrigger", "Step", "Clock FX trigger"] }],
      ["gate_track", "Gate track", { options: Array.from({ length: 14 }, (_, i) => `Track ${i + 1}`) }],
      ["gate_note", "Gate NOTE", { max: 127 }],
      ["parameter", "Mod parameter", { options: destinations }],
      ["amount", "Mod amount", { min: -100, max: 100 }],
    ])
      control(grid, label, c[k], opts, (v) => {
        c[k] = v;
        cvDirty = true;
      });
    box.append(grid);
    parent.append(box);
  }
}
function render() {
  renderTracks();
  $("name").value = entry().track.name;
  renderPerformance();
  renderPatch();
  renderMeta();
  renderSequence();
  drawEnvelope();
}
$("name").oninput = () => {
  entry().track.name = cleanName($("name").value).trim();
  mark();
};
for (let i = 0; i < factory.length; i++)
  $("factory").add(
    new Option(`F${String(i + 1).padStart(2, "0")} · ${factory[i].name.trim()}`, i),
  );
$("factory").value = 20;
for (let i = 0; i < 16; i++) $("user-patch").add(new Option(`USER ${i + 1}`, i));
for (let i = 0; i < 4; i++) $("project-slot").add(new Option(`PROJECT ${i + 1}`, i));
$("load-factory").onclick = () => {
  const f = factory[Number($("factory").value)];
  entry().track.patch = structuredClone(f.patch);
  entry().track.name = f.name.trim();
  entry().track.morph_target = entry().track.morph = 0;
  mark();
  render();
  message("工場音色を読み込みました。実機接続時は鍵盤で演奏できます。実機へは「本体に反映」で送ります。");
};
$("load-user").onclick = () =>
  guard(async () => {
    const data = await request(13, [Number($("user-patch").value)]);
    entry().track.patch = decodePatch(data, 9);
    entry().track.name = String.fromCharCode(...data.slice(1, 9)).trim();
    entry().track.morph_target = entry().track.morph = 0;
    mark();
    render();
    message("USER音色をローカルへ読み込みました。反映ボタンで本体へ送信します。");
  });
$("read-sequence").onclick = () =>
  guard(async () => {
    entry().sequence = decodeSequence(await request(8, [selected]));
    entry().sequenceDirty = false;
    renderSequence();
  });
$("send-sequence").onclick = () =>
  guard(async () => {
    entry().sequence = decodeSequence(await request(9, encodeSequence(entry().sequence)));
    entry().sequenceDirty = false;
    message("SequenceをRAMへ反映しました。");
  });
$("read-global").onclick = () =>
  guard(async () => {
    global = decodeGlobal(await request(4));
    globalDirty = false;
    renderGlobal();
    message("共通設定を読み出しました。");
  });
$("send-global").onclick = () =>
  guard(async () => {
    global = decodeGlobal(await request(5, encodeGlobal(global)));
    globalDirty = false;
    renderGlobal();
    message("SNクロックと共通設定を反映しました。");
  });
$("read-cv").onclick = () =>
  guard(async () => {
    const d = await request(10, [127]);
    cvs = [decodeCv(d.slice(0, 14)), decodeCv(d.slice(14))];
    cvDirty = false;
    renderCv();
    message("CV/Gateを読み出しました。");
  });
$("send-cv").onclick = () =>
  guard(async () => {
    const d = await request(11, cvs.flatMap(encodeCv));
    cvs = [decodeCv(d.slice(0, 14)), decodeCv(d.slice(14))];
    cvDirty = false;
    renderCv();
    message("CV/Gateの2系統をまとめて反映しました。");
  });
$("read-matrix").onclick = () =>
  guard(async () => {
    const d = await request(15);
    matrix = Array.from({ length: 8 }, (_, i) => ({
      source: d[i * 5],
      track: d[i * 5 + 1],
      parameter: d[i * 5 + 2],
      amount: readWord(d, i * 5 + 3) - 8192,
    }));
    matrixDirty = false;
    renderMatrix();
  });
$("send-matrix").onclick = () =>
  guard(async () => {
    await request(
      16,
      matrix.flatMap((m) => [m.source, m.track, m.parameter, ...signed(m.amount)]),
    );
    matrixDirty = false;
    message("変調ルーティングを反映しました。");
  });
$("send-system").onclick = () =>
  guard(async () => {
    await request(18, [Number($("brightness").value)]);
    message("Systemの明るさをRAMへ反映しました。永続化はSystem保存を選択してください。");
  });
$("diagnostics").onclick = () =>
  guard(async () => {
    const d = await request(21),
      wide = (i) => d.slice(i, i + 5).reduce((n, v, k) => n + v * 2 ** (7 * k), 0);
    $("diagnostic-result").textContent = JSON.stringify(
      {
        voices: d[0],
        steals: wide(1),
        refused: wide(6),
        register_writes: wide(11),
        coalesced: wide(16),
        pending: d[21],
        max_pending: d[22],
        tick_max_us: wide(23),
        tick_overruns: wide(28),
      },
      null,
      2,
    );
  });
function saveScope() {
  const kind = Number($("save-kind").value);
  $("save-slot").replaceChildren();
  for (let i = 0; i < (kind === 2 ? 4 : kind === 3 ? 1 : 16); i++)
    $("save-slot").add(new Option(String(i + 1), i));
  $("save-scope").textContent = [
    "音色のみ保存。Project・Track設定・Systemは変更しません。",
    "このTrackのみ保存。ProjectとSystemは変更しません。",
    "現在の全Track・ルート・Sequence・共通クロックを保存します。",
    "Systemのみ保存。音色とProjectは変更しません。",
  ][kind];
}
$("save-kind").onchange = saveScope;
$("save-open").onclick = () => $("save-dialog").showModal();
$("save-cancel").onclick = () => $("save-dialog").close();
$("save").onclick = () =>
  guard(async () => {
    const kind = Number($("save-kind").value);
    if (
      (kind !== 3 && [...cache.values()].some((e) => e.dirty)) ||
      (kind === 2 &&
        ([...cache.values()].some((e) => e.sequenceDirty) || globalDirty || cvDirty || matrixDirty))
    )
      throw Error("未反映の編集があります。先に必要な項目を本体へ反映してください。");
    await request(3, [kind, selected, Number($("save-slot").value)]);
    $("save-dialog").close();
    message(
      [
        "Patchのみ保存しました。Projectは変更していません。",
        "Trackのみ保存しました。",
        "Projectを保存しました。",
        "Systemのみ保存しました。",
      ][kind],
    );
  });
function replace(action, text) {
  replaceAction = action;
  if (text) $("replace-dialog").querySelector("p").textContent = text;
  $("replace-dialog").showModal();
}
$("poly-focus").onclick = () =>
  replace(async () => {
    await request(24, [selected]);
    entry().dirty = false;
    entry().sequenceDirty = false;
    await readProjectTracks();
    await readSelected();
    message("選択TrackをPOLY化しました。同じチップの他TrackはOFF、別チップの演奏は保持します。");
  }, "同じチップのほかのTrackをOFFにして、選択Trackへ声数を集めます。別チップの演奏と他Trackの未反映編集は保持します。選択Trackの未反映編集は破棄します。");
$("apply-template").onclick = () =>
  replace(async () => {
    await request(22, [Number($("template").value)]);
    for (const e of cache.values()) {
      e.dirty = false;
      e.sequenceDirty = false;
    }
    await readProjectTracks();
    await readSelected();
    message("配置をRAMへ置き換えました。保存は別操作です。");
  });
$("load-project").onclick = () =>
  replace(async () => {
    await request(6, [Number($("project-slot").value)]);
    for (const e of cache.values()) {
      e.dirty = false;
      e.sequenceDirty = false;
    }
    await readProjectTracks();
    await readSelected();
    message("Projectを読み込みました。");
  });
$("replace-cancel").onclick = () => $("replace-dialog").close();
$("replace-confirm").onclick = () =>
  guard(async () => {
    await replaceAction();
    $("replace-dialog").close();
  });
function download(name, text) {
  const url = URL.createObjectURL(new Blob([text], { type: "application/json" })),
    a = document.createElement("a");
  a.href = url;
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
$("export").onclick = () =>
  guard(() => {
    const e = entry();
    download(
      `${cleanName(e.track.name).trim() || "PATCH"}-v10.json`,
      JSON.stringify(exportTrack(e.track, e.sequence), null, 2),
    );
  });
$("import-open").onclick = () => $("import").click();
$("import").onchange = () =>
  guard(async () => {
    const f = $("import").files[0];
    if (!f) return;
    const got = importTrack(JSON.parse(await f.text()), selected);
    entry().track = got.track;
    entry().sequence = got.sequence;
    entry().sequenceDirty = true;
    mark();
    render();
    message("v0.10 JSONをローカルへ読み込みました。");
  });

/* ================================================================
   演奏：実機へのMIDIノート送信のみ（内蔵音源なし＝未接続時は無音）
   ================================================================ */
const NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
const noteName = (n) => `${NOTE_NAMES[n % 12]}${Math.floor(n / 12) - 1}`;
const LFO_HZ = [0.5, 1, 2, 4, 8, 16, 32, 64];
let actx = null,
  master = null,
  noiseBuffer = null,
  pulseWave = null;
let bendValue = 0; // -8192..8191
let sustainOn = false;
const heldNotes = []; // 押下順（モノ時の復帰用）
const voices = new Map(); // note -> voice（POLY）/ モノは単一音のみ保持
let monoNote = null;
const sustained = new Set();

function ensureAudio() {
  // 実機専用：ブラウザ内蔵音源は持たない。実機未接続時は発音しない。
  return false;
}
let unconnectedNotified = false;
function notifyUnconnected() {
  if (unconnectedNotified || output) return;
  unconnectedNotified = true;
  message(
    "実機が未接続のため鳴りません。このページは実機専用です。USB MIDIで実機を接続してください（Android / PCのChrome系）。",
    true,
  );
}

function midiChannel() {
  const t = entry().track;
  if (t.usb) {
    for (let i = 0; i < 16; i++) if (t.usb & (1 << i)) return i;
  }
  return selected < 10 ? selected : 0;
}
function midiSend(bytes) {
  try {
    output?.send(bytes);
  } catch {
    /* ポート未選択時は無音（実機専用） */
  }
}
function midiNoteOn(note, velocity) {
  midiSend([0x90 | midiChannel(), note, velocity]);
}
function midiNoteOff(note) {
  midiSend([0x80 | midiChannel(), note, 0]);
}
function sendPerformanceCc() {
  const t = entry().track,
    ch = midiChannel();
  midiSend([0xb0 | ch, 7, Math.round((t.level / 100) * 127)]);
  if (t.patch.output === 0) midiSend([0xb0 | ch, 10, Math.round((t.pan / 100) * 127)]);
}
function sendBend() {
  const v = bendValue + 8192,
    ch = midiChannel();
  midiSend([0xe0 | ch, v & 127, (v >> 7) & 127]);
  for (const voice of voices.values())
    for (const o of voice.oscs)
      o.detune.setTargetAtTime(
        voice.baseDetune + (bendValue / 8192) * 200,
        actx.currentTime,
        0.01,
      );
}

function patchFreq(note, p) {
  const bendSemis = (bendValue / 8192) * 2;
  return (
    440 *
    2 ** ((note + p.transpose + bendSemis - 69) / 12) *
    2 ** (p.fine / 1200)
  );
}
function oscFor(wave, p) {
  const osc = actx.createOscillator();
  if (wave === Wave.PULSE) osc.setPeriodicWave(pulseWave);
  else if (wave === Wave.ENV || wave === Wave.AM)
    osc.type = p.shape === 5 || p.shape === 4 ? "triangle" : p.shape === 3 || p.shape === 7 ? "sawtooth" : "square";
  else osc.type = "square";
  return osc;
}
function startVoice(note, velocity) {
  if (!ensureAudio()) return null;
  const t = entry().track,
    p = t.patch,
    wave = waveform(p),
    now = actx.currentTime;
  const gainNode = actx.createGain();
  const peak =
    (t.level / 100) * (p.level / 100) * (velocity / 127) * (t.mute ? 0 : 1);
  const a = Math.max(p.attack / 1000, 0.003),
    d = Math.max(p.decay / 1000, 0.003);
  gainNode.gain.setValueAtTime(0, now);
  gainNode.gain.linearRampToValueAtTime(Math.max(peak, 0.0001), now + a);
  gainNode.gain.linearRampToValueAtTime(
    Math.max(peak * (p.sustain / 100), 0.0001),
    now + a + d,
  );
  const panNode = actx.createStereoPanner ? actx.createStereoPanner() : null;
  if (panNode) {
    panNode.pan.value =
      p.output === 1 ? -0.8 : p.output === 2 ? 0.8 : (t.pan - 50) / 50;
    gainNode.connect(panNode).connect(master);
  } else gainNode.connect(master);
  const voice = { oscs: [], sources: [], gain: gainNode, baseDetune: 0, note };
  const freq = patchFreq(note, p);
  const addOsc = (semis, detuneCents, levelScale, pan) => {
    const osc = oscFor(wave, p);
    const target = freq * 2 ** (semis / 12);
    const startF =
      p.pitch_amount && p.pitch_decay > 0
        ? target * 2 ** (p.pitch_amount / 12)
        : target;
    if (p.glide > 0 && voice.lastFreq) {
      osc.frequency.setValueAtTime(voice.lastFreq, now);
      osc.frequency.linearRampToValueAtTime(startF, now + 0.02);
    } else osc.frequency.setValueAtTime(startF, now);
    if (p.pitch_amount && p.pitch_decay > 0)
      osc.frequency.exponentialRampToValueAtTime(target, now + p.pitch_decay / 1000);
    else if (p.glide > 0 && voice.lastFreq)
      osc.frequency.linearRampToValueAtTime(target, now + p.glide / 1000);
    osc.detune.value = detuneCents + (bendValue / 8192) * 200;
    const og = actx.createGain();
    og.gain.value = levelScale;
    let dest = gainNode;
    if (pan !== null && actx.createStereoPanner) {
      const sp = actx.createStereoPanner();
      sp.pan.value = pan;
      osc.connect(og).connect(sp).connect(gainNode);
      dest = null;
    }
    if (dest) osc.connect(og).connect(dest);
    // LFO：ビブラート / トレモロ（近似）
    let lfo = null;
    if (p.vibrato > 0 || p.tremolo > 0) {
      lfo = actx.createOscillator();
      lfo.type = ["sine", "triangle", "square", "sawtooth"][p.lfo_shape] || "sine";
      lfo.frequency.value = LFO_HZ[p.lfo_rate] || 4;
      if (p.vibrato > 0) {
        const lg = actx.createGain();
        lg.gain.value = p.vibrato;
        lfo.connect(lg).connect(osc.detune);
      }
      if (p.tremolo > 0) {
        const tg = actx.createGain();
        tg.gain.value = (p.tremolo / 100) * 0.3 * Math.max(peak, 0.05);
        lfo.connect(tg).connect(gainNode.gain);
      }
      lfo.start();
      voice.oscs.push(lfo);
    }
    osc.start();
    voice.oscs.push(osc);
    voice.lastFreq = target;
    return osc;
  };
  const addNoise = (levelScale) => {
    const src = actx.createBufferSource();
    src.buffer = noiseBuffer;
    src.loop = true;
    src.playbackRate.value =
      p.noise_rate === 3 ? Math.max(freq / 220, 0.2) : [2.2, 1.4, 0.8][p.noise_rate] || 1;
    const nf = actx.createBiquadFilter();
    nf.type = "lowpass";
    nf.frequency.value = p.noise_type === 0 ? 3500 : 7000;
    const ng = actx.createGain();
    ng.gain.value = levelScale;
    src.connect(nf).connect(ng).connect(gainNode);
    src.start();
    voice.sources.push(src);
  };
  if (wave === Wave.NOISE) addNoise(1);
  else if (wave === Wave.TONE_NOISE) {
    addOsc(0, 0, 0.7, null);
    addNoise(0.5);
  } else if (wave === Wave.METAL) {
    addOsc(0, 0, 1 - p.noise_mix / 100, null);
    addNoise(p.noise_mix / 100);
  } else {
    const stereoDetune = p.chip === 1 && p.output === 0 && p.detune > 0;
    if (stereoDetune) {
      addOsc(0, -p.detune / 2, 0.6, -0.65);
      addOsc(0, p.detune / 2, 0.6, 0.65);
    } else addOsc(0, 0, 1, null);
    if (p.chip === 1 && p.source === 0 && p.layers >= 2) {
      addOsc(p.layer_interval, 0, (p.layer_level / 100) * 0.7, null);
      if (p.layers >= 3) addOsc(p.stack_interval, 0, (p.layer_level / 100) * 0.6, null);
    }
    if (p.chip === 0 && p.layers > 1)
      addOsc(p.layer_interval, 0, (p.layer_level / 100) * 0.8, null);
  }
  voice.baseDetune = 0;
  return voice;
}
function releaseVoice(voice, fast = false) {
  if (!voice || !actx) return;
  const p = entry().track.patch,
    now = actx.currentTime,
    r = fast ? 0.03 : Math.max(p.release / 1000, 0.03);
  try {
    voice.gain.gain.cancelScheduledValues(now);
    voice.gain.gain.setTargetAtTime(0, now, r / 3);
    const stopAt = now + r + 0.15;
    for (const o of voice.oscs) o.stop(stopAt);
    for (const s of voice.sources) s.stop(stopAt);
  } catch {
    /* 停止済み */
  }
}
function retargetVoice(voice, note) {
  // LEGATO：エンベロープを継続したまま音程だけ移す
  const p = entry().track.patch,
    now = actx.currentTime,
    freq = patchFreq(note, p),
    g = Math.max(p.glide / 1000, 0.01);
  for (const o of voice.oscs) {
    if (o.frequency)
      o.frequency.setTargetAtTime(freq, now, g / 3);
  }
  voice.note = note;
}
function updateNowNote() {
  const active = [...heldNotes, ...sustained];
  $("now-note").textContent = active.length
    ? [...new Set(active)].map(noteName).join(" ")
    : "—";
  document.querySelectorAll("#keyboard .key").forEach((k) => {
    const n = Number(k.dataset.note);
    k.classList.toggle("active", heldNotes.includes(n) || sustained.has(n));
  });
}
function noteOn(note, velocity = null) {
  const vel = velocity ?? Number($("velocity").value);
  if (!heldNotes.includes(note)) heldNotes.push(note);
  sustained.delete(note);
  const t = entry().track;
  midiNoteOn(note, vel);
  notifyUnconnected();
  if (t.mode === 1) {
    // POLY
    if (voices.has(note)) releaseVoice(voices.get(note), true);
    while (voices.size >= Math.max(t.voices, 1)) {
      const oldest = voices.keys().next().value;
      releaseVoice(voices.get(oldest), true);
      voices.delete(oldest);
    }
    const v = startVoice(note, vel);
    if (v) voices.set(note, v);
  } else {
    // DIRECT / LEGATO（単音）
    if (monoNote !== null && voices.has(monoNote)) {
      if (t.mode === 2 && voices.get(monoNote)) {
        retargetVoice(voices.get(monoNote), note);
        voices.set(note, voices.get(monoNote));
        voices.delete(monoNote);
      } else {
        releaseVoice(voices.get(monoNote), true);
        voices.delete(monoNote);
        const v = startVoice(note, vel);
        if (v) voices.set(note, v);
      }
    } else {
      const v = startVoice(note, vel);
      if (v) voices.set(note, v);
    }
    monoNote = note;
  }
  updateNowNote();
}
function noteOff(note) {
  const i = heldNotes.indexOf(note);
  if (i >= 0) heldNotes.splice(i, 1);
  midiNoteOff(note);
  if (sustainOn) {
    sustained.add(note);
    updateNowNote();
    return;
  }
  finishNoteOff(note);
}
function finishNoteOff(note) {
  sustained.delete(note);
  const t = entry().track;
  if (t.mode !== 1 && monoNote === note) {
    const prev = heldNotes[heldNotes.length - 1];
    if (prev !== undefined && voices.has(note)) {
      if (t.mode === 2) {
        retargetVoice(voices.get(note), prev);
        voices.set(prev, voices.get(note));
        voices.delete(note);
      } else {
        releaseVoice(voices.get(note), true);
        voices.delete(note);
        const v = startVoice(prev, Number($("velocity").value));
        if (v) voices.set(prev, v);
      }
      monoNote = prev;
      updateNowNote();
      return;
    }
    monoNote = null;
  }
  if (voices.has(note)) {
    releaseVoice(voices.get(note));
    voices.delete(note);
  }
  updateNowNote();
}
function allNotesOff() {
  heldNotes.length = 0;
  sustained.clear();
  for (const v of voices.values()) releaseVoice(v, true);
  voices.clear();
  monoNote = null;
  for (let ch = 0; ch < 16; ch++) {
    midiSend([0xb0 | ch, 120, 0]);
    midiSend([0xb0 | ch, 123, 0]);
  }
  updateNowNote();
}
function setSustain(on) {
  sustainOn = on;
  $("sustain-btn").classList.toggle("on", on);
  $("sustain-btn").setAttribute("aria-pressed", String(on));
  midiSend([0xb0 | midiChannel(), 64, on ? 127 : 0]);
  if (!on) for (const n of [...sustained]) if (!heldNotes.includes(n)) finishNoteOff(n);
}
$("sustain-btn").addEventListener("pointerdown", (e) => {
  e.preventDefault();
  setSustain(true);
});
addEventListener("pointerup", () => {
  if (sustainOn && !spaceHeld) setSustain(false);
});
let spaceHeld = false;

/* ---------- 鍵盤（レスポンシブ） ---------- */
let baseNote = 48; // C3
const PC_KEYS_LOWER = { a: 0, w: 1, s: 2, e: 3, d: 4, f: 5, t: 6, g: 7, y: 8, h: 9, u: 10, j: 11, k: 12, o: 13, l: 14 };
function buildKeyboard() {
  const kb = $("keyboard");
  const width = kb.clientWidth || innerWidth;
  const whiteTarget = width < 600 ? 42 : 48;
  let whites = Math.max(7, Math.min(36, Math.floor(width / whiteTarget)));
  // Cから始めて白鍵whites個ぶんの音域を作る
  const notes = [];
  let n = baseNote,
    w = 0;
  while (w < whites) {
    notes.push(n);
    if (![1, 3, 6, 8, 10].includes(n % 12)) w++;
    n++;
  }
  const last = notes[notes.length - 1];
  $("oct-label").textContent = `${noteName(baseNote)} – ${noteName(last)}`;
  kb.replaceChildren();
  const whiteW = 100 / whites;
  let wi = 0;
  for (const note of notes) {
    const black = [1, 3, 6, 8, 10].includes(note % 12);
    const el = document.createElement("button");
    el.className = "key" + (black ? " black" : "");
    el.dataset.note = note;
    el.setAttribute("aria-label", noteName(note));
    if (black) {
      el.style.left = `calc(${(wi - 0.32) * whiteW}% )`;
      el.style.width = `calc(${whiteW * 0.64}%)`;
      el.style.height = "62%";
    } else {
      el.style.left = `${wi * whiteW}%`;
      el.style.width = `${whiteW}%`;
      el.style.height = "100%";
      wi++;
    }
    const label = document.createElement("span");
    label.className = "klabel";
    label.textContent = note % 12 === 0 ? noteName(note) : "";
    const pc = Object.entries(PC_KEYS_LOWER).find(([, v]) => baseNote + v === note);
    if (pc && !black) {
      const s = document.createElement("span");
      s.className = "pckey";
      s.textContent = pc[0].toUpperCase();
      label.append(s);
    }
    el.append(label);
    kb.append(el);
  }
  updateNowNote();
}
const pointerNotes = new Map();
function keyVelocity(event, el) {
  const r = el.getBoundingClientRect();
  const ratio = Math.min(1, Math.max(0, (event.clientY - r.top) / r.height));
  return Math.round(55 + ratio * 72);
}
$("keyboard").addEventListener("pointerdown", (event) => {
  const el = event.target.closest(".key");
  if (!el) return;
  event.preventDefault();
  ensureAudio();
  const note = Number(el.dataset.note);
  pointerNotes.set(event.pointerId, note);
  noteOn(note, keyVelocity(event, el));
});
$("keyboard").addEventListener("pointermove", (event) => {
  if (!pointerNotes.has(event.pointerId)) return;
  const el = document.elementFromPoint(event.clientX, event.clientY)?.closest?.(".key");
  if (!el) return;
  const note = Number(el.dataset.note),
    prev = pointerNotes.get(event.pointerId);
  if (note !== prev) {
    pointerNotes.set(event.pointerId, note);
    noteOff(prev);
    noteOn(note, keyVelocity(event, el));
  }
});
for (const type of ["pointerup", "pointercancel"]) {
  $("keyboard").addEventListener(type, (event) => {
    const note = pointerNotes.get(event.pointerId);
    if (note !== undefined) {
      pointerNotes.delete(event.pointerId);
      noteOff(note);
    }
  });
}
$("keyboard").addEventListener("contextmenu", (e) => e.preventDefault());
$("oct-down").onclick = () => {
  baseNote = Math.max(12, baseNote - 12);
  buildKeyboard();
};
$("oct-up").onclick = () => {
  baseNote = Math.min(96, baseNote + 12);
  buildKeyboard();
};
let resizeTimer = null;
addEventListener("resize", () => {
  clearTimeout(resizeTimer);
  resizeTimer = setTimeout(buildKeyboard, 150);
});
addEventListener("keydown", (event) => {
  const tag = document.activeElement?.tagName;
  if (["INPUT", "SELECT", "TEXTAREA"].includes(tag) && event.key !== "Escape") return;
  if (event.repeat) return;
  const key = event.key.toLowerCase();
  if (key === " ") {
    event.preventDefault();
    spaceHeld = true;
    setSustain(true);
    return;
  }
  if (key === "z") {
    baseNote = Math.max(12, baseNote - 12);
    buildKeyboard();
    return;
  }
  if (key === "x") {
    baseNote = Math.min(96, baseNote + 12);
    buildKeyboard();
    return;
  }
  const semis = PC_KEYS_LOWER[key];
  if (semis === undefined) return;
  event.preventDefault();
  noteOn(baseNote + semis);
});
addEventListener("keyup", (event) => {
  const key = event.key.toLowerCase();
  if (key === " ") {
    spaceHeld = false;
    setSustain(false);
    return;
  }
  const semis = PC_KEYS_LOWER[key];
  if (semis === undefined) return;
  noteOff(baseNote + semis);
});
$("bend").addEventListener("input", () => {
  bendValue = Number($("bend").value);
  sendBend();
});
$("bend").addEventListener("change", () => {
  $("bend").value = 0;
  bendValue = 0;
  sendBend();
});
$("panic").onclick = () => {
  stopPlayback();
  allNotesOff();
  if (ready) guard(() => request(19));
  message("PANIC：全ノートを停止しました。");
};
addEventListener("blur", allNotesOff);
document.addEventListener("visibilitychange", () => {
  if (document.hidden) allNotesOff();
});

/* ================= MIDIファイル再生（実機へ送出） =================
   SMF format 0/1 を読み、テンポマップに従ってファイルのチャンネル
   そのままで実機へ送る。未接続時は再生しない（実機専用の方針どおり）。 */
let midiSong = null;
let playback = null;

function parseMidiFile(buffer) {
  const view = new DataView(buffer);
  let pos = 0;
  const readStr = (n) => {
    let s = "";
    for (let i = 0; i < n; i++) s += String.fromCharCode(view.getUint8(pos++));
    return s;
  };
  const readU32 = () => {
    const v = view.getUint32(pos);
    pos += 4;
    return v;
  };
  const readU16 = () => {
    const v = view.getUint16(pos);
    pos += 2;
    return v;
  };
  const readVar = () => {
    let v = 0;
    for (;;) {
      const b = view.getUint8(pos++);
      v = (v << 7) | (b & 0x7f);
      if (!(b & 0x80)) return v;
    }
  };
  if (readStr(4) !== "MThd") throw Error("MIDIファイルではありません。");
  const headerLen = readU32();
  readU16(); // format
  const trackCount = readU16();
  const division = readU16();
  pos += headerLen - 6;
  const rawEvents = [];
  const tempos = [];
  for (let t = 0; t < trackCount; t++) {
    if (readStr(4) !== "MTrk") throw Error("MIDIファイルが壊れています。");
    const len = readU32();
    const end = pos + len;
    let tick = 0,
      running = 0;
    while (pos < end) {
      tick += readVar();
      let status = view.getUint8(pos);
      if (status & 0x80) {
        pos++;
        running = status;
      } else status = running;
      if (status === 0xff) {
        const type = view.getUint8(pos++);
        const mlen = readVar();
        if (type === 0x51 && mlen === 3) {
          const us =
            (view.getUint8(pos) << 16) |
            (view.getUint8(pos + 1) << 8) |
            view.getUint8(pos + 2);
          tempos.push({ tick, usPerQuarter: us });
        }
        pos += mlen;
      } else if (status === 0xf0 || status === 0xf7) {
        const slen = readVar();
        const data = [0xf0];
        for (let i = 0; i < slen; i++) data.push(view.getUint8(pos++));
        if (status === 0xf0) data.push(0xf7);
        rawEvents.push({ tick, data });
      } else {
        const hi = status & 0xf0;
        const d1 = view.getUint8(pos++);
        const d2 = hi === 0xc0 || hi === 0xd0 ? null : view.getUint8(pos++);
        rawEvents.push({
          tick,
          data: d2 === null ? [status, d1] : [status, d1, d2],
        });
      }
    }
    pos = end;
  }
  const tickToSec = (tick) => {
    if (division & 0x8000) {
      const frames = 256 - ((division >> 8) & 0xff);
      const fps = frames === 29 ? 29.97 : frames;
      return tick / (fps * (division & 0xff));
    }
    let sec = 0,
      lastTick = 0,
      us = 500000;
    for (const tp of [...tempos].sort((a, b) => a.tick - b.tick)) {
      if (tp.tick > tick) break;
      sec += ((tp.tick - lastTick) / division) * (us / 1e6);
      lastTick = tp.tick;
      us = tp.usPerQuarter;
    }
    return sec + ((tick - lastTick) / division) * (us / 1e6);
  };
  const events = rawEvents
    .map((e) => ({ time: tickToSec(e.tick), data: e.data }))
    .sort((a, b) => a.time - b.time);
  return {
    events,
    duration: events.length ? events[events.length - 1].time : 0,
  };
}

function stopPlayback(sendOff = true) {
  if (!playback) return;
  clearInterval(playback.timer);
  playback = null;
  if (sendOff && output) {
    for (let ch = 0; ch < 16; ch++) {
      output.send([0xb0 | ch, 123, 0]);
      output.send([0xb0 | ch, 120, 0]);
    }
  }
  $("midi-play").disabled = !midiSong;
  $("midi-stop").disabled = true;
  if (midiSong) $("midi-status").textContent = "停止";
}

function startPlayback() {
  if (!midiSong) return;
  if (!output) {
    message(
      "実機が未接続です。MIDIファイル再生も実機接続時のみです。",
      true,
    );
    return;
  }
  stopPlayback(false);
  const startAt = performance.now() / 1000 + 0.15;
  playback = { startAt, index: 0, timer: null };
  playback.timer = setInterval(() => {
    if (!playback) return;
    const elapsed = performance.now() / 1000 - playback.startAt;
    while (
      playback.index < midiSong.events.length &&
      midiSong.events[playback.index].time <= elapsed + 0.25
    ) {
      const ev = midiSong.events[playback.index++];
      try {
        output.send(ev.data, (playback.startAt + ev.time) * 1000);
      } catch {
        /* ポート切断時は無視 */
      }
    }
    $("midi-status").textContent =
      `再生中 ${Math.max(0, elapsed).toFixed(1)} / ${midiSong.duration.toFixed(1)} 秒`;
    if (
      playback.index >= midiSong.events.length &&
      elapsed > midiSong.duration + 0.3
    ) {
      stopPlayback();
      $("midi-status").textContent = "再生終了";
    }
  }, 40);
  $("midi-play").disabled = true;
  $("midi-stop").disabled = false;
}

$("midi-file").onchange = async () => {
  const file = $("midi-file").files[0];
  if (!file) return;
  stopPlayback(false);
  try {
    midiSong = parseMidiFile(await file.arrayBuffer());
    $("midi-status").textContent =
      `${file.name} · ${midiSong.events.length}イベント · ${midiSong.duration.toFixed(1)}秒`;
    $("midi-play").disabled = false;
    message("MIDIファイルを読み込みました。実機接続時に再生できます。");
  } catch (error) {
    midiSong = null;
    $("midi-play").disabled = true;
    message(error.message || "MIDIファイルを読み込めませんでした。", true);
  }
};
$("midi-play").onclick = () => startPlayback();
$("midi-stop").onclick = () => stopPlayback();

/* ---------- 初期化 ---------- */
connected();
render();
renderGlobal();
renderCv();
renderMatrix();
saveScope();
buildKeyboard();
