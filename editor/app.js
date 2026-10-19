import {
  fields,
  models,
  shapes,
  parameters,
  isSaa,
  isEnv,
  isNoise,
  limit,
  roleFor,
  fromValues,
  valuesOf,
  validateSound,
  word,
  signed,
  readWord,
  frame,
  parseFrame,
  decodePart,
  encodePart,
  visibleFields,
  resourceInfo,
  waveform,
  layerWaveform,
  envValue,
  encodeMotion,
  decodeMotion,
  exportPatch,
  importPatch,
  cleanName,
} from "./core.js";
const $ = (id) => document.getElementById(id);
const factory = await fetch("./factory.json").then((r) => {
  if (!r.ok) throw Error("Factory library unavailable");
  return r.json();
});
const descriptions = [
  "矩形波とソフトADSR。1回路で左右へ出力します。",
  "グループ共有のノイズ発生器を使います。",
  "1つの出力でトーンとノイズを有効化します。",
  "ハードウェアENVそのものを楽音波形にします。",
  "トーンをハードウェアENVで振幅変調します。",
  "左右の同番号トーンをペアにします。",
  "左SNのトーンです。",
  "右SNのトーンです。",
  "左右のノイズ発生器をペアにします。",
  "トーン3と連動ノイズを混合します。周期モードで金属音に。",
  "左SNのノイズです。",
  "右SNのノイズです。",
  "ノイズをハードウェアENVで振幅変調します。",
  "トーン＋ノイズをハードウェアENVで振幅変調します。",
  "ENVの楽音波形に、短いノイズ／トーンを重ねます。",
  "複数のステレオトーンペアを重ねて1つの音色にします。",
];
function freshPart(part = 0, index = 18) {
  const sound = fromValues(factory[index].values);
  return {
    part,
    enabled: 1,
    poly: Math.min(2, limit(sound.model)),
    role: roleFor(sound.model),
    channel: part === 3 ? 9 : Math.min(part, 15),
    name: factory[index].name.trim(),
    sound,
  };
}
const newMotion = (p) => ({
  part: p,
  length: 16,
  division: 6,
  direction: 0,
  sequence: 0,
  arp: 0,
  arp_octaves: 1,
  running: 0,
  note: Array.from({ length: 16 }, (_, i) => [48, 55, 60, 55][i % 4]),
  gate: Array(16).fill(70),
  velocity: Array(16).fill(85),
  motion: Array(16).fill(0),
});
const cache = new Map();
let part = freshPart(),
  motion = newMotion(0),
  dirty = false,
  motionDirty = false,
  clockDirty = false,
  cvDirty = false,
  chip = "saa";
let clock = [1, 0, 0, 24, 4, 0, 1, 0, 7, 0, 50, 100, 25, 75, 100, 50, 0];
let cvs = Array.from({ length: 2 }, (_, i) => ({
  input: i,
  mode: 0,
  part: i,
  base_note: 48,
  scale: 1,
  root: 0,
  glide: 0,
  range: 0,
  gate: 0,
  gate_part: i,
  parameter: 0,
  gate_note: 48,
  amount: 100,
}));
let access,
  output,
  input,
  ready = false,
  nextId = 1,
  pending = null,
  queue = Promise.resolve(),
  session = 0;
let envelopeHandles = [],
  envelopeScale = 1,
  working = false;
