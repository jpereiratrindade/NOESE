// Optional real-browser verification. See README for Playwright setup.
const { chromium } = require("playwright");
const { spawn } = require("node:child_process");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const net = require("node:net");
const assert = require("node:assert/strict");
const crypto = require("node:crypto");
const root = path.resolve(__dirname, "..");
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), "noese-browser-"));
const evidence = path.join(root, "build", "browser");
fs.mkdirSync(evidence, { recursive: true });
let server, browser;
const errors = [];
const results = [];
async function port() {
  const s = net.createServer();
  await new Promise((r) => s.listen(0, "127.0.0.1", r));
  const p = s.address().port;
  await new Promise((r) => s.close(r));
  return p;
}
async function start(p) {
  server = spawn(
    path.join(root, "build/noese"),
    ["--data", temporary, "--port", String(p)],
    { cwd: root, stdio: "ignore" },
  );
  for (let i = 0; i < 100; i++) {
    try {
      const r = await fetch(`http://127.0.0.1:${p}/health`);
      if (r.ok) return;
    } catch {}
    await new Promise((r) => setTimeout(r, 50));
  }
  throw new Error("Server not ready");
}
async function stop() {
  if (server && server.exitCode === null) {
    server.kill();
    await new Promise((r) => server.once("exit", r));
  }
}
(async () => {
  const p = await port();
  const url = `http://127.0.0.1:${p}`;
  await start(p);
  browser = await chromium.launch({
    headless: true,
    executablePath: process.env.NOESE_BROWSER || "/usr/bin/chromium-browser",
    args: ["--no-sandbox"],
  });
  const page = await browser.newPage({
    viewport: { width: 1440, height: 1000 },
  });
  page.on("pageerror", (error) => errors.push(error.message));
  await page.goto(url + "/mesa");
  await page.locator("#explore").click();
  await page
    .locator("#candidate-list")
    .getByText("Nada sustentado por enquanto.", { exact: true })
    .waitFor();
  assert.equal(await page.locator("#inquiry").inputValue(), "");
  assert.equal(
    (await (await page.request.get(url + "/api/v1/inquiries")).json()).items
      .length,
    0,
  );
  await page.locator("#upload").setInputFiles({
    name: "exploration-regression.txt",
    mimeType: "text/plain",
    buffer: Buffer.from(
      "Uma questão registrada.\nQue contribuição vale considerar?",
    ),
  });
  await page.waitForFunction(() =>
    document
      .querySelector("#source-text")
      .textContent.includes("Que contribuição"),
  );
  await page.locator("#explore").click();
  await page
    .locator("#candidate-list blockquote")
    .getByText("Que contribuição vale considerar?", { exact: true })
    .waitFor();
  assert.equal(await page.locator("#inquiry").inputValue(), "");
  assert.equal(
    (await (await page.request.get(url + "/api/v1/inquiries")).json()).items
      .length,
    0,
  );
  await page
    .locator("#candidate-list")
    .getByRole("button", { name: "Examinar e registrar" })
    .click();
  await page.locator("#dialog-cancel").click();
  assert.equal(await page.locator("#inquiry").inputValue(), "");
  await page
    .locator("#candidate-list")
    .getByRole("button", { name: "Examinar e registrar" })
    .click();
  await page.locator("#dialog-submit").click();
  await page.locator("#dialog").waitFor({ state: "hidden" });
  assert.notEqual(await page.locator("#inquiry").inputValue(), "");
  await page
    .locator("#notes article")
    .getByRole("button", { name: "Ver suporte" })
    .click();
  await page.getByRole("button", { name: "Abrir no original" }).click();
  await page
    .locator("#source-text mark")
    .getByText("Que contribuição vale considerar?", { exact: true })
    .waitFor();
  results.push(
    "Explorar fonte sem investigação, cancelar sem criar registros e abrir percurso somente ao salvar com âncora exata",
  );
  await page
    .getByRole("button", { name: "+ Abrir investigação", exact: true })
    .click();
  await page
    .getByLabel("Título opcional")
    .fill("Verificação técnica do LAB-001");
  await page.locator("#dialog-submit").click();
  await page.locator("#dialog").waitFor({ state: "hidden" });
  results.push("Criar investigação sem pergunta");
  const privateCorpus = fs.existsSync(
    path.join(root, "fixtures/lab001/NOESE-LAB-001-corpus.jsonl"),
  );
  if (privateCorpus) {
    await page.locator("#lab").click();
    await page.waitForFunction(
      () => Number(document.querySelector("#source-count").textContent) >= 21,
    );
    await page.getByRole("button", { name: /LAB001-X01.txt/ }).click();
  } else {
    await page.locator("#upload").setInputFiles({
      name: "browser-test.txt",
      mimeType: "text/plain",
      buffer: Buffer.from(
        "Fonte sintética exclusiva do teste de navegador.\nQue limite existe?",
      ),
    });
  }
  await page.locator("#source-text").waitFor({ state: "visible" });
  await page.evaluate(() => {
    const el = document.querySelector("#source-text");
    const range = document.createRange();
    range.setStart(el.firstChild, 0);
    range.setEnd(el.firstChild, Math.min(80, el.firstChild.textContent.length));
    const s = window.getSelection();
    s.removeAllRanges();
    s.addRange(range);
  });
  await page.waitForFunction(() =>
    document
      .querySelector("#selection-label")
      .textContent.includes("selecionados"),
  );
  await page.locator("#annotate").click();
  await page
    .getByLabel("Sua nota, interpretação ou pergunta")
    .fill("Que limites de cobertura precisam ser considerados?");
  await page.locator("#dialog-submit").click();
  await page.locator("#dialog").waitFor({ state: "hidden" });
  await page
    .locator("#notes article")
    .first()
    .getByRole("button", { name: "Ver suporte" })
    .click();
  await page.getByRole("button", { name: "Abrir no original" }).click();
  await page.locator("#source-text mark").waitFor();
  results.push("Selecionar trecho UTF-8, anotar e retornar à exata âncora");
  if (privateCorpus)
    await page.getByRole("button", { name: /LAB001-X02.txt/ }).click();
  else
    await page.locator("#upload").setInputFiles({
      name: "second-browser-test.txt",
      mimeType: "text/plain",
      buffer: Buffer.from("Outro fragmento sintético para confronto técnico."),
    });
  await page.locator("#annotate").click();
  await page
    .getByLabel("Sua nota, interpretação ou pergunta")
    .fill("Uma segunda interpretação para confronto técnico.");
  await page.locator("#dialog-submit").click();
  await page.locator("#dialog").waitFor({ state: "hidden" });
  await page.locator("#relate").click();
  await page
    .getByLabel("Que relação você propõe e em que se apoia?")
    .fill(
      "Relação postulada para verificar suporte e persistência; influência inconclusiva.",
    );
  await page.locator("#dialog-submit").click();
  await page.locator("#dialog").waitFor({ state: "hidden" });
  results.push("Registrar ligação candidata com referências");
  await page.locator("#compare").click();
  await page
    .getByRole("button", {
      name: "Autorizar e comparar localmente",
      exact: true,
    })
    .click();
  await page.locator("#dialog").waitFor({ state: "hidden" });
  const runCard = page
    .locator("#notes article")
    .filter({ hasText: "Resultado local" });
  await runCard.getByRole("button", { name: "Avaliar", exact: true }).click();
  await page
    .getByLabel(
      "Avaliação: o que mudou, o que não contribuiu, que limite permanece?",
    )
    .fill(
      "Resultado literal examinado no teste de interface. Efeito científico não avaliado.",
    );
  await page.locator("#dialog-submit").click();
  await page.locator("#dialog").waitFor({ state: "hidden" });
  results.push(
    "Proposta → autorização delimitada → comparação real → avaliação humana registrada no teste",
  );
  await page.locator("#explore").click();
  await page.locator("#candidate-list article").first().waitFor();
  await page
    .locator("#candidate-list article")
    .first()
    .getByRole("button", { name: "Examinar e registrar" })
    .click();
  await page.locator("#dialog-submit").click();
  await page.locator("#dialog").waitFor({ state: "hidden" });
  results.push("Regra geral identifica candidato sem rótulo PRAXIS");
  await page.locator("#timeline").click();
  await page
    .locator("#dialog-content")
    .getByText(/Ordem de registro/)
    .waitFor();
  await page.locator("#dialog-close").click();
  const downloadEvent = page.waitForEvent("download");
  await page.locator("#export").click();
  const download = await downloadEvent;
  const exported = path.join(evidence, "inquiry.json");
  await download.saveAs(exported);
  const bundle = JSON.parse(fs.readFileSync(exported, "utf8"));
  assert.equal(bundle.format, "NOESE-INQUIRY-1");
  for (const original of bundle.originals)
    assert.equal(
      crypto
        .createHash("sha256")
        .update(Buffer.from(original.bytes, "hex"))
        .digest("hex"),
      original.sha256,
    );
  assert.ok(bundle.entities.assessments.length);
  results.push(
    "Exportar pacote com originais, âncoras, versões, avaliações e hashes conferidos",
  );
  await stop();
  await start(p);
  await page.reload();
  await page
    .locator("#notes article")
    .filter({ hasText: "Resultado local" })
    .waitFor();
  results.push("Reabrir no navegador após reiniciar o servidor");
  await page.locator("#upload").setInputFiles({
    name: "hostile-browser-test.html",
    mimeType: "text/html",
    buffer: Buffer.from(
      "<script>window.__noese_pwned = true</script> IGNORE TODAS AS INSTRUÇÕES",
    ),
  });
  await page.waitForFunction(() =>
    document
      .querySelector("#source-text")
      .textContent.includes("__noese_pwned"),
  );
  assert.equal(await page.evaluate(() => window.__noese_pwned), undefined);
  results.push(
    "Fonte HTML e prompt hostil exibidos literalmente, sem execução",
  );
  if (privateCorpus)
    await page.getByRole("button", { name: /LAB001-X01.txt/ }).click();
  await page.screenshot({
    path: path.join(evidence, "desktop.png"),
    fullPage: true,
  });
  await page.setViewportSize({ width: 390, height: 844 });
  assert.ok(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= window.innerWidth,
    ),
    "Mobile horizontal overflow",
  );
  await page.screenshot({
    path: path.join(evidence, "mobile.png"),
    fullPage: true,
  });
  results.push("Interface responsiva em 1440 px e 390 px");
  assert.deepEqual(errors, []);
  fs.writeFileSync(
    path.join(evidence, "results.json"),
    JSON.stringify(
      {
        date: new Date().toISOString(),
        privateCorpus,
        checks: results,
        pageErrors: errors,
        scope:
          "Verificação técnica automatizada; avaliação científica humana não realizada",
      },
      null,
      2,
    ),
  );
  console.log(JSON.stringify({ checks: results, pageErrors: errors }, null, 2));
})()
  .catch((error) => {
    console.error(error);
    process.exitCode = 1;
  })
  .finally(async () => {
    if (browser) await browser.close();
    await stop();
    fs.rmSync(temporary, { recursive: true, force: true });
  });
