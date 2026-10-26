import test from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import { spawnSync } from "node:child_process";
import {
  SCHEMA,
  fields,
  encodeGlobal,
  decodeGlobal,
  validatePatch,
  newTrack,
  newSequence,
  frame,
  parseFrame,
  verifyHello,
  encodeTrack,
  decodeTrack,
  encodeSequence,
  decodeSequence,
  encodeCv,
  decodeCv,
  newCv,
  exportTrack,
  importTrack,
  sameStructure,
  supported,
  normalizePatch,
  supportedMod,
  Wave,
  waveform,
  availableWaves,
  selectWave,
  featureMask,
  musicalParameters,
  Feature,
} from "../core.js";
const { factories, waveforms } = JSON.parse(
  fs.readFileSync(new URL("../factory.json", import.meta.url)),
);
function bridge(messages) {
  assert(process.env.EDITOR_BRIDGE, "EDITOR_BRIDGE is required");
  const r = spawnSync(process.env.EDITOR_BRIDGE, [], {
    input:
      messages.map((b) => Buffer.from(b).toString("hex")).join("\n") + "\n",
    encoding: "utf8",
  });
  assert.equal(r.status, 0, r.stderr);
  return r.stdout
    .trim()
    .split("\n")
    .map((s) => parseFrame(Buffer.from(s, "hex")));
}
test("all compositional factory patches validate", () => {
  assert.equal(factories.length, 23);
  for (const f of factories) assert.equal(validatePatch(f.patch), f.patch);
  assert.equal(fields.length, Object.keys(factories[0].patch).length);
});
test("MIDI10 defaults have independent direct tracks", () => {
  for (let i = 0; i < 14; i++) {
    const t = newTrack(i, factories);
    assert.equal(t.mode, 0);
    assert.equal(t.enabled, i < 10 ? 1 : 0);
    assert.equal(t.usb, i < 10 ? 1 << i : 0);
    assert.equal(t.patch.detune, 0);
    assert.equal(t.voices, 1);
  }
});
test("wire schema and checksum reject incompatible versions", () => {
  const b = frame(0, 1, [SCHEMA]);
  assert.equal(b[4], 2);
  const r = bridge([b])[0];
  assert.equal(r.status, 0);
  verifyHello(r.data);
  assert.throws(() => verifyHello([0, 9, 0, 1, 9]));
  const bad = Uint8Array.from([240, 125, 67, 84, 1, 1, 64, 0, 0, 247]);
  assert.throws(() => parseFrame(bad));
});
test("real C Track roundtrip including signed fields and MIDI ch16", () => {
  const t = newTrack(0, factories);
  t.name = "WEB TEST";
  t.usb = 32768;
  t.trs = 65535;
  t.patch.pitch_amount = -48;
  t.macro = [-100, 100, 0, 17];
  const r = bridge([
    frame(0, 1, [10]),
    frame(2, 2, encodeTrack(t)),
    frame(1, 3, [0]),
  ]);
  for (const a of r) assert.equal(a.status, 0);
  const got = decodeTrack(r[2].data);
  for (const k of ["usb", "trs", "macro", "patch", "name"])
    assert.deepEqual(got[k], t[k]);
});
test("mutations before handshake and schema mismatch cannot change C state", () => {
  const t = newTrack(0, factories);
  t.level = 1;
  const r = bridge([
    frame(2, 1, encodeTrack(t)),
    frame(0, 2, [9]),
    frame(2, 3, encodeTrack(t)),
    frame(0, 4, [10]),
    frame(1, 5, [0]),
  ]);
  assert.deepEqual(
    r.map((x) => x.status),
    [5, 5, 5, 0, 0],
  );
  assert.equal(decodeTrack(r[4].data).level, 100);
});
test("sequence and CV signed endpoints roundtrip through firmware", () => {
  const q = newSequence(0);
  q.motion[0] = -100;
  q.motion[1] = 100;
  const c = newCv(0);
  c.amount = -100;
  const r = bridge([
    frame(0, 1, [10]),
    frame(9, 2, encodeSequence(q)),
    frame(11, 3, encodeCv(c)),
  ]);
  assert.equal(r[1].status, 0);
  assert.deepEqual(decodeSequence(r[1].data), q);
  assert.deepEqual(decodeCv(r[2].data), c);
});
test("resource errors preserve prior voices and explain alternatives", () => {
  const t = newTrack(0, factories);
  t.patch = structuredClone(factories[18].patch);
  const r = bridge([
    frame(0, 1, [10]),
    frame(2, 2, encodeTrack(t)),
    frame(1, 3, [0]),
  ]);
  assert.equal(r[1].status, 2);
  assert(r[1].data[4] > 0);
  assert.deepEqual(decodeTrack(r[2].data).patch, newTrack(0, factories).patch);
});
test("JSON schema is explicit and imported state is isolated", () => {
  const t = newTrack(0, factories),
    q = newSequence(0),
    data = exportTrack(t, q);
  const got = importTrack(data, 5);
  assert.equal(got.track.track, 5);
  got.track.patch.level = 2;
  assert.notEqual(t.patch.level, 2);
  assert.throws(() => importTrack({ ...data, schema: 9 }, 0));
});
test("capabilities and morph topology reflect real chips", () => {
  const saa = factories[20].patch,
    sn = factories[21].patch;
  assert(!supported(saa, "detune"));
  assert(supported(sn, "detune"));
  assert(!supported(sn, "shape"));
  assert(!sameStructure(saa, sn));
  assert(sameStructure(saa, { ...saa, attack: 800 }));
  assert.throws(() => validatePatch({ ...sn, env: 1 }));
});