function message(text, error = false) {
  $("message").textContent = text;
  $("message").classList.toggle("error", error);
}
function updateConnection() {
  for (const id of [
    "read",
    "send",
    "save-open",
    "audition",
    "stop",
    "read-motion",
    "send-motion",
    "read-clock",
    "send-clock",
    "read-cv",
    "send-cv",
  ])
    $(id).disabled = !ready;
  $("connection-state").classList.toggle("connected", ready);
  $("connection-state").querySelector("span").textContent = ready
    ? "USB MIDI CONNECTED"
    : "OFFLINE EDITOR";
}
function invalidate(reason) {
  ready = false;
  session++;
  if (pending) {
    clearTimeout(pending.timer);
    pending.reject(Error(reason));
    pending = null;
  }
  updateConnection();
}
function request(command, payload = []) {
  const epoch = session;
  const job = queue
    .catch(() => {})
    .then(
      () =>
        new Promise((resolve, reject) => {
          if (!output || !input || epoch !== session) {
            reject(Error("MIDIポートを接続してください"));
            return;
          }
          const id = nextId++ % 128;
          const timer = setTimeout(() => {
            pending = null;
            reject(
              Error(
                "本体の応答がありません。v0.8以降・入出力ポート・USB接続を確認してください。",
              ),
            );
          }, 3000);
          pending = { id, command, resolve, reject, timer };
          try {
            output.send(frame(command, id, payload));
          } catch (e) {
            clearTimeout(timer);
            pending = null;
            reject(e);
          }
        }),
    );
  queue = job;
  return job;
}
function received(event) {
  let r;
  try {
    r = parseFrame(event.data);
  } catch {
    return;
  }
  if (!pending || r.id !== pending.id || r.command !== (pending.command | 64))
    return;
  const p = pending;
  pending = null;
  clearTimeout(p.timer);
  if (r.status) {
    p.reject(
      Error(
        [
          "",
          "通信形式が一致しません。",
          "適合しない音色／回路不足／範囲外です。声数とプロファイルを確認してください。",
          "本体で編集中、または音源電源がOFFです。編集を閉じて再試行してください。",
          "保存に失敗しました。電源と校正状態を確認してください。",
        ][r.status] || `Device error ${r.status}`,
      ),
    );
  } else p.resolve(r.data);
}
async function guard(fn) {
  if (working) return;
  working = true;
  document.body.classList.add("busy");
  const state = [...document.querySelectorAll("button,input,select")].map(
    (c) => [c, c.disabled],
  );
  for (const [c] of state) c.disabled = true;
  try {
    await fn();
  } catch (e) {
    message(e.message || String(e), true);
  } finally {
    for (const [c, disabled] of state) c.disabled = disabled;
    working = false;
    document.body.classList.remove("busy");
    updateConnection();
  }
}
function listPorts() {
  if (!access) return;
  const previous = [$("midi-out").value, $("midi-in").value];
  for (const [id, map, index] of [
    ["midi-out", access.outputs, 0],
    ["midi-in", access.inputs, 1],
  ]) {
    const s = $(id);
    s.replaceChildren(new Option(index ? "MIDI input" : "MIDI output", ""));
    for (const p of map.values())
      s.add(new Option(`${p.name} · ${p.manufacturer || "MIDI"}`, p.id));
    const known = [...map.values()].find((p) => /chiptune/i.test(p.name || ""));
    s.value = map.has(previous[index]) ? previous[index] : known?.id || "";
  }
}
async function selectPorts() {
  invalidate("MIDIポートが変更されました");
  if (input) input.onmidimessage = null;
  output = access?.outputs.get($("midi-out").value);
  input = access?.inputs.get($("midi-in").value);
  if (!output || !input) {
    message("本体のMIDI入力・出力を両方選択してください。");
    return;
  }
  await output.open();
  await input.open();
  input.onmidimessage = received;
  const hello = await request(0);
  if (hello.length !== 8 || hello[1] < 8 || hello[7] !== fields.length)
    throw Error("v0.8以降の対応ファームが必要です。");
  ready = true;
  updateConnection();
  message(
    `本体 v${hello[0]}.${hello[1]}.${hello[2]}に接続しました。「本体から読み出す」で同期できます。`,
  );
}
$("connect").onclick = () =>
  guard(async () => {
    if (!navigator.requestMIDIAccess)
      throw Error(
        "このブラウザーはWeb MIDIに対応していません。Chrome / Edgeを使用してください。JSON編集はこのまま使えます。",
      );
    access = await navigator.requestMIDIAccess({ sysex: true });
    access.onstatechange = () => {
      if (
        (output && output.state === "disconnected") ||
        (input && input.state === "disconnected")
      ) {
        invalidate("USBが切断されました");
        message("USBが切断されました。編集内容はブラウザー内に残っています。");
      }
      listPorts();
    };
    listPorts();
    await selectPorts();
  });
