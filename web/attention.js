"use strict";
const $ = (id) => document.getElementById(id);
const state = {
  token: "",
  window: Number(localStorage.getItem("noese.window") ?? 2),
  run: null,
  rendered: null,
  polling: false,
};
function el(tag, text, cls) {
  const e = document.createElement(tag);
  if (text !== undefined) e.textContent = text;
  if (cls) e.className = cls;
  return e;
}
function message(text) {
  $("message").textContent = text;
}
function button(text, fn, cls = "text-button") {
  const b = el("button", text, cls);
  b.type = "button";
  b.onclick = () =>
    Promise.resolve()
      .then(fn)
      .catch((e) => message(e.message));
  return b;
}
async function api(path, body) {
  const response = await fetch(
    "/api/v1/" +
      path +
      (path.includes("?") ? "&" : "?") +
      "window=" +
      state.window,
    {
      method: body === undefined ? "GET" : "POST",
      headers: {
        "X-NOESE-Token": state.token,
        ...(body instanceof FormData
          ? {}
          : { "Content-Type": "application/json" }),
      },
      body:
        body === undefined
          ? undefined
          : body instanceof FormData
            ? body
            : JSON.stringify(body),
    },
  );
  const value = await response.json();
  if (!response.ok)
    throw new Error(value.message || "Falha na comunicação local");
  return value;
}
function dialog(title, build) {
  $("dialog-title").textContent = title;
  $("dialog-body").replaceChildren();
  build($("dialog-body"));
  if (!$("dialog").open) $("dialog").showModal();
}
$("close-dialog").onclick = () => $("dialog").close();
async function source(evidence) {
  const artifact = await api("artifacts/" + evidence.artifact_id);
  const response = await fetch(
    "/api/v1/artifacts/" + artifact.id + "/content?window=" + state.window,
  );
  if (!response.ok) throw new Error("Original indisponível neste recorte");
  const text = await response.text();
  const encoded = new TextEncoder().encode(text),
    decoder = new TextDecoder();
  dialog(artifact.filename, (body) => {
    body.append(
      el(
        "p",
        "Autoria atribuída: " +
          (artifact.attributed_author || "desconhecida") +
          " · " +
          artifact.coverage +
          " · data da fala: " +
          (artifact.occurred_at || "desconhecida"),
        "dialog-note",
      ),
    );
    const pre = el("pre", undefined, "dialog-text");
    pre.append(
      document.createTextNode(decoder.decode(encoded.slice(0, evidence.start))),
      el("mark", decoder.decode(encoded.slice(evidence.start, evidence.end))),
      document.createTextNode(decoder.decode(encoded.slice(evidence.end))),
    );
    body.append(pre);
    const details = el("details");
    details.append(
      el("summary", "Proveniência e integridade"),
      el(
        "p",
        "SHA-256 da fonte: " +
          artifact.sha256 +
          "\nIntervalo em bytes UTF-8: " +
          evidence.start +
          "–" +
          evidence.end,
        "source-details",
      ),
    );
    body.append(details);
    requestAnimationFrame(() =>
      pre.querySelector("mark").scrollIntoView({ block: "center" }),
    );
  });
}
function critique(run, finding) {
  dialog("Colocar esta interpretação em questão", (body) => {
    body.append(
      el("p", finding.title, "dialog-note"),
      el(
        "p",
        "Sua crítica será preservada e considerada no próximo exame. Não é necessário revisar os outros pontos.",
        "dialog-note",
      ),
    );
    const label = el(
        "label",
        "Que leitura, limite ou evidência precisa ser reconsiderada?",
      ),
      text = el("textarea");
    label.append(text);
    body.append(label);
    const actions = el("div", undefined, "actions");
    actions.append(
      button("Cancelar", () => $("dialog").close()),
      button(
        "Preservar crítica e reexaminar",
        async () => {
          if (!text.value.trim())
            throw new Error("Escreva o ponto a reconsiderar.");
          await api("attention/" + run.id + "/critique", {
            text: finding.title + "\n" + text.value,
          });
          $("dialog").close();
          message(
            "Crítica preservada. O NOESE vai reconsiderar o recorte automaticamente.",
          );
          await poll();
        },
        "button",
      ),
    );
    body.append(actions);
  });
}
function render(run) {
  $("report").replaceChildren();
  $("limits").replaceChildren();
  if (!run) return;
  state.run = run;
  const output = run.output;
  const caption = el(
    "p",
    "Leitura preservada em " +
      new Date(run.created_at).toLocaleString("pt-BR") +
      " · " +
      output.coverage.used_sources +
      "/" +
      output.coverage.total_sources +
      " fontes no contexto · " +
      run.model,
    "run-caption",
  );
  $("report").append(caption);
  if (!output.findings.length) {
    const empty = el("section", undefined, "empty-report");
    empty.append(
      el("h2", "Nada suficientemente sustentado por enquanto."),
      el(
        "p",
        "O exame não produziu uma interpretação que pudesse ser apresentada com os apoios exigidos. Esse resultado fica preservado; novas fontes ou uma crítica podem mudar a próxima leitura.",
      ),
    );
    $("report").append(empty);
  }
  for (const [index, f] of output.findings.entries()) {
    const article = el("article", undefined, "finding");
    const heading = el("div", undefined, "finding-heading");
    heading.append(
      el("span", String(index + 1).padStart(2, "0"), "finding-index"),
      el("span", "INTERPRETAÇÃO CANDIDATA", "candidate"),
    );
    article.append(
      heading,
      el("h2", f.title),
      el("p", f.interpretation),
      el("p", f.why_attention, "why"),
    );
    const evidence = el("div", undefined, "evidence");
    for (const e of f.evidence) {
      const quote = el("blockquote", e.quote);
      quote.append(
        el("cite", e.filename),
        button("Ler no contexto ↗", () => source(e)),
      );
      evidence.append(quote);
    }
    article.append(evidence);
    const alternatives = el("details");
    alternatives.append(el("summary", "Que outra leitura é possível?"));
    const list = el("ul");
    for (const a of f.alternatives) list.append(el("li", a));
    alternatives.append(list);
    if (f.automated_counterpoint) {
      alternatives.append(
        el("p", f.automated_counterpoint.alternative),
        el(
          "p",
          "Contraponto automático: " + f.automated_counterpoint.reason,
          "counterpoint",
        ),
        el(
          "p",
          "O mesmo modelo fez os dois passes. Este confronto não é validação independente.",
          "counterpoint",
        ),
      );
    }
    article.append(alternatives);
    for (const c of f.contributions) {
      const box = el("section", undefined, "contribution");
      box.append(
        el("small", "CONTRIBUIÇÃO POSSÍVEL · NÃO EXECUTADA"),
        el("h3", c.proposal),
        el("p", c.reason),
        el("p", "Alternativa: " + c.alternative),
        el("p", "Condição de mobilização: " + c.authorization_needed),
        el(
          "p",
          c.availability === "CAPABILITY_UNAVAILABLE"
            ? "A capacidade necessária não está disponível neste ambiente."
            : "Capacidade proposta: " +
                c.capability_id +
                " · exige decisão sobre o escopo.",
        ),
      );
      article.append(box);
    }
    const actions = el("div", undefined, "finding-actions");
    actions.append(
      button("Contestar ou acrescentar um contraponto", () => critique(run, f)),
    );
    const inspect = el("a", "Examinar percurso preservado ↗", "text-button");
    inspect.href = "/mesa?inquiry=" + run.inquiry_id;
    actions.append(inspect);
    article.append(actions);
    $("report").append(article);
  }
  const limits = el("details", undefined, "limits");
  limits.append(el("summary", "Alcance deste exame e limites"));
  const list = el("ul");
  for (const text of output.limits) list.append(el("li", text));
  if (output.rejected?.length)
    list.append(
      el(
        "li",
        output.rejected.length +
          " propostas ou revisões foram descartadas por falta de apoio ou contraponto.",
      ),
    );
  if (output.coverage.excluded?.length)
    list.append(
      el(
        "li",
        output.coverage.excluded.length +
          " fontes não entraram no contexto, por formato ou limite de tamanho.",
      ),
    );
  limits.append(list);
  $("limits").append(limits);
}
async function poll() {
  if (state.polling) return;
  state.polling = true;
  const requestedWindow = state.window;
  try {
    const data = await api("attention");
    if (state.window !== requestedWindow) return;
    const phase = data.state.phase;
    const descriptions = {
      idle: ["Observando o acervo", "O próximo exame começa automaticamente."],
      waiting: [
        "Preparando o primeiro exame",
        "As fontes disponíveis serão examinadas sem precisar abrir um percurso.",
      ],
      waiting_for_window: [
        "Preparando este recorte",
        "A leitura desta janela aguarda processamento.",
      ],
      investigating: [
        "Investigando o acervo",
        "Examinando relações de sentido nas fontes. O primeiro exame local pode levar alguns minutos.",
      ],
      confronting: [
        "Confrontando interpretações",
        "Procurando alternativas, limites e alegações sem apoio.",
      ],
      preserving: [
        "Preservando a investigação",
        "Ligando as interpretações aos trechos exatos.",
      ],
      current: [
        "Leitura atual preservada",
        "O NOESE retomará a investigação quando o acervo ou uma crítica mudar.",
      ],
      disabled: [
        "Análise semântica local desativada",
        "Use Processamento local para habilitar um modelo instalado. O acervo permanece acessível.",
      ],
      empty: [
        "Nenhum texto disponível neste recorte",
        "Incorpore fontes autorizadas para começar. Imagens e formatos sem extração permanecem preservados.",
      ],
      unavailable: [
        "Não foi possível concluir o exame",
        data.state.message ||
          "O recurso local está indisponível. Nenhuma conclusão foi fabricada.",
      ],
    };
    const [title, description] = descriptions[phase] || descriptions.waiting;
    $("activity-title").textContent = title;
    $("activity-detail").textContent = description;
    $("activity-model").textContent = data.model + " · local";
    $("activity-dot").classList.toggle(
      "busy",
      ["investigating", "confronting", "preserving"].includes(phase),
    );
    $("archive-label").textContent =
      data.source_count === undefined
        ? "ACERVO LOCAL EM INVESTIGAÇÃO"
        : data.source_count + " FONTES AUTORIZADAS · J" + state.window;
    const key = data.run?.id || null;
    if (key !== state.rendered) {
      state.rendered = key;
      render(data.run);
    }
    if (!data.run && phase === "current")
      $("activity-detail").textContent =
        "Nenhuma investigação preservada neste recorte.";
  } catch (e) {
    message(e.message);
  } finally {
    state.polling = false;
  }
}
async function settings() {
  const data = await api("attention/settings");
  dialog("Processamento semântico local", (body) => {
    body.append(
      el(
        "p",
        "O modelo trabalha apenas com as fontes do acervo e não recebe ferramentas para executar ações externas. Nenhum modelo é baixado automaticamente.",
        "dialog-note",
      ),
    );
    const label = el("label", "Modelo instalado"),
      select = el("select");
    for (const model of data.installed_models) {
      const option = el("option", model);
      option.value = model;
      select.append(option);
    }
    select.value = data.config.model;
    label.append(select);
    body.append(label);
    if (!data.installed_models.length)
      body.append(
        el(
          "p",
          "Nenhum modelo local disponível no provedor configurado.",
          "dialog-note",
        ),
      );
    const actions = el("div", undefined, "actions");
    if (data.config.enabled)
      actions.append(
        button(
          "Pausar análise",
          async () => {
            await api("attention/settings", { enabled: false });
            $("dialog").close();
            await poll();
          },
          "button",
        ),
      );
    if (data.installed_models.length)
      actions.append(
        button(
          data.config.enabled
            ? "Aplicar modelo"
            : "Habilitar processamento local",
          async () => {
            await api("attention/settings", {
              enabled: true,
              model: select.value,
              window: state.window,
            });
            $("dialog").close();
            message(
              "Processamento local habilitado. O exame começa automaticamente.",
            );
            await poll();
          },
          "button",
        ),
      );
    body.append(actions);
  });
}
$("settings-button").onclick = () =>
  settings().catch((e) => message(e.message));
$("window").value = String(state.window);
$("window").onchange = async () => {
  state.window = Number($("window").value);
  localStorage.setItem("noese.window", String(state.window));
  state.rendered = undefined;
  render(null);
  try {
    await api("attention/settings", { window: state.window });
    await poll();
  } catch (e) {
    message(e.message);
  }
};
$("upload").onchange = async () => {
  try {
    for (const file of $("upload").files) {
      const body = new FormData();
      body.append("file", file);
      await api("artifacts", body);
    }
    message("Fontes incorporadas. O próximo exame considerará o novo acervo.");
    await poll();
  } catch (e) {
    message(e.message);
  }
};
(async () => {
  try {
    state.token = (await api("session")).token;
    await poll();
    setInterval(poll, 4000);
  } catch (e) {
    message(e.message);
  }
})();
