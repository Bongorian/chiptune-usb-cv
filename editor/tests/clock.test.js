import test from "node:test";
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { SCHEMA, frame, parseFrame } from "../core.js";
import {
  clockDefault,
  clockPlan,
  directPlan,
  MIN_HZ,
  MAX_HZ,
  basePlan,
  encodeClock,
  decodeClock,
  decodeClockStatus,
  clockSimulator,
  fxNames,
} from "../clock.js";
function bridge(commands) {
  const r = spawnSync(process.env.EDITOR_BRIDGE, [], {
    input:
      [frame(0, 1, [SCHEMA]), ...commands]
        .map((b) => Buffer.from(b).toString("hex"))
        .join("\n") + "\n",
    encoding: "utf8",
  });
  assert.equal(r.status, 0, r.stderr);
  return r.stdout
    .trim()
    .split("\n")
    .map((s) => parseFrame(Buffer.from(s, "hex")));
}
test("PIO limits are derived; exact default and no 250k–4M restriction", () => {
  assert.equal(clockPlan(2000000).hz, 2000000);
  for (const hz of [MIN_HZ, 1e-5, 1, 100, 200000, 8000000, MAX_HZ])
    assert(clockPlan(hz).available);
  for (const hz of [0, -1, NaN, Infinity, MIN_HZ * 0.99, MAX_HZ * 1.01])
    assert(!clockPlan(hz).available);
  assert(clockPlan(8000000).experimental);
});
test("two-slot clock wire roundtrip agrees with firmware and survives project save/load", () => {
  const c = clockDefault();
  Object.assign(c, { base_hz: 0.00001, follow: 2 });
  Object.assign(c.mod[0], {
    mode: 8,
    offset: -1200,
    depth: 700,
    rate_millihz: 1799999,
  });
  Object.assign(c.mod[1], {
    mode: 10,
    domain: 2,
    depth: 600000,
    sync: 5,
    retrigger: 0,
  });
  const r = bridge([
    frame(26, 2, encodeClock(c)),
    frame(25, 3),
    frame(3, 4, [2, 0, 1]),
    frame(6, 5, [1]),
    frame(25, 6),
  ]);
  for (const a of r) assert.equal(a.status, 0);
  assert.deepEqual(decodeClock(r[1].data), c);
  assert.deepEqual(decodeClock(r[2].data), c);
  assert.deepEqual(decodeClock(r[5].data), c);
});
test("direct PIO extrema, unavailable input and version protection never alter settings", () => {
  const c = clockDefault();
  Object.assign(c, {
    direct: 1,
    period: 0xffffffff,
    div256: 16777216,
    stopped: 1,
  });
  const bytes = encodeClock(c),
    bad = [...bytes];
  bad[25] = 127;
  const r = bridge([
    frame(26, 2, bytes),
    frame(26, 3, bad),
    frame(25, 4),
    frame(27, 5),
  ]);
  assert.equal(r[1].status, 0);
  assert.equal(r[2].status, 2);
  assert.deepEqual(decodeClock(r[3].data), c);
  const d = decodeClockStatus(r[4].data);
  assert.equal(d.base_hz, directPlan(c.period, c.div256).hz);
  assert.throws(() => encodeClock({ ...c, direct: 0, base_hz: MAX_HZ + 1 }));
});
test("graph uses musical cents, physical Hz/ratio/period and stop states", () => {
  const c = clockDefault();
  c.mod[0].mode = 1;
  c.mod[0].offset = 1200;
  assert.equal(clockSimulator(c)(0).hz, 4000000);
  c.mod[0].domain = 1;
  c.mod[0].offset = -1999900;
  assert.equal(clockSimulator(c)(0).hz, 100);
  c.mod[0].domain = 2;
  c.mod[0].offset = 500000;
  assert(Math.abs(clockSimulator(c)(0).hz - 3000000) < 3000);
  c.mod[0].domain = 3;
  c.mod[0].offset = 32;
  assert.equal(clockSimulator(c)(0).hz, 1000000);
  c.stopped = 1;
  assert.equal(clockSimulator(c)(0).hz, 0);
  for (let mode = 0; mode < fxNames.length; mode++) {
    const p = clockDefault();
    p.mod[0].mode = mode;
    const sim = clockSimulator(p);
    for (let i = 0; i < 100; i++) assert(sim(0.001).available);
  }
});