$("midi-out").onchange = $("midi-in").onchange = () => guard(selectPorts);
function mark() {
  dirty = true;
  delete part.outputs;
  delete part.resources;
  renderMeta();
  drawWave();
  drawEnvelope();
}
function selectControl(label, options, value, onchange) {
  const box = document.createElement("div");
  box.className = "control";
  const l = document.createElement("label"),
    select = document.createElement("select");
  l.textContent = label;
  select.setAttribute("aria-label", label);
  for (const [v, text] of options) select.add(new Option(text, v));
  select.value = value;
  select.onchange = () => onchange(Number(select.value));
  box.append(l, select);
  return box;
}
function rangeControl(label, lo, hi, value, onchange, step = 1) {
  const box = document.createElement("div");
  box.className = "control";
  const top = document.createElement("div");
  top.className = "control-top";
  const l = document.createElement("label");
  l.textContent = label;
  const number = document.createElement("input");
  number.type = "number";
  number.min = lo;
  number.max = hi;
  number.step = step;
  number.value = value;
  number.setAttribute("aria-label", label + " value");
  number.style.width = "72px";
  const range = document.createElement("input");
  range.type = "range";
  range.min = lo;
  range.max = hi;
  range.step = step;
  range.value = value;
  range.setAttribute("aria-label", label);
  function change(v) {
    if (!Number.isFinite(v)) return;
    v = Math.round(Math.max(lo, Math.min(hi, v)) / step) * step;
    range.value = number.value = v;
    onchange(v);
  }
  range.oninput = () => change(Number(range.value));
  number.onchange = () => change(Number(number.value));
  top.append(l, number);
  box.append(top, range);
  return box;
}
const enums = {
  shape: shapes,
  steps: ["8", "16"],
  invert: ["OFF", "ON"],
  retrigger: ["FREE RUN", "KEY RESET"],
  noise_rate: ["FAST", "MID", "SLOW", "KEY TRACK"],
  mode: ["POLY", "MONO", "LEGATO"],
  lfo_rate: [
    "0.5 Hz",
    "1 Hz",
    "2 Hz",
    "4 Hz",
    "8 Hz",
    "16 Hz",
    "32 Hz",
    "64 Hz",
  ],
  glide: [
    "OFF",
    "10 ms",
    "30 ms",
    "60 ms",
    "120 ms",
    "250 ms",
    "500 ms",
    "1000 ms",
  ],
  layer_source: ["TONE", "NOISE", "TONE + NOISE"],
  stack_size: ["2", "3"],
};
function fieldControl(f) {
  const n = f.name;
  let options = enums[n];
  if (n.startsWith("macro_dest")) options = parameters;
  const change = (v) => {
    part.sound[n] = v;
    mark();
    if (["layer_source", "stack_size"].includes(n)) renderControls();
  };
  if (options) {
    const values = options.map((text, i) => [
      n === "steps" ? Number(text) : n === "stack_size" ? i + 2 : i,
      text,
    ]);
    if (n === "shape" && !isSaa(part.sound.model))
      return selectControl(
        "Noise pattern",
        [
          [0, "PERIODIC"],
          [1, "WHITE"],
        ],
        part.sound[n],
        change,
      );
    return selectControl(f.label, values, part.sound[n], change);
  }
  return rangeControl(f.label, f.min, f.max, part.sound[n], change);
}
function renderControls() {
  for (const id of [
    "character-controls",
    "envelope-controls",
    "motion-controls",
    "macro-controls",
  ])
    $(id).replaceChildren();
  for (const f of visibleFields(part.sound)) {
    let target =
      f.group === "envelope"
        ? "envelope-controls"
        : f.group === "macro"
          ? "macro-controls"
          : f.group === "motion" ||
              ["vibrato", "tremolo", "lfo_rate", "glide", "mode"].includes(
                f.name,
              )
            ? "motion-controls"
            : "character-controls";
    $(target).append(fieldControl(f));
  }
  $("shape-buttons").replaceChildren();
  if (isEnv(part.sound.model))
    for (let i = 0; i < 8; i++) {
      const b = document.createElement("button");
      b.textContent = shapes[i];
      b.classList.toggle("active", part.sound.shape === i);
      b.onclick = () => {
        part.sound.shape = i;
        mark();
        renderControls();
      };
      $("shape-buttons").append(b);
    }
}
function renderMeta() {
  const m = part.sound.model,
    r = resourceInfo(part.sound, part.poly);
  $("resource-cost").textContent = r.cost;
  $("resource-detail").textContent = r.detail;
  $("resource-actual").textContent =
    part.resources !== undefined
      ? `本体確認済み 予約0x${part.resources.toString(16).padStart(4, "0")} / 出力0x${part.outputs.toString(16).padStart(4, "0")}`
      : dirty
        ? "未反映 · 回路の適合は本体への反映時に検証します"
        : "オフラインの構成見積もり";
  $("chip-label").textContent = isSaa(m) ? "SAA1099" : "DUAL SN76489";
  $("part-label").textContent =
    `PART ${String.fromCharCode(65 + part.part)} / ${part.channel === 127 ? "MULTI" : `CH ${part.channel + 1}`}${dirty || motionDirty ? " *" : ""}`;
  $("structure-description").textContent = descriptions[m];
  $("wave-note").textContent = isEnv(m)
    ? m === 14
      ? "緑がENV、橙が補助成分（指定NOTEからの経過時間に対する追加減衰）。合成前の成分を表示します。"
      : "ENV形状はチップ内蔵の8種類から選択。8 / 16段階で描きます。純ENVは波形の周期で音程が変わり、高音域に上限があります。"
    : "トーン／ノイズ／レイヤーの模式図です。ノイズの実際の系列・位相・アナログ出力を再現した波形ではありません。";
}
function renderModels() {
  const indices = Array.from({ length: 16 }, (_, i) => i).filter(
    (m) => isSaa(m) === (chip === "saa"),
  );
  $("models").replaceChildren();
  for (const i of indices) {
    const b = document.createElement("button");
    b.textContent = models[i].replace(/^SAA |^SN /, "");
    const s = document.createElement("span");
    s.textContent = resourceInfo({ ...part.sound, model: i }, 1).cost;
    b.append(s);
    b.classList.toggle("active", i === part.sound.model);
    b.onclick = () => {
      const f = factory.find((f) => fromValues(f.values).model === i);
      const old = part.sound;
      part.sound = f ? fromValues(f.values) : { ...old, model: i };
      part.poly = Math.min(part.poly, limit(i));
      part.role = roleFor(i);
      part.name = cleanName(models[i].replace(/^SAA |^SN /, "")).trim();
      mark();
      render();
    };
    $("models").append(b);
  }
  $("tab-saa").classList.toggle("active", chip === "saa");
  $("tab-sn").classList.toggle("active", chip === "sn");
  const old = $("factory").value;
  $("factory").replaceChildren();
  factory.forEach((f, i) => {
    if (isSaa(fromValues(f.values).model) === (chip === "saa"))
      $("factory").add(
        new Option(`F${String(i + 1).padStart(2, "0")} · ${f.name.trim()}`, i),
      );
  });
  if ([...$("factory").options].some((o) => o.value === old))
    $("factory").value = old;
}
function render() {
  chip = isSaa(part.sound.model) ? "saa" : "sn";
  $("part").value = part.part;
  $("name").value = part.name;
  $("channel").value = part.channel;
  $("poly").value = part.poly;
  $("poly").max = limit(part.sound.model);
  $("role").value = part.role;
  $("enabled").value = part.enabled;
  renderModels();
  renderControls();
  renderMeta();
  drawWave();
  drawEnvelope();
  renderMotion();
}
$("tab-saa").onclick = () => {
  chip = "saa";
  renderModels();
};
$("tab-sn").onclick = () => {
  chip = "sn";
  renderModels();
};
$("load-factory").onclick = () => {
  const f = factory[Number($("factory").value)];
  part.sound = fromValues(f.values);
  part.name = f.name.trim();
  part.poly = Math.min(part.poly, limit(part.sound.model));
  part.role = roleFor(part.sound.model);
  mark();
  render();
  message("音色をブラウザーへ読み込みました。「本体に反映」で送信します。");
};
$("name").oninput = () => {
  part.name = cleanName($("name").value).trim();
  mark();
};
for (const id of ["channel", "poly", "role", "enabled"])
  $(id).onchange = () => {
    part[id] = Number($(id).value);
    mark();
  };
