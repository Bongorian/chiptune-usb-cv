// Headless regression tests for this app only; never attaches to a user's browser profile.
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || "playwright");
const assert = require("node:assert/strict");
(async () => {
  const browser = await chromium.launch({
    executablePath: process.env.CHROME_BIN || "/opt/google/chrome/chrome",
    headless: true,
    args: ["--no-sandbox"],
  });
  const page = await browser.newPage({
    viewport: { width: 1440, height: 1080 },
  });
  const errors = [];
  page.on("pageerror", (e) => {
    errors.push(e.message);
    console.error("PAGE ERROR:", e.stack);
  });
  page.setDefaultTimeout(10000);
  if (process.env.EDITOR_BRIDGE) {
    const { spawnSync } = require("node:child_process");
    const history = [];
    await page.exposeFunction("firmwareMessage", (bytes) => {
      history.push(Buffer.from(bytes).toString("hex"));
      const run = spawnSync(process.env.EDITOR_BRIDGE, [], {
        input: history.join("\n") + "\n",
        encoding: "utf8",
      });
      assert.equal(run.status, 0, run.stderr);
      const line = run.stdout.trim().split("\n").at(-1);
      return Array.from(Buffer.from(line, "hex"));
    });
    await page.addInitScript(() => {
      const input = {
        id: "chip-in",
        name: "Chiptune USB/CV",
        manufacturer: "Test firmware",
        state: "connected",
        open: async () => {},
        onmidimessage: null,
      };
      const output = {
        id: "chip-out",
        name: "Chiptune USB/CV",
        manufacturer: "Test firmware",
        state: "connected",
        open: async () => {},
        send: (bytes) => {
          window
            .firmwareMessage(Array.from(bytes))
            .then((data) =>
              input.onmidimessage?.({ data: new Uint8Array(data) }),
            );
        },
      };
      Object.defineProperty(navigator, "requestMIDIAccess", {
        value: async () => ({
          inputs: new Map([[input.id, input]]),
          outputs: new Map([[output.id, output]]),
          onstatechange: null,
        }),
      });
    });
  }
  await page.goto(process.env.EDITOR_URL || "http://127.0.0.1:8765");
  await page.getByRole("button", { name: "SAA1099", exact: true }).waitFor();
  await page
    .getByRole("button", { name: "ATK BASS", exact: true })
    .count()
    .catch(() => 0);
  await page.locator("#factory").selectOption("18");
  await page
    .getByRole("button", { name: "音色を読み込む", exact: true })
    .click();
  assert.equal(await page.locator("#name").inputValue(), "ATK BASS");
  await page.getByRole("button", { name: "SAW DOWN", exact: true }).click();
  assert(
    await page
      .getByRole("button", { name: "SAW DOWN", exact: true })
      .getAttribute("class")
      .then((c) => c.includes("active")),
  );
  await page.locator("#tab-sn").click();
  await page.locator("#factory").selectOption("19");
  await page
    .getByRole("button", { name: "音色を読み込む", exact: true })
    .click();
  assert.equal(await page.locator("#name").inputValue(), "SN STACK");
  assert(
    (await page.locator("#resource-cost").textContent()).includes("SN 2組"),
  );
  await page.locator("#tab-saa").click();
  await page.locator("#factory").selectOption("18");
  await page
    .getByRole("button", { name: "音色を読み込む", exact: true })
    .click();
  await page.locator("#steps").scrollIntoViewIfNeeded();
  const steps = await page.locator("#steps").boundingBox();
  await page.mouse.move(steps.x + steps.width * 0.15, steps.y + 25);
  await page.mouse.down();
  await page.mouse.move(steps.x + steps.width * 0.45, steps.y + 90, {
    steps: 5,
  });
  await page.mouse.up();
  const downloadPromise = page.waitForEvent("download");
  await page.locator("#export").click();
  const download = await downloadPromise;
  assert(download.suggestedFilename().endsWith(".json"));
  const fs = require("node:fs");
  const path = await download.path();
  const data = JSON.parse(fs.readFileSync(path));
  assert(data.motion.motion.some((v) => v !== 0));
  if (process.env.EDITOR_BRIDGE) {
    await page.locator("#connect").click();
    await page.locator("#connection-state.connected").waitFor();
    await page.locator("#read").click();
    await page
      .getByRole("status")
      .filter({ hasText: "本体の音色・ステップを読み出しました" })
      .waitFor();
    await page.locator("#profile").selectOption("2");
    await page.locator("#apply-profile").click();
    await page.locator("#confirm-profile").click();
    await page
      .getByRole("status")
      .filter({ hasText: "全パートをプロファイルで置き換えました" })
      .waitFor();
    await page.locator("#name").fill("WEB TEST");
    await page.locator("#send").click();
    await page
      .getByRole("status")
      .filter({ hasText: "本体RAMに反映しました" })
      .waitFor();
    await page.locator("#read").click();
    await page
      .getByRole("status")
      .filter({ hasText: "本体の音色・ステップを読み出しました" })
      .waitFor();
    assert.equal(await page.locator("#name").inputValue(), "WEB TEST");
    await page.locator("#read-clock").click();
    await page
      .getByRole("status")
      .filter({ hasText: "共通SNクロックを読み出しました" })
      .waitFor();
    await page.locator("#send-clock").click();
    await page
      .getByRole("status")
      .filter({ hasText: "共通クロックを反映しました" })
      .waitFor();
    await page.locator("#read-cv").click();
    await page
      .getByRole("status")
      .filter({ hasText: "CV / Gateの割り当てを読み出しました" })
      .waitFor();
    await page.locator("#send-cv").click();
    await page
      .getByRole("status")
      .filter({ hasText: "CV / Gateを本体へ反映しました" })
      .waitFor();
    await page.locator("#send-motion").click();
    await page
      .getByRole("status")
      .filter({ hasText: "ステップとトランスポートを本体RAMへ反映しました" })
      .waitFor();
    await page.locator("#save-open").click();
    await page.locator("#save").click();
    await page
      .getByRole("status")
      .filter({ hasText: "保存に失敗しました" })
      .waitFor();
    await page.locator("#save-cancel").click();
  }
  await page.evaluate(() => scrollTo(0, 0));
  await page.screenshot({
    path: process.env.SCREENSHOT_DIR
      ? `${process.env.SCREENSHOT_DIR}/editor-desktop.png`
      : "/tmp/chiptune-editor-desktop.png",
    fullPage: true,
  });
  await page.setViewportSize({ width: 390, height: 844 });
  await page.evaluate(() => scrollTo(0, 0));
  await page.screenshot({
    path: process.env.SCREENSHOT_DIR
      ? `${process.env.SCREENSHOT_DIR}/editor-mobile.png`
      : "/tmp/chiptune-editor-mobile.png",
    fullPage: true,
  });
  assert(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= window.innerWidth + 1,
    ),
  );
  assert.deepEqual(errors, []);
  await browser.close();
  console.log(
    "PASS: firmware-backed Web MIDI handshake, read/write, clock, CV atomic update, save rejection; offline structure selection, ENV shape, SN stack, motion painting, JSON export, mobile overflow, console",
  );
})().catch((e) => {
  console.error(e);
  process.exit(1);
});
