// Real browser, isolated archive; synthetic records are never inserted in use data.
const { chromium } = require("playwright");
const { spawn } = require("node:child_process");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const net = require("node:net");
const { randomUUID } = require("node:crypto");
const root = path.resolve(__dirname, "..");
const temp = fs.mkdtempSync(
  path.join(os.tmpdir(), "noese-experience-browser-"),
);
let server, browser;
const errors = [];
(async () => {
  const socket = net.createServer();
  await new Promise((r) => socket.listen(0, "127.0.0.1", r));
  const port = socket.address().port;
  await new Promise((r) => socket.close(r));
  const url = `http://127.0.0.1:${port}`;
  server = spawn(
    path.join(root, "build/noese"),
    ["--data", temp, "--port", String(port)],
    { stdio: "ignore" },
  );
  for (let i = 0; i < 100; i++) {
    try {
      if ((await fetch(url + "/health")).ok) break;
    } catch {}
    await new Promise((r) => setTimeout(r, 40));
  }
  browser = await chromium.launch({
    headless: true,
    executablePath: process.env.NOESE_BROWSER || "/usr/bin/chromium-browser",
    args: ["--no-sandbox"],
  });
  const page = await browser.newPage({
    viewport: { width: 1440, height: 1000 },
  });
  page.on("pageerror", (e) => errors.push(e.message));
  await page.goto(url + "/experiencias");
  await page
    .getByText("Nenhuma experiência preservada neste recorte.")
    .waitFor();
  const report =
    "# Relato secundário\n<script>window.executedSource=true</script>\nSem respostas originais; momento da escolha desconhecido.";
  await page
    .getByRole("button", { name: "Preservar experiência", exact: true })
    .click();
  await page.locator("#capture-title").fill("Relato de contrato do navegador");
  await page.locator("#capture-file").setInputFiles({
    name: "report.md",
    mimeType: "text/markdown",
    buffer: Buffer.from(report),
  });
  await page
    .locator("#capture-limitations")
    .fill("Fixture secundária. Não demonstra sustentação científica.");
  await page.getByRole("button", { name: "Preservar sem homologar" }).click();
  const detail = page.locator("#experience-detail");
  await detail
    .getByRole("heading", { name: "Relato de contrato do navegador" })
    .waitFor();
  await detail
    .getByText(
      "Avaliação humana pendente. Nenhuma conclusão é homologada automaticamente.",
    )
    .waitFor();
  assert.match(
    await detail.innerText(),
    /A escolha inicial não pode ser reconstituída/,
  );
  await detail
    .getByText("Relato original preservado · respostas brutas ausentes")
    .click();
  assert.equal(await page.evaluate(() => window.executedSource), undefined);
  assert.equal(await detail.locator("script").count(), 0);
  assert.ok((await detail.locator("pre").allTextContents()).includes(report));
  const inquiry = new URL(page.url()).searchParams.get("inquiry");
  const initial = await (
    await page.request.get(url + "/api/v1/experiences/" + inquiry)
  ).json();
  assert.equal(initial.experience.capture, "secondary_report");
  assert.equal(initial.assessments.length, 0);
  await detail
    .getByLabel("Pertinência: a evidência sustenta a afirmação?")
    .selectOption("insufficient");
  await detail
    .getByLabel("Alcance: a conclusão respeita os limites?")
    .selectOption("pending");
  await detail
    .getByLabel("A contribuição merece atenção?")
    .selectOption("pending");
  await detail
    .getByLabel("Justificativa e limites da avaliação")
    .fill("Avaliação sintética de contrato. Sem homologação científica.");
  await detail.getByRole("button", { name: "Registrar avaliação" }).click();
  await page
    .getByText("Avaliação registrada, sem promoção automática da conclusão.")
    .waitFor();
  const assessed = await (
    await page.request.get(url + "/api/v1/experiences/" + inquiry)
  ).json();
  assert.equal(assessed.assessments.length, 1);
  assert.equal(assessed.assessments[0].dimensions.pertinence, "insufficient");
  assert.equal(assessed.assessments[0].dimensions.scope, "pending");
  assert.equal(assessed.experience.status, "SECONDARY_REPORT");
  await page.reload();
  await detail.getByText(/Avaliação sintética de contrato/).waitFor();
  const exported = await (
    await page.request.get(url + "/api/v1/export/inquiries/" + inquiry)
  ).json();
  assert.equal(
    Buffer.from(exported.originals[0].bytes, "hex").toString(),
    report,
  );
  // A later window must not leak the report or review into J0.
  await page.locator("#window").selectOption("0");
  await page
    .getByText("Nenhuma experiência preservada neste recorte.")
    .waitFor();
  assert.doesNotMatch(
    await detail.innerText(),
    /Avaliação sintética de contrato/,
  );
  await page.locator("#window").selectOption("2");
  await page.locator(".experience-choice").first().waitFor();
  await page
    .getByRole("button", { name: "Preservar experiência", exact: true })
    .click();
  const runId = randomUUID();
  const journal =
    [
      {
        id: "e0001",
        at: "2026-11-12T17:00:00Z",
        kind: "mission",
        payload: {
          run_id: runId,
          config: {
            memory_policy: "none",
            experiment_purpose: "operational_validation",
          },
        },
      },
      {
        id: "e0002",
        at: "2026-11-12T17:00:01Z",
        kind: "outcome",
        payload: { status: "OPERATIONAL_FAILURE", initial_focus: null },
      },
    ]
      .map((e) => JSON.stringify(e))
      .join("\n") + "\n";
  await page.locator("#capture-title").fill("Captura instrumental de falha");
  await page.locator("#capture-file").setInputFiles({
    name: "events.jsonl",
    mimeType: "text/plain",
    buffer: Buffer.from(journal),
  });
  await page.waitForFunction(
    () => document.getElementById("capture-kind").value === "trace",
  );
  await page
    .locator("#capture-limitations")
    .fill("Falha simulada antes do foco. Não é abstenção.");
  await page.getByRole("button", { name: "Preservar sem homologar" }).click();
  await detail
    .getByRole("heading", { name: "Captura instrumental de falha" })
    .waitFor();
  await detail
    .locator(".experience-badges")
    .getByText("Falha operacional", { exact: true })
    .waitFor();
  assert.match(await detail.innerText(), /Não houve foco inicial aceito/);
  assert.match(await detail.innerText(), /2 eventos/);
  await require("./identity.cjs")(page);
  fs.mkdirSync(path.join(root, "build/experience-browser"), {
    recursive: true,
  });
  await page.setViewportSize({ width: 1440, height: 1000 });
  await page.screenshot({
    path: path.join(root, "build/experience-browser/desktop.png"),
    fullPage: true,
  });
  await page.setViewportSize({ width: 390, height: 844 });
  await page.screenshot({
    path: path.join(root, "build/experience-browser/mobile.png"),
    fullPage: true,
  });
  assert.deepEqual(errors, []);
  console.log(
    JSON.stringify(
      {
        checks: [
          "secondary report retains missing provenance",
          "untrusted report rendered literally",
          "human review separates pertinence, scope and attention",
          "recovery and original byte export",
          "window isolation",
          "instrumental failure not abstention",
          "seven widths and keyboard navigation",
        ],
        pageErrors: errors,
      },
      null,
      2,
    ),
  );
})()
  .catch((e) => {
    console.error(e);
    process.exitCode = 1;
  })
  .finally(async () => {
    if (browser) await browser.close();
    if (server && server.exitCode === null) {
      server.kill();
      await new Promise((r) => server.once("exit", r));
    }
    fs.rmSync(temp, { recursive: true, force: true });
  });