for (let i = 0; i < 14; i++)
  $("part").add(
    new Option(
      `PART ${String.fromCharCode(65 + i)}${i < 4 ? [" · SAA", " · ENV", " · SN", " · SN NOISE"][i] : ""}`,
      i,
    ),
  );
for (let i = 0; i < 16; i++) $("channel").add(new Option(String(i + 1), i));
$("channel").add(new Option("MULTI / 変更しない", 127));
for (let i = 0; i < 16; i++)
  $("save-slot").add(new Option(`USER ${String(i + 1).padStart(2, "0")}`, i));
function storeCache() {
  cache.set(part.part, {
    part: structuredClone(part),
    motion: structuredClone(motion),
    dirty,
    motionDirty,
  });
}
async function readPart() {
  part = decodePart(await request(1, [part.part]));
  motion = decodeMotion(await request(8, [part.part]));
  dirty = motionDirty = false;
  storeCache();
  render();
  message("本体の音色・ステップを読み出しました。");
}
$("part").onchange = () =>
  guard(async () => {
    storeCache();
    const p = Number($("part").value),
      cached = cache.get(p);
    if (cached) {
      part = structuredClone(cached.part);
      motion = structuredClone(cached.motion);
      dirty = cached.dirty;
      motionDirty = cached.motionDirty;
    } else {
      part = freshPart(p, [3, 1, 7, 12][p] ?? 3);
      motion = newMotion(p);
      dirty = motionDirty = false;
    }
    render();
    if (ready && !cached) await readPart();
  });
