// Isolated headless browser. MIDI ports below run the portable C firmware, never a physical device.
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || "playwright");
const assert = require("node:assert/strict");
const { spawnSync } = require("node:child_process");
(async () => {
  const browser = await chromium.launch({
    executablePath:
      process.env.CHROME_BIN ||
      "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
    headless: true,
  });
  const context = await browser.newContext({
      viewport: { width: 1440, height: 1080 },
    }),
    page = await context.newPage();
  const errors = [];
  page.on("pageerror", (e) => errors.push(e.message));
  page.setDefaultTimeout(10000);
  const history = [];
  assert(process.env.EDITOR_BRIDGE);
  await page.exposeFunction("firmwareMessage", (bytes) => {
    history.push(Buffer.from(bytes).toString("hex"));
    const r = spawnSync(process.env.EDITOR_BRIDGE, [], {
      input: history.join("\n") + "\n",
      encoding: "utf8",
    });
    assert.equal(r.status, 0, r.stderr);
    return [...Buffer.from(r.stdout.trim().split("\n").at(-1), "hex")];
  });
  await page.addInitScript(() => {
    const input = {
      id: "in",
      name: "Chiptune v0.10",
      state: "connected",
      open: async () => {},
      onmidimessage: null,
    };
    const output = {
      id: "out",
      name: "Chiptune v0.10",
      state: "connected",
      open: async () => {},
      send: (bytes) =>
        window
          .firmwareMessage([...bytes])
          .then((data) =>
            input.onmidimessage?.({ data: Uint8Array.from(data) }),
          ),
    };
    Object.defineProperty(navigator, "requestMIDIAccess", {
      value: async () => ({
        inputs: new Map([["in", input]]),
        outputs: new Map([["out", output]]),
      }),
    });
  });
  await page.goto(process.env.EDITOR_URL || "http://127.0.0.1:8766");
  await page.locator("#track-level").waitFor();
  assert.equal(await page.locator("#name").inputValue(), "SAA TONE");
  assert.equal(await page.locator("#track-mode").inputValue(), "0");
  assert.match(
    await page.locator("#test-frequency").textContent(),
    /110.00 Hz/,
  );
  await page.locator("#track-level").evaluate((el) => {
    el.value = 75;
    el.dispatchEvent(new Event("input", { bubbles: true }));
  });
  assert.match(await page.locator("#track-label").textContent(), /未反映/);
  const downloadPromise = page.waitForEvent("download");
  await page.locator("#export").click();
  const download = await downloadPromise;
  const fs = require("node:fs");
  const exported = JSON.parse(fs.readFileSync(await download.path()));
  assert.equal(exported.schema, 10);
  assert.equal(exported.track.level, 75);
  assert.equal(await page.locator("#patch-wave option").count(), 5);
  assert(
    !(await page
      .locator("#patch-wave")
      .textContent()
      .then((t) => t.includes("PULSE"))),
  );
  await page.locator("#patch-wave").selectOption({ label: "ENV WAVE" });
  assert.equal(await page.locator("#patch-shape").count(), 1);
  assert.equal(await page.locator("#patch-env_steps").count(), 1);
  assert.equal(await page.locator("#patch-detune").count(), 0);
  await page.locator("#patch-wave").selectOption({ label: "TONE + NOISE" });
  assert.equal(await page.locator("#patch-noise_rate").count(), 1);
  assert.equal(await page.locator("#patch-noise_mix").count(), 0);
  assert.equal(await page.locator("#patch-noise_type").count(), 0);
  await page.locator("#patch-wave").selectOption({ label: "SQUARE 50%" });
  await page.screenshot({
    path: "/tmp/chiptune-v10-offline.png",
    fullPage: true,
  });
  await page.locator("#connect").click();
  await page.locator("#connection-state.connected").waitFor();
  await page.waitForFunction(
    () => document.querySelector("#track-level").value === "75",
  );
  await page.locator("#read").click();
  await page.waitForFunction(
    () => document.querySelector("#track-level").value === "100",
  );
  await page.locator("#track-level").evaluate((el) => {
    el.value = 55;
    el.dispatchEvent(new Event("input", { bubbles: true }));
  });
  await page.locator("#send").click();
  await page
    .getByRole("status")
    .filter({ hasText: "本体RAMへ反映しました" })
    .waitFor();
  await page.locator("#read").click();
  await page.waitForFunction(
    () => document.querySelector("#track-level").value === "55",
  );
  await page.locator("#factory").selectOption("18");
  await page.locator("#load-factory").click();
  await page.locator("#send").click();
  await page.getByRole("status").filter({ hasText: "代替案" }).waitFor();
  await page.locator("#read").click();
  await page.waitForFunction(
    () => document.querySelector("#name").value === "SAA TONE",
  );
  await page.locator('[data-track="6"]').click();
  await page.waitForFunction(
    () => document.querySelector("#name").value === "SN MONO",
  );
  assert.equal(await page.locator("#patch-detune").inputValue(), "0");
  assert.equal(await page.locator("#patch-wave option").count(), 4);
  assert(
    !(await page
      .locator("#patch-wave")
      .textContent()
      .then((t) => t.includes("ENV WAVE"))),
  );
  await page.locator("#factory").selectOption("22");
  await page.locator("#load-factory").click();
  for (const id of [
    "patch-detune",
    "patch-glide",
    "patch-vibrato",
    "patch-pitch_amount",
    "patch-shape",
  ])
    assert.equal(await page.locator("#" + id).count(), 0, id);
  await page.locator("#read").click();
  await page.waitForFunction(
    () => document.querySelector("#name").value === "SN MONO",
  );

  await page.locator("#track-pan").evaluate((el) => {
    el.value = 0;
    el.dispatchEvent(new Event("input", { bubbles: true }));
  });
  await page.locator("#send").click();
  await page
    .getByRole("status")
    .filter({ hasText: "本体RAMへ反映しました" })
    .waitFor();
  await page.locator('[data-track="0"]').click();
  await page.locator("#track-level").evaluate((el) => {
    el.value = 77;
    el.dispatchEvent(new Event("input", { bubbles: true }));
  });
  await page.locator('[data-track="6"]').click();
  await page.waitForFunction(
    () => document.querySelector("#name").value === "SN MONO",
  );
  await page.locator("#poly-focus").click();
  await page.locator("#replace-confirm").click();
  await page
    .getByRole("status")
    .filter({ hasText: "選択TrackをPOLY化しました" })
    .waitFor();
  assert.equal(await page.locator("#track-mode").inputValue(), "1");
  assert.equal(await page.locator("#track-voices").inputValue(), "3");
  assert(
    await page
      .locator('[data-track="7"]')
      .evaluate((el) => el.classList.contains("muted")),
  );
  await page.locator("#track-mode").selectOption("0");
  await page.locator("#track-voices").evaluate((el) => {
    el.value = 1;
    el.dispatchEvent(new Event("input", { bubbles: true }));
  });
  await page.locator("#patch-wave").selectOption({ label: "PULSE ~6.7%" });
  assert.equal(await page.locator("#patch-noise_rate").count(), 0);
  assert.equal(await page.locator("#patch-noise_type").count(), 0);
  assert.equal(await page.locator("#patch-noise_pitch").count(), 1);
  assert.match(await page.locator("#wave-hint").textContent(), /1\/15/);
  await page.locator("#send").click();
  await page
    .getByRole("status")
    .filter({ hasText: "本体RAMへ反映しました" })
    .waitFor();
  assert.match(await page.locator("#resource-cost").textContent(), /4回路/);
  assert.match(
    await page.locator("#resource-actual").textContent(),
    /SN L noise/,
  );
  assert.match(await page.locator("#resource-actual").textContent(), /SN L3/);
  await page.screenshot({
    path: "/tmp/chiptune-v10-pulse.png",
    fullPage: true,
  });
  await page.locator('[data-track="0"]').click();
  assert.equal(await page.locator("#track-level").inputValue(), "77");
  await page.locator('[data-track="6"]').click();
  await page.waitForFunction(
    () => document.querySelector("#name").value === "SN MONO",
  );
  await page.locator("aside details>summary").click();
  await page.locator("#template").selectOption("1");
  await page.locator("#apply-template").click();
  await page.locator("#replace-confirm").click();
  await page
    .getByRole("status")
    .filter({ hasText: "配置をRAMへ置き換えました" })
    .waitFor();
  await page.locator('[data-track="0"]').click();
  await page.waitForFunction(
    () => document.querySelector("#track-mode").value === "1",
  );
  assert.equal(await page.locator("#track-voices").inputValue(), "6");
  await page.locator("#advanced>summary").click();
  await page.locator("#morph-target").selectOption("4");
  await page.locator("#morph-amount").evaluate((el) => {
    el.value = 100;
    el.dispatchEvent(new Event("input", { bubbles: true }));
  });
  await page.locator("#send").click();
  await page
    .getByRole("status")
    .filter({ hasText: "本体RAMへ反映しました" })
    .waitFor();
  await page.locator("#motion-panel>summary").click();
  await page.getByLabel("NOTE step 1", { exact: true }).fill("48");
  await page.getByLabel("NOTE step 1", { exact: true }).dispatchEvent("change");
  await page.locator("#send-sequence").click();
  await page
    .getByRole("status")
    .filter({ hasText: "SequenceをRAMへ反映" })
    .waitFor();
  await page.locator("#clock-panel>summary").click();
  await page.locator("#read-global").click();
  await page
    .getByRole("status")
    .filter({ hasText: "共通設定を読み出しました" })
    .waitFor();
  await page.locator("#send-global").click();
  await page
    .getByRole("status")
    .filter({ hasText: "SNクロックと共通設定" })
    .waitFor();
  await page.locator("#cv-panel>summary").click();
  await page.locator("#read-cv").click();
  await page
    .getByRole("status")
    .filter({ hasText: "CV/Gateを読み出しました" })
    .waitFor();
  await page.locator("#send-cv").click();
  await page
    .getByRole("status")
    .filter({ hasText: "CV/Gateの2系統" })
    .waitFor();
  await page.locator("#save-open").click();
  await page.locator("#save-kind").selectOption("0");
  await page.locator("#save-slot").selectOption("5");
  assert.match(
    await page.locator("#save-scope").textContent(),
    /Project.*変更しません/,
  );
  await page.locator("#save").click();
  await page
    .getByRole("status")
    .filter({ hasText: "Patchのみ保存しました" })
    .waitFor();
  // LIVE sends latest state without replacing a slider DOM node on every input.
  await page.locator("#live").check();
  await page.locator("#track-level").evaluate((el) => {
    window.sliderBefore = el;
    el.value = 67;
    el.dispatchEvent(new Event("input", { bubbles: true }));
    el.value = 68;
    el.dispatchEvent(new Event("input", { bubbles: true }));
    if (window.sliderBefore !== document.querySelector("#track-level"))
      throw Error("Slider was replaced during drag");
  });
  await page.waitForFunction(
    () =>
      !document.querySelector("#track-label").textContent.includes("未反映"),
  );
  assert(
    await page.evaluate(
      () => window.sliderBefore === document.querySelector("#track-level"),
    ),
  );
  await page.locator("#live").uncheck();
  await page.setViewportSize({ width: 390, height: 844 });
  await page.screenshot({
    path: "/tmp/chiptune-v10-mobile.png",
    fullPage: true,
  });
  assert(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= innerWidth + 1,
    ),
  );
  assert.deepEqual(errors, []);
  // A v0.9-like protocol-1 responder must never receive a setting write.
  const old = await browser.newContext(),
    oldPage = await old.newPage();
  let oldWrites = 0;
  await oldPage.exposeFunction("oldMessage", (bytes) => {
    if (bytes[6] !== 0) oldWrites++;
    const b = [125, 67, 84, 1, bytes[5], 64, 0, 0, 9, 0, 6, 14, 16, 23, 39];
    return [240, ...b, -b.reduce((a, v) => a + v, 0) & 127, 247];
  });
  await oldPage.addInitScript(() => {
    const input = {
      id: "in",
      name: "Chiptune v0.9",
      state: "connected",
      open: async () => {},
    };
    const output = {
      id: "out",
      name: "Chiptune v0.9",
      state: "connected",
      open: async () => {},
      send: (bytes) =>
        window
          .oldMessage([...bytes])
          .then((data) =>
            input.onmidimessage?.({ data: Uint8Array.from(data) }),
          ),
    };
    Object.defineProperty(navigator, "requestMIDIAccess", {
      value: async () => ({
        inputs: new Map([["in", input]]),
        outputs: new Map([["out", output]]),
      }),
    });
  });
  let legacyShellServed = false;
  // A cached v0.10 HTML shell may not contain the new optional hint element.
  await oldPage.route(
    new URL(process.env.EDITOR_URL || "http://127.0.0.1:8766").href,
    async (route) => {
      legacyShellServed = true;
      const response = await route.fetch();
      await route.fulfill({
        response,
        body: (await response.text()).replace(
          /<p id="wave-hint" class="hint"><\/p>/,
          "",
        ),
      });
    },
  );
  await oldPage.goto(process.env.EDITOR_URL || "http://127.0.0.1:8766");
  assert(legacyShellServed);
  await oldPage.locator("#wave-hint").waitFor();
  await oldPage.locator("#connect").click();
  await oldPage.locator("#message.error").waitFor();
  assert(await oldPage.locator("#send").isDisabled());
  assert.equal(oldWrites, 0);
  await browser.close();
  console.log(
    "PASS browser: offline editing/export, protocol-2 real-C sync, resource rejection, DIRECT/POLY, PAN, morph, sequence/clock/CV, independent patch save, LIVE coalescing, old-version write lock, chip-specific wave controls and Pulse/Tone3 reservations, mobile layout",
  );
})().catch((e) => {
  console.error(e);
  process.exit(1);
});
