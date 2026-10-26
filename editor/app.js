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
  $("connection-state").textContent = ready ? "CONNECTED · v0.10" : "OFFLINE";
  $("connection-state").classList.toggle("connected", ready);
  for (const id of [
    "poly-focus",
    "read",
    "send",
    "audition",
    "stop",
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
      const alternative = String.fromCharCode(
        ...r.data.slice(k + 1, k + 1 + m),
      );
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
    message("MIDIの入出力を両方選択してください。");
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
        "接続しました。オフラインの編集を保持しています。反映か本体から読み出すを選べます。",
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
        "Web MIDIに対応するブラウザーで開いてください。オフライン編集は利用できます。",
      );
    access = await navigator.requestMIDIAccess({ sysex: true });
    for (const [id, ports] of [
      ["midi-out", access.outputs],
      ["midi-in", access.inputs],
    ]) {
      $(id).replaceChildren(new Option("ポートを選択", ""));
      for (const p of ports.values())
        $(id).add(new Option(p.name || p.id, p.id));
      const preferred = [...ports.values()].find((p) =>
        /chiptune|saa1099/i.test(p.name || ""),
      );
      if (preferred) $(id).value = preferred.id;
    }
    access.onstatechange = () => {
      if (input?.state === "disconnected" || output?.state === "disconnected") {
        invalidate("本体が切断されました");
        message("切断しました。ローカルの編集は保持します。");
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
    sub.textContent = `${e.track.patch.chip ? "SN" : "SAA"} · ${e.track.mode === 1 ? "POLY" : "DIRECT"}`;
    info.append(sub);
    b.append(num, info);
    b.onclick = () =>
      guard(async () => {
        clearTimeout(liveTimer);
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
  {
    min = 0,
    max = 100,
    step = 1,
    options = null,
    id = null,
    disabledOptions = [],
  } = {},
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
    advanced ? f.label : labels[name] || f.label,
    p[name],
    {
      min: f.min,
      max: f.max,
      step: name === "env_steps" ? 8 : f.type === "u16" ? 5 : 1,
      options: optionsFor[name] || null,
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
    ["level", "音量", {}],
    ["pan", "PAN · L ↔ R", {}],
    ["mute", "ミュート", { options: ["Off", "On"] }],
    [
      "mode",
      "演奏モード",
      { options: ["DIRECT · 単音", "POLY · 複数音", "LEGATO · 単音"] },
    ],
    ["voices", "POLY声数", { min: 1, max: 6 }],
    ["enabled", "Track", { options: ["Off", "On"] }],
  ]) {
    if (name === "pan" && t.patch.output !== 0) continue;
    control(parent, label, t[name], { ...opts, id: `track-${name}` }, (v) => {
      t[name] = v;
      mark();
    });
  }
  const channel = !t.usb
    ? 0
    : !(t.usb & (t.usb - 1))
      ? Math.log2(t.usb) + 1
      : 17;
  control(
    parent,
    "受信MIDI ch",
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
  for (const name of [
    "chip",
    "source",
    "env",
    "shape",
    "noise_type",
    "noise_rate",
    "noise_mix",
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
    "detune",
  ])
    patchControl(normal, name);
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
    {
      options: [
        "Off",
        ...Array.from({ length: 16 }, (_, i) => `USER ${i + 1}`),
      ],
      id: "morph-target",
    },
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
    control(
      macros,
      `Macro ${i + 1}`,
      t.macro[i],
      { min: -100, max: 100 },
      (v) => {
        t.macro[i] = v;
        mark();
      },
    );
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
    [
      "arp",
      "Arpeggio",
      { options: ["Off", "Up", "Down", "Up-down", "Random", "Chord"] },
    ],
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
    [
      "FX",
      [
        "Off",
        "Manual",
        "Rise",
        "Triangle",
        "Square",
        "Random",
        "Sequence",
        "Dive",
      ],
    ],
    ["Base drop · 半音", 0, 48],
    ["Depth · 半音", 0, 48],
    [
      "Rate · Hz",
      [
        "0.25",
        "0.5",
        "1",
        "2",
        "4",
        "8",
        "16",
        "32",
        "50",
        "80",
        "100",
        "160",
        "250",
        "400",
        "600",
        "800",
      ],
    ],
    ["MIDI sync", 0, 8],
    ["Note retrigger", ["Off", "On"]],
    [
      "Trigger source",
      ["Any SN", "MIDI SN", "CV1", "CV2", "CH10", "Tone", "Noise", "Melody/CV"],
    ],
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
      Array.isArray(lo)
        ? { options: lo }
        : { min: i === 10 ? 30 : lo, max: i === 10 ? 240 : hi },
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
      [
        "track",
        "Track",
        { options: Array.from({ length: 14 }, (_, i) => `Track ${i + 1}`) },
      ],
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
      [
        "track",
        "CV track",
        { options: Array.from({ length: 14 }, (_, i) => `Track ${i + 1}`) },
      ],
      ["base_note", "0V NOTE", { max: 127 }],
      [
        "scale",
        "Scale",
        {
          options: [
            "Off",
            "Chromatic",
            "Major",
            "Minor",
            "Dorian",
            "Minor pentatonic",
            "Major pentatonic",
            "Whole tone",
          ],
        },
      ],
      ["root", "Root", { max: 11 }],
      ["glide", "Glide", { max: 7 }],
      ["range", "Input range", { options: ["0–5V", "0–10V"] }],
      [
        "gate",
        "Gate mode",
        { options: ["Off", "Note", "Retrigger", "Step", "Clock FX trigger"] },
      ],
      [
        "gate_track",
        "Gate track",
        { options: Array.from({ length: 14 }, (_, i) => `Track ${i + 1}`) },
      ],
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
  renderPitch();
}
$("name").oninput = () => {
  entry().track.name = cleanName($("name").value).trim();
  mark();
};
for (let i = 0; i < factory.length; i++)
  $("factory").add(
    new Option(
      `F${String(i + 1).padStart(2, "0")} · ${factory[i].name.trim()}`,
      i,
    ),
  );
$("factory").value = 20;
for (let i = 0; i < 16; i++)
  $("user-patch").add(new Option(`USER ${i + 1}`, i));
for (let i = 0; i < 4; i++)
  $("project-slot").add(new Option(`PROJECT ${i + 1}`, i));
$("load-factory").onclick = () => {
  const f = factory[Number($("factory").value)];
  entry().track.patch = structuredClone(f.patch);
  entry().track.name = f.name.trim();
  entry().track.morph_target = entry().track.morph = 0;
  mark();
  render();
  message(
    "工場音色をローカルへ読み込みました。Trackのモード・PAN・声数は保持します。",
  );
};
$("load-user").onclick = () =>
  guard(async () => {
    const data = await request(13, [Number($("user-patch").value)]);
    entry().track.patch = decodePatch(data, 9);
    entry().track.name = String.fromCharCode(...data.slice(1, 9)).trim();
    entry().track.morph_target = entry().track.morph = 0;
    mark();
    render();
    message(
      "USER音色をローカルへ読み込みました。反映ボタンで本体へ送信します。",
    );
  });
function renderPitch() {
  const n = Number($("test-note").value);
  $("test-frequency").textContent =
    Number.isInteger(n) && n >= 0 && n <= 127
      ? `${["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][n % 12]}${Math.floor(n / 12) - 1} · ${(440 * 2 ** ((n - 69) / 12)).toFixed(2)} Hz`
      : "NOTE 0–127";
}
$("test-note").oninput = renderPitch;
$("audition").onclick = () =>
  guard(() => request(7, [selected, Number($("test-note").value), 1]));
$("stop").onclick = () => guard(() => request(19));
$("read-sequence").onclick = () =>
  guard(async () => {
    entry().sequence = decodeSequence(await request(8, [selected]));
    entry().sequenceDirty = false;
    renderSequence();
  });
$("send-sequence").onclick = () =>
  guard(async () => {
    entry().sequence = decodeSequence(
      await request(9, encodeSequence(entry().sequence)),
    );
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
      matrix.flatMap((m) => [
        m.source,
        m.track,
        m.parameter,
        ...signed(m.amount),
      ]),
    );
    matrixDirty = false;
    message("変調ルーティングを反映しました。");
  });
$("send-system").onclick = () =>
  guard(async () => {
    await request(18, [Number($("brightness").value)]);
    message(
      "Systemの明るさをRAMへ反映しました。永続化はSystem保存を選択してください。",
    );
  });
$("diagnostics").onclick = () =>
  guard(async () => {
    const d = await request(21),
      wide = (i) =>
        d.slice(i, i + 5).reduce((n, v, k) => n + v * 2 ** (7 * k), 0);
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
$("save-open").onclick = () => {
  $("save-dialog").showModal();
};
$("save-cancel").onclick = () => $("save-dialog").close();
$("save").onclick = () =>
  guard(async () => {
    const kind = Number($("save-kind").value);
    if (
      (kind !== 3 && [...cache.values()].some((e) => e.dirty)) ||
      (kind === 2 &&
        ([...cache.values()].some((e) => e.sequenceDirty) ||
          globalDirty ||
          cvDirty ||
          matrixDirty))
    )
      throw Error(
        "未反映の編集があります。先に必要な項目を本体へ反映してください。",
      );
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
function replace(
  action,
  text = "現在の全トラックを置き換え、演奏を停止します。フラッシュ保存は別操作です。",
) {
  replaceAction = action;
  $("replace-dialog").querySelector("p").textContent = text;
  $("replace-dialog").showModal();
}
$("poly-focus").onclick = () =>
  replace(async () => {
    await request(24, [selected]);
    for (const e of cache.values()) {
      e.dirty = false;
      e.sequenceDirty = false;
    }
    await readSelected();
    message(
      "選択TrackをPOLY化しました。同じチップの他TrackはOFF、別チップの演奏は保持します。",
    );
  }, "同じチップのほかのTrackをOFFにして、選択Trackへ声数を集めます。別チップの演奏は保持します。");
$("apply-template").onclick = () =>
  replace(async () => {
    await request(22, [Number($("template").value)]);
    for (const e of cache.values()) {
      e.dirty = false;
      e.sequenceDirty = false;
    }
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
  const url = URL.createObjectURL(
      new Blob([text], { type: "application/json" }),
    ),
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
connected();
render();
renderGlobal();
renderCv();
renderMatrix();
saveScope();