$("read").onclick = () => guard(readPart);
async function sendPart() {
  const result = await request(2, encodePart(part));
  part = decodePart(result);
  dirty = false;
  storeCache();
  render();
  message(
    "本体RAMに反映しました。電源を切ると戻ります。保持するには「保存…」を使います。",
  );
}
$("send").onclick = () => guard(sendPart);
$("audition").onclick = () =>
  guard(async () => {
    await request(7, [part.part, Number($("test-note").value), 1]);
    message("本体の現在の音色を試聴します。音はLINE OUTから出ます。");
  });
$("stop").onclick = () =>
  guard(async () => {
    await request(7, [part.part, Number($("test-note").value), 0]);
    output.send([0xb0 | (part.channel === 127 ? 0 : part.channel), 120, 0]);
    message("試聴と指定MIDIチャンネルの発音を停止しました。");
  });
$("save-open").onclick = () => {
  if (dirty || motionDirty || clockDirty || cvDirty) {
    message(
      "未反映の編集があります。音色・ステップ・クロック・CVの各反映ボタンで送信してから保存してください。",
      true,
    );
    return;
  }
  $("save-dialog").showModal();
};
$("save-cancel").onclick = () => $("save-dialog").close();
$("save").onclick = () =>
  guard(async () => {
    await request(3, [part.part, Number($("save-slot").value)]);
    $("save-dialog").close();
    message(
      `USER ${Number($("save-slot").value) + 1}と全パートのセットアップを本体へ保存しました。`,
    );
  });
$("apply-profile").onclick = () => ($("profile-confirm").hidden = false);
$("cancel-profile").onclick = () => ($("profile-confirm").hidden = true);
$("confirm-profile").onclick = () =>
  guard(async () => {
    const profile = Number($("profile").value);
    if (ready) {
      await request(6, [profile]);
      cache.clear();
      dirty = motionDirty = false;
      part.part = 0;
      await readPart();
      message(
        "全パートをプロファイルで置き換えました。保持するには保存してください。",
      );
    } else {
      part = freshPart(0, profile === 2 ? 18 : 3);
      part.poly = profile === 1 ? 6 : 2;
      if (profile === 2) part.sound.mode = 0;
      motion = newMotion(0);
      cache.clear();
      dirty = true;
      render();
      message(
        "オフラインの構成例です。接続後にプロファイルを適用すると、本体の全パートが変わります。",
      );
    }
    $("profile-confirm").hidden = true;
  });
function setupCanvas(id) {
  const canvas = $(id),
    ctx = canvas.getContext("2d");
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.strokeStyle = "#2b3b30";
  ctx.lineWidth = 1;
  for (let x = 0; x < canvas.width; x += canvas.width / 16) {
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, canvas.height);
    ctx.stroke();
  }
  for (let y = canvas.height / 4; y < canvas.height; y += canvas.height / 4) {
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(canvas.width, y);
    ctx.stroke();
  }
  return [canvas, ctx];
}
function drawWave() {
  const [c, ctx] = setupCanvas("wave");
  const modes = [false];
  if (part.sound.invert && isEnv(part.sound.model)) modes.push(true);
  if (part.sound.model === 14) modes.push("layer");
  for (const mode of modes) {
    ctx.beginPath();
    for (let x = 0; x < c.width; x++) {
      const phase = (x / c.width) * 2,
        v =
          mode === "layer"
            ? layerWaveform(
                part.sound,
                phase,
                Number($("test-note").value) || 48,
              )
            : waveform(part.sound, phase, mode);
      const y = 25 + (1 - v) * (c.height - 50);
      if (x) ctx.lineTo(x, y);
      else ctx.moveTo(x, y);
    }
    ctx.strokeStyle =
      mode === "layer" ? "#dda879" : mode ? "#aab9c1" : "#d0ec8c";
    ctx.lineWidth = 2.5;
    ctx.stroke();
  }
}
function drawEnvelope() {
  const [c, ctx] = setupCanvas("envelope"),
    s = part.sound,
    pad = 20,
    hold = 400,
    total = Math.max(500, s.attack + s.decay + hold + s.release);
  envelopeScale = total / (c.width - 2 * pad);
  const pts = [
    [pad, c.height - pad],
    [pad + s.attack / envelopeScale, pad],
    [
      pad + (s.attack + s.decay) / envelopeScale,
      c.height - pad - (s.sustain / 100) * (c.height - 2 * pad),
    ],
    [
      pad + (s.attack + s.decay + hold) / envelopeScale,
      c.height - pad - (s.sustain / 100) * (c.height - 2 * pad),
    ],
    [c.width - pad, c.height - pad],
  ];
  ctx.beginPath();
  pts.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y)));
  ctx.lineTo(pad, c.height - pad);
  ctx.fillStyle = "#d0ec8c10";
  ctx.fill();
  ctx.strokeStyle = "#d0ec8c";
  ctx.lineWidth = 2;
  ctx.stroke();
  envelopeHandles = [pts[1], pts[2], pts[4]];
  for (const [x, y] of envelopeHandles) {
    ctx.beginPath();
    ctx.arc(x, y, 6, 0, Math.PI * 2);
    ctx.fillStyle = "#dda879";
    ctx.fill();
  }
}
function point(event, canvas) {
  const r = canvas.getBoundingClientRect();
  return [
    ((event.clientX - r.left) / r.width) * canvas.width,
    ((event.clientY - r.top) / r.height) * canvas.height,
  ];
}
let draggingEnvelope = -1,
  dragScale = 1;
