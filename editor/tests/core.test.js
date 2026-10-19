import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import {
  fields,
  models,
  fromValues,
  validateSound,
  frame,
  parseFrame,
  word,
  signed,
  readWord,
  encodePart,
  decodePart,
  encodeMotion,
  decodeMotion,
  exportPatch,
  importPatch,
  visibleFields,
  waveform,
  resourceInfo,
} from "../core.js";
const factory = JSON.parse(
  readFileSync(new URL("../factory.json", import.meta.url)),
);
const patch = () => ({
  part: 0,
  enabled: 1,
  poly: 2,
  role: 2,
  channel: 0,
  name: "ATK BASS",
  sound: fromValues(factory[18].values),
});
const motion = () => ({
  part: 0,
  length: 16,
  division: 6,
  direction: 0,
  sequence: 1,
  arp: 0,
  arp_octaves: 1,
  running: 1,
  note: Array(16).fill(48),
  gate: Array(16).fill(70),
  velocity: Array(16).fill(85),
  motion: Array.from({ length: 16 }, (_, i) => (i % 2 ? -100 : 100)),
});
test("all factory structures conform to schema", () => {
  assert.equal(factory.length, 20);
  for (const f of factory)
    assert.doesNotThrow(() => validateSound(fromValues(f.values)));
  assert.equal(models.length, 16);
});
test("every structure has resource guidance", () => {
  for (let m = 0; m < 16; m++)
    assert(resourceInfo({ ...patch().sound, model: m }, 1).cost);
});
test("signed 14-bit endpoints retain full precision", () => {
  for (const v of [-8192, -100, -48, 0, 100, 8000, 8191])
    assert.equal(readWord(signed(v)) - 8192, v);
  assert.throws(() => word(16384));
});
test("SysEx checksum, identifiers and malformed payload rejection", () => {
  const b = frame(64, 17, [0, 0, 8, 0]);
  const r = parseFrame(b);
  assert.equal(r.id, 17);
  assert.equal(r.status, 0);
  b[8] ^= 1;
  assert.throws(() => parseFrame(b));
  assert.throws(() => frame(2, 1, [128]));
});
test("motion preserves bipolar extrema and rejects out of range values", () => {
  const m = motion();
  assert.deepEqual(decodeMotion(encodeMotion(m)), m);
  m.motion[0] = 101;
  assert.throws(() => encodeMotion(m));
  assert.throws(() => decodeMotion([1]));
});
test("JSON format validates and isolates imported state", () => {
  const p = patch(),
    m = motion(),
    data = exportPatch(p, m),
    copy = importPatch(JSON.parse(JSON.stringify(data)));
  copy.part.sound.attack = 700;
  assert.notEqual(p.sound.attack, 700);
  assert.throws(() => importPatch({ version: 99 }));
  data.part.sound.attack = NaN;
  assert.throws(() => importPatch(data));
});
test("only relevant parameters are shown, ENV and noise are separate", () => {
  const s = patch().sound;
  s.layer_source = 0;
  let names = visibleFields(s).map((f) => f.name);
  assert(names.includes("shape"));
  assert(names.includes("layer_interval"));
  assert(!names.includes("noise_mix"));
  assert(!names.includes("noise_rate"));
  s.layer_source = 1;
  s.noise_rate = 3;
  names = visibleFields(s).map((f) => f.name);
  assert(names.includes("layer_interval"));
  s.model = 0;
  names = visibleFields(s).map((f) => f.name);
  assert(!names.includes("shape"));
  assert(!names.includes("detune"));
});
test("schematic waveforms remain bounded for every factory", () => {
  for (const f of factory)
    for (let i = 0; i < 1000; i++) {
      const s = fromValues(f.values),
        v = waveform(s, i / 137, true);
      assert(Number.isFinite(v) && v >= 0 && v <= 1);
    }
});
test("browser wire format roundtrips through real firmware", () => {
  const bridge = process.env.EDITOR_BRIDGE;
  if (!bridge) {
    assert.fail("Set EDITOR_BRIDGE to the host editor_bridge executable");
  }
  const p = patch(),
    m = motion();
  const payloads = [
    frame(0, 1),
    frame(6, 2, [2]),
    frame(2, 3, encodePart(p)),
    frame(1, 4, [0]),
    frame(9, 5, encodeMotion(m)),
    frame(8, 6, [0]),
  ];
  const run = spawnSync(bridge, [], {
    input:
      payloads.map((p) => Buffer.from(p).toString("hex")).join("\n") + "\n",
    encoding: "utf8",
  });
  assert.equal(run.status, 0, run.stderr);
  const replies = run.stdout
    .trim()
    .split("\n")
    .map((line) => parseFrame(Buffer.from(line, "hex")));
  assert.equal(replies.length, 6);
  for (const r of replies) assert.equal(r.status, 0);
  const got = decodePart(replies[3].data);
  assert.deepEqual(got.sound, p.sound);
  assert.equal(got.name, p.name);
  assert.equal(got.resources, 63);
  assert.deepEqual(decodeMotion(replies[5].data), m);
});