test("global tempo 240 BPM and Gate2 roundtrip without 7-bit truncation", () => {
  const read = bridge([frame(0, 1, [10]), frame(4, 2)])[1];
  const v = decodeGlobal(read.data);
  v[10] = 210;
  v[24] = 1;
  const r = bridge([
    frame(0, 1, [10]),
    frame(5, 2, encodeGlobal(v)),
    frame(4, 3),
  ]);
  assert.equal(r[1].status, 0);
  assert.deepEqual(decodeGlobal(r[2].data), v);
});

test("POLY focus disables only the same chip and returns assigned poly voices", () => {
  const r = bridge([
    frame(0, 1, [10]),
    frame(24, 2, [6]),
    frame(1, 3, [0]),
    frame(1, 4, [7]),
  ]);
  assert.equal(r[1].status, 0);
  assert.equal(decodeTrack(r[1].data).voices, 3);
  assert.equal(decodeTrack(r[1].data).mode, 1);
  assert.equal(decodeTrack(r[2].data).enabled, 1);
  assert.equal(decodeTrack(r[3].data).enabled, 0);
});

test("structure editing normalizes incompatible layers and unused noise controls", () => {
  const p = structuredClone(factories[18].patch);
  p.env = 0;
  normalizePatch(p, "env");
  assert.equal(p.layers, 1);
  assert.equal(p.env, 0);
  const n = structuredClone(factories[22].patch);
  n.noise_rate = 3;
  n.noise_pitch = 12;
  n.source = 0;
  normalizePatch(n, "source");
  assert.equal(n.noise_pitch, 0);
  const t = newTrack(0, factories);
  assert(!supportedMod(t, 7));
  assert(!supportedMod(t, 6));
  t.patch.output = 1;
  assert(!supportedMod(t, 2));
});

test("C and Web agree on wave classification and capability masks for every factory", () => {
  for (const f of factories) {
    assert.equal(waveform(f.patch), f.wave, f.name);
    assert.equal(featureMask(f.patch), f.features, f.name);
  }
});
test("C and Web construct the same chip-specific waveforms without new stored fields", () => {
  assert.equal(waveforms.length, 9);
  for (const spec of waveforms) {
    const p = structuredClone(
      waveforms.find((w) => w.chip === spec.chip && w.wave === Wave.SQUARE)
        .patch,
    );
    selectWave(p, spec.wave);
    assert.deepEqual(p, spec.patch);
    assert.equal(featureMask(p), spec.features);
  }
  const sn = structuredClone(factories[21].patch),
    before = structuredClone(sn);
  assert.throws(() => selectWave(sn, Wave.ENV));
  assert.deepEqual(sn, before);
  assert.deepEqual(availableWaves(0), [
    Wave.SQUARE,
    Wave.ENV,
    Wave.TONE_NOISE,
    Wave.NOISE,
    Wave.AM,
  ]);
  assert.deepEqual(availableWaves(1), [
    Wave.SQUARE,
    Wave.PULSE,
    Wave.NOISE,
    Wave.METAL,
  ]);
});
test("musical controls exclude unsupported or ineffective chip parameters", () => {
  const sn = structuredClone(factories[22].patch);
  let names = musicalParameters(sn);
  for (const n of [
    "shape",
    "env_ratio",
    "detune",
    "glide",
    "pitch_amount",
    "vibrato",
    "noise_pitch",
  ])
    assert(!names.includes(n), n);
  assert(!supportedMod({ patch: sn }, 3));
  assert(!supportedMod({ patch: sn }, 4));
  selectWave(sn, Wave.PULSE);
  names = musicalParameters(sn);
  for (const n of ["noise_type", "noise_rate", "shape", "noise_mix"])
    assert(!names.includes(n), n);
  for (const n of ["noise_pitch", "noise_sweep", "glide", "detune"])
    assert(names.includes(n), n);
  const saa = structuredClone(factories[20].patch);
  selectWave(saa, Wave.TONE_NOISE);
  names = musicalParameters(saa);
  assert(names.includes("noise_rate"));
  assert(!names.includes("noise_mix"));
  assert(!names.includes("noise_type"));
  assert(!names.includes("detune"));
  selectWave(saa, Wave.ENV);
  assert(musicalParameters(saa).includes("shape"));
  saa.env = 2;
  saa.shape = 0;
  assert(!(featureMask(saa) & Feature.ENV_RATIO));
});