const envCanvas = $("envelope");
envCanvas.onpointerdown = (e) => {
  const [x, y] = point(e, envCanvas);
  let distance = 60;
  envelopeHandles.forEach(([px, py], i) => {
    const d = Math.hypot(x - px, y - py);
    if (d < distance) {
      distance = d;
      draggingEnvelope = i;
    }
  });
  if (draggingEnvelope >= 0) {
    dragScale = envelopeScale;
    envCanvas.setPointerCapture(e.pointerId);
  }
};
envCanvas.onpointermove = (e) => {
  if (draggingEnvelope < 0) return;
  const [x, y] = point(e, envCanvas),
    s = part.sound,
    t = Math.max(0, (x - 20) * dragScale);
  if (draggingEnvelope === 0) s.attack = Math.round(Math.min(8000, t));
  if (draggingEnvelope === 1) {
    s.decay = Math.round(Math.max(0, Math.min(8000, t - s.attack)));
    s.sustain = Math.round(
      Math.max(
        0,
        Math.min(
          100,
          ((envCanvas.height - 20 - y) / (envCanvas.height - 40)) * 100,
        ),
      ),
    );
  }
  if (draggingEnvelope === 2)
    s.release = Math.round(
      Math.max(0, Math.min(8000, t - s.attack - s.decay - 400)),
    );
  mark();
  renderControls();
};
envCanvas.onpointerup = envCanvas.onpointercancel = () =>
  (draggingEnvelope = -1);
function renderMotion() {
  for (const key of ["length", "division", "direction", "sequence", "running"])
    $(key).value = motion[key];
  $("note-steps").replaceChildren();
  for (let i = 0; i < 16; i++) {
    const box = document.createElement("div");
    box.className = "note-cell";
    for (const [k, label, max] of [
      ["note", "NOTE", 127],
      ["velocity", "VEL", 127],
      ["gate", "GATE %", 100],
    ]) {
      const l = document.createElement("label");
      l.textContent = `${String(i + 1).padStart(2, "0")} ${label}`;
      const n = document.createElement("input");
      n.type = "number";
      n.min = 0;
      n.max = max;
      n.value = motion[k][i];
      n.setAttribute("aria-label", `Step ${i + 1} ${label}`);
      n.onchange = () => {
        motion[k][i] = Math.max(
          0,
          Math.min(max, Math.round(Number(n.value) || 0)),
        );
        motionDirty = true;
        renderMeta();
      };
      box.append(l, n);
    }
    $("note-steps").append(box);
  }
  drawSteps();
}
function drawSteps() {
  const [c, ctx] = setupCanvas("steps"),
    w = c.width / 16;
  for (let i = 0; i < 16; i++) {
    const v = motion.motion[i],
      y = c.height / 2 - (v / 100) * (c.height / 2 - 15);
    ctx.fillStyle = i < motion.length ? "#a6c66c" : "#3d4b3b";
    ctx.fillRect(
      i * w + 4,
      Math.min(y, c.height / 2),
      w - 8,
      Math.max(2, Math.abs(y - c.height / 2)),
    );
    ctx.fillStyle = "#c8d6b9";
    ctx.font = "11px monospace";
    ctx.fillText(String(v), i * w + 8, c.height - 5);
  }
}
for (const key of ["length", "division", "direction", "sequence", "running"])
  $(key).onchange = () => {
    const lo = key === "length" || key === "division" ? 1 : 0,
      hi =
        key === "length"
          ? 16
          : key === "division"
            ? 24
            : key === "direction"
              ? 2
              : 1;
    motion[key] = Math.max(
      lo,
      Math.min(hi, Math.round(Number($(key).value) || lo)),
    );
    motionDirty = true;
    renderMeta();
    drawSteps();
  };
