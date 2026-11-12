// Browser contracts with a deterministic provider in a separate temporary archive.
// This fixture is not a scientific evaluation or a substitute for a real model run.
const { chromium } = require("playwright");
const fs = require("node:fs"),
  os = require("node:os"),
  path = require("node:path"),
  http = require("node:http"),
  net = require("node:net");
const { spawn, spawnSync } = require("node:child_process");
const assert = require("node:assert/strict");
const { randomUUID } = require("node:crypto");
const root = path.resolve(__dirname, ".."),
  temp = fs.mkdtempSync(path.join(os.tmpdir(), "noese-attention-ui-"));
const evidence = path.join(root, "build/attention-browser");
fs.mkdirSync(evidence, { recursive: true });
let browser,
  process,
  calls = 0;
const errors = [];
const provider = http.createServer(async (req, res) => {
  if (req.method === "GET") {
    res.setHeader("Content-Type", "application/json");
    res.end(
      JSON.stringify({
        models: [{ name: "browser-contract", digest: "fixture-only" }],
      }),
    );
    return;
  }
  let body = "";
  for await (const data of req) body += data;
  const request = JSON.parse(body),
    input = JSON.parse(request.messages[1].content);
  calls++;
  const result = input.candidate_findings
    ? {
        reviews: input.candidate_findings.map((_, index) => ({
          index,
          keep: true,
          reason: "Contract fixture, not scientific evidence.",
          alternative: "Another reading remains possible.",
        })),
        limits: [],
      }
    : {
        findings: [
          {
            title: input.human_criticism.length
              ? "Leitura reconsiderada após crítica"
              : 'Uma tensão candidata <img src=x onerror="window.sourceExecuted=true">',
            interpretation:
              "Interpretação sintética exclusiva do teste de interface.",
            why_attention:
              "Duas fontes foram confrontadas neste teste técnico.",
            alternatives: ["As fontes podem descrever contextos distintos."],
            evidence: input.sources
              .slice(0, 2)
              .map((s) => ({ source: s.source, quote: s.text })),
            contributions: [
              {
                proposal: "Examinar o contexto com outra perspectiva.",
                reason: "Há limites de contexto.",
                alternative: "Manter a questão em aberto.",
                capability_id: "unavailable",
                authorization_needed:
                  "Uma consulta externa real depende de autorização e fonte disponível.",
              },
            ],
          },
        ],
        limits: ["Provider fixture; no scientific claim."],
      };
  res.setHeader("Content-Type", "application/json");
  res.end(
    JSON.stringify({
      message: { content: JSON.stringify(result) },
      done: true,
      done_reason: "stop",
      model: "browser-contract",
    }),
  );
});
async function port() {
  const s = net.createServer();
  await new Promise((r) => s.listen(0, "127.0.0.1", r));
  const p = s.address().port;
  await new Promise((r) => s.close(r));
  return p;
}
async function start(p) {
  process = spawn(
    path.join(root, "build/noese"),
    ["--data", temp, "--port", String(p)],
    { stdio: "ignore" },
  );
  for (let i = 0; i < 100; i++) {
    try {
      if ((await fetch(`http://127.0.0.1:${p}/health`)).ok) return;
    } catch {}
    await new Promise((r) => setTimeout(r, 40));
  }
  throw new Error("HTTP not ready");
}
async function stop() {
  if (process && process.exitCode === null && process.signalCode === null) {
    process.kill();
    await new Promise((r) => process.once("exit", r));
  }
}
(async () => {
  await new Promise((r) => provider.listen(0, "127.0.0.1", r));
  const messages = [
    {
      jsonrpc: "2.0",
      id: 1,
      method: "initialize",
      params: {
        protocolVersion: "2025-11-25",
        capabilities: {},
        clientInfo: { name: "ui-contract-setup", version: "1" },
      },
    },
    { jsonrpc: "2.0", method: "notifications/initialized" },
    ...[
      "Primeira nota técnica sem interrogação: o orçamento inicial exclui os custos recorrentes.",
      "Segunda nota técnica sem interrogação: os custos recorrentes reduzem os recursos disponíveis.",
    ].map((text, i) => ({
      jsonrpc: "2.0",
      id: i + 2,
      method: "tools/call",
      params: {
        name: "import_text",
        arguments: {
          filename: randomUUID() + ".txt",
          text,
          request_id: randomUUID(),
        },
      },
    })),
  ];
  const seeded = spawnSync(
    path.join(root, "build/noese"),
    ["mcp", "--data", temp, "--allow-writes"],
    {
      input: messages.map((m) => JSON.stringify(m)).join("\n") + "\n",
      encoding: "utf8",
    },
  );
  assert.equal(seeded.status, 0, seeded.stderr);
  fs.writeFileSync(
    path.join(temp, "investigator.json"),
    JSON.stringify({
      enabled: true,
      model: "browser-contract",
      port: provider.address().port,
      window: 2,
    }),
  );
  const p = await port(),
    url = `http://127.0.0.1:${p}`;
  await start(p);
  browser = await chromium.launch({
    headless: true,
    executablePath:
      global.process.env.NOESE_BROWSER || "/usr/bin/chromium-browser",
    args: ["--no-sandbox"],
  });
  const page = await browser.newPage({
    viewport: { width: 1440, height: 1000 },
  });
  page.on("pageerror", (e) => errors.push(e.message));
  await page.goto(url);
  await page.locator("#report article.finding").waitFor();
  assert.equal(await page.locator("#new-inquiry").count(), 0);
  assert.equal(await page.locator("#report img").count(), 0);
  assert.equal(await page.evaluate(() => window.sourceExecuted), undefined);
  assert.equal(calls, 2);
  const before = await (
    await page.request.get(url + "/api/v1/attention")
  ).json();
  await page
    .locator(".evidence")
    .getByRole("button", { name: "Ler no contexto ↗" })
    .first()
    .click();
  await page.locator("#dialog mark").waitFor();
  assert.ok(
    (await page.locator("#dialog mark").textContent()).includes(
      "Primeira nota",
    ),
  );
  await page.locator("#close-dialog").click();
  await page.reload();
  await page.locator("#report article.finding").waitFor();
  assert.equal(calls, 2);
  await page.locator("#report summary").click();
  await page
    .getByText("O mesmo modelo fez os dois passes.", { exact: false })
    .waitFor();
  await page
    .getByRole("button", { name: "Contestar ou acrescentar um contraponto" })
    .click();
  await page
    .getByLabel("Que leitura, limite ou evidência precisa ser reconsiderada?")
    .fill("Considere a possibilidade de serem contextos distintos.");
  await page
    .getByRole("button", { name: "Preservar crítica e reexaminar" })
    .click();
  await page
    .getByRole("heading", { name: "Leitura reconsiderada após crítica" })
    .waitFor();
  const after = await (
    await page.request.get(url + "/api/v1/attention")
  ).json();
  assert.notEqual(before.run.id, after.run.id);
  assert.equal(after.run.output.previous_run_id, before.run.id);
  await page.locator("#window").selectOption("0");
  await page.waitForFunction(
    () => !document.querySelector("#report article.finding"),
  );
  assert.equal(await page.locator("#report article").count(), 0);
  await page.locator("#window").selectOption("2");
  await page
    .getByRole("heading", { name: "Leitura reconsiderada após crítica" })
    .waitFor();
  await stop();
  await start(p);
  await page.reload();
  await page
    .getByRole("heading", { name: "Leitura reconsiderada após crítica" })
    .waitFor();
  assert.equal(calls, 4);
  await page.screenshot({
    path: path.join(evidence, "desktop.png"),
    fullPage: true,
  });
  await page.setViewportSize({ width: 390, height: 844 });
  await require("./identity.cjs")(page);
  assert.ok(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= innerWidth,
    ),
  );
  await page.screenshot({
    path: path.join(evidence, "mobile.png"),
    fullPage: true,
  });
  assert.deepEqual(errors, []);
  const result = {
    checks: [
      "investigation appears without question or manual inquiry",
      "two literal source contexts resolve",
      "untrusted model HTML is escaped",
      "continuity after page/server restart without repeat inference",
      "human criticism triggers a new cycle and preserves previous run",
      "J0 does not display J2 output",
      "responsive 1440/390px",
      "shared identity assets, seven viewport widths and accessible mobile navigation",
      "no JavaScript errors",
    ],
    provider: "DETERMINISTIC_CONTRACT_FIXTURE_NOT_SCIENTIFIC_EVALUATION",
    calls,
    pageErrors: errors,
  };
  fs.writeFileSync(
    path.join(evidence, "results.json"),
    JSON.stringify(result, null, 2),
  );
  console.log(JSON.stringify(result, null, 2));
})()
  .catch((e) => {
    console.error(e);
    global.process.exitCode = 1;
  })
  .finally(async () => {
    if (browser) await browser.close();
    await stop();
    await new Promise((r) => provider.close(r));
    fs.rmSync(temp, { recursive: true, force: true });
  });