let painting = false;
function paint(e) {
  const c = $("steps"),
    [x, y] = point(e, c),
    i = Math.max(0, Math.min(15, Math.floor((x / c.width) * 16)));
  motion.motion[i] = Math.round(
    Math.max(-100, Math.min(100, (0.5 - y / c.height) * 220)),
  );
  motionDirty = true;
  renderMeta();
  drawSteps();
}
$("steps").onpointerdown = (e) => {
  painting = true;
  $("steps").setPointerCapture(e.pointerId);
  paint(e);
};
$("steps").onpointermove = (e) => {
  if (painting) paint(e);
};
$("steps").onpointerup = $("steps").onpointercancel = () => (painting = false);
$("read-motion").onclick = () =>
  guard(async () => {
    motion = decodeMotion(await request(8, [part.part]));
    motionDirty = false;
    renderMotion();
    renderMeta();
    message("本体のステップを読み出しました。");
  });
$("send-motion").onclick = () =>
  guard(async () => {
    motion.part = part.part;
    motion = decodeMotion(await request(9, encodeMotion(motion)));
    motionDirty = false;
    renderMotion();
    renderMeta();
    message(
      "ステップとトランスポートを本体RAMへ反映しました。STOPは全体の演奏を停止します。保持するには保存してください。",
    );
  });
function renderClock() {
  const specs = [
    ["Reference", ["4 MHz", "2 MHz", "1 MHz"]],
    [
      "FX",
      [
        "OFF",
        "MANUAL",
        "RISE",
        "TRIANGLE",
        "SQUARE",
        "RANDOM",
        "SEQUENCE",
        "DIVE",
      ],
    ],
    ["Base drop · st", 0, 48],
    ["Depth · st", 0, 48],
    [
      "Rate",
      [
        "0.25 Hz",
        "0.5 Hz",
        "1 Hz",
        "2 Hz",
        "4 Hz",
        "8 Hz",
        "16 Hz",
        "32 Hz",
        "50 Hz",
        "80 Hz",
        "100 Hz",
        "160 Hz",
        "250 Hz",
        "400 Hz",
        "600 Hz",
        "800 Hz",
      ],
    ],
    [
      "Sync",
      [
        "FREE",
        "1 CLOCK",
        "2 CLOCKS",
        "3 CLOCKS",
        "4 CLOCKS",
        "6 CLOCKS",
        "8 CLOCKS",
        "12 CLOCKS",
        "24 CLOCKS",
      ],
    ],
    ["Retrigger", ["OFF", "ON"]],
    ["Pair width · cents", 0, 30],
    [
      "Trigger from",
      [
        "ANY SN",
        "MIDI SN",
        "CV1",
        "CV2",
        "DRUM CH10",
        "SN TONES",
        "SN NOISE",
        "MELODY / CV",
      ],
    ],
  ];
  $("clock-controls").replaceChildren();
  specs.forEach(([label, lo, hi], i) => {
    const cb = (v) => {
      clock[i] = v;
      clockDirty = true;
    };
    $("clock-controls").append(
      Array.isArray(lo)
        ? selectControl(
            label,
            lo.map((s, i) => [i, s]),
            clock[i],
            cb,
          )
        : rangeControl(label, lo, hi, clock[i], cb),
    );
  });
  for (let i = 0; i < 8; i++)
    $("clock-controls").append(
      rangeControl(`Clock step ${i + 1} · %`, 0, 100, clock[9 + i], (v) => {
        clock[9 + i] = v;
        clockDirty = true;
      }),
    );
}
$("read-clock").onclick = () =>
  guard(async () => {
    clock = await request(4);
    clockDirty = false;
    if (clock.length !== 17) throw Error("Clock形式が違います");
    renderClock();
    message("共通SNクロックを読み出しました。");
  });
$("send-clock").onclick = () =>
  guard(async () => {
    clock = await request(5, clock);
    clockDirty = false;
    renderClock();
    message(
      "左右全SNの共通クロックを反映しました。REFERENCE変更時は全体の演奏を停止します。",
    );
  });
const cvKeys = [
  "input",
  "mode",
  "part",
  "base_note",
  "scale",
  "root",
  "glide",
  "range",
  "gate",
  "gate_part",
  "parameter",
  "gate_note",
];
function cvDecode(d) {
  if (d.length !== 14) throw Error("CV形式が違います");
  return {
    ...Object.fromEntries(cvKeys.map((k, i) => [k, d[i]])),
    amount: readWord(d, 12) - 8192,
  };
}
function cvEncode(c) {
  return [...cvKeys.map((k) => c[k]), ...signed(c.amount)];
}
function renderCv() {
  $("cv-controls").replaceChildren();
  for (let i = 0; i < 2; i++) {
    const c = cvs[i],
      panel = document.createElement("div");
    panel.className = "cv-panel";
    const h = document.createElement("h3");
    h.textContent = `CV / GATE ${i + 1}`;
    const grid = document.createElement("div");
    grid.className = "control-grid";
    const parts = Array.from(
      { length: 14 },
      (_, p) => `PART ${String.fromCharCode(65 + p)}`,
    );
    const specs = [
      ["mode", "CV function", ["OFF", "NOTE · 1V/oct", "MOD"]],
      ["part", "CV to", parts],
      ["base_note", "0V note", 0, 127],
      [
        "scale",
        "Scale",
        [
          "OFF",
          "CHROMATIC",
          "MAJOR",
          "MINOR",
          "DORIAN",
          "MIN PENTA",
          "MAJ PENTA",
          "WHOLE TONE",
        ],
      ],
      [
        "root",
        "Root",
        Array.from(
          { length: 12 },
          (_, n) =>
            ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][
              n
            ],
        ),
      ],
      ["glide", "CV glide", enums.glide],
      ["range", "Input range", ["0–5 V", "0–10 V"]],
      [
        "gate",
        "Gate function",
        ["OFF", "NOTE", "RETRIGGER", "STEP", "FX TRIGGER"],
      ],
      ["gate_part", "Gate to", parts],
      ["parameter", "Mod parameter", parameters],
      ["gate_note", "Fixed gate note", 0, 127],
      ["amount", "Mod amount", -100, 100],
    ];
    for (const [key, label, lo, hi] of specs) {
      const cb = (v) => {
        c[key] = v;
        cvDirty = true;
      };
      grid.append(
        Array.isArray(lo)
          ? selectControl(
              label,
              lo.map((s, n) => [n, s]),
              c[key],
              cb,
            )
          : rangeControl(label, lo, hi, c[key], cb),
      );
    }
    panel.append(h, grid);
    $("cv-controls").append(panel);
  }
}
$("read-cv").onclick = () =>
  guard(async () => {
    for (let i = 0; i < 2; i++) cvs[i] = cvDecode(await request(10, [i]));
    cvDirty = false;
    renderCv();
    message("CV / Gateの割り当てを読み出しました。");
  });
$("send-cv").onclick = () =>
  guard(async () => {
    const data = await request(11, cvs.flatMap(cvEncode));
    cvs = [cvDecode(data.slice(0, 14)), cvDecode(data.slice(14))];
    cvDirty = false;
    renderCv();
    message("CV / Gateを本体へ反映しました。保持するには保存してください。");
  });
function download(name, data, type) {
  const url = URL.createObjectURL(new Blob([data], { type })),
    a = document.createElement("a");
  a.href = url;
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
$("export").onclick = () =>
  guard(async () => {
    encodePart(part);
    download(
      `${part.name.trim().replace(/[^A-Z0-9-]/g, "_") || "CHIPTUNE"}.json`,
      JSON.stringify(exportPatch(part, motion), null, 2),
      "application/json",
    );
    message(
      "音色とステップをJSONへ書き出しました。クロック・CV設定は含みません。",
    );
  });
$("import-open").onclick = () => $("import").click();
$("import").onchange = () =>
  guard(async () => {
    const f = $("import").files[0];
    if (!f) return;
    if (f.size > 100000) throw Error("ファイルが大きすぎます");
    const data = importPatch(JSON.parse(await f.text()));
    part = data.part;
    motion = data.motion || newMotion(part.part);
    dirty = true;
    motionDirty = !!data.motion;
    render();
    message("JSONをブラウザーへ読み込みました。本体にはまだ送信していません。");
    $("import").value = "";
  });
updateConnection();
render();
renderClock();
renderCv();
