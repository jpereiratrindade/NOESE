"use strict";
const $ = (id) => document.getElementById(id);
const state = {
  token: "",
  window: Number(localStorage.getItem("noese.window") ?? 2),
  offset: 0,
  next: null,
  current: null,
  requestId: crypto.randomUUID(),
};
const labels = {
  trace: "Captura instrumental",
  secondary_report: "Relato secundário",
  CANDIDATE: "Contribuição candidata",
  ABSTENTION: "Abstenção registrada",
  OPERATIONAL_FAILURE: "Falha operacional",
  PROTOCOL_REJECTED: "Saída fora do contrato",
  CANDIDATE_REJECTED: "Candidata rejeitada",
  BUDGET_EXHAUSTED: "Orçamento esgotado",
  SECONDARY_REPORT: "Relato sem respostas originais",
  operational_validation: "Verificação de funcionamento",
  open_investigation: "Investigação aberta",
  directed_investigation: "Foco fornecido",
  retrospective: "Registro retrospectivo",
  none: "Memória NOESE excluída",
  archive: "Acervo anterior disponível",
  not_recorded: "Memória: exposição não registrada",
  local_harness: "Harness local (declarado)",
  conversation: "Conversa (declarada)",
  human: "Pesquisador (declarado)",
};
function el(tag, text, cls) {
  const e = document.createElement(tag);
  if (text !== undefined) e.textContent = text;
  if (cls) e.className = cls;
  return e;
}
function message(text) {
  $("experience-status").textContent = text;
}
async function api(path, body) {
  const r = await fetch(
    "/api/v1/" +
      path +
      (path.includes("?") ? "&" : "?") +
      "window=" +
      state.window,
    {
      method: body === undefined ? "GET" : "POST",
      headers: {
        "Content-Type": "application/json",
        "X-NOESE-Token": state.token,
      },
      body: body === undefined ? undefined : JSON.stringify(body),
    },
  );
  const value = await r.json();
  if (!r.ok) throw new Error(value.message || "Falha na comunicação");
  return value;
}
function detail(parent, title, value) {
  const box = el("details");
  box.append(
    el("summary", title),
    el(
      "pre",
      typeof value === "string" ? value : JSON.stringify(value, null, 2),
    ),
  );
  parent.append(box);
}
async function list(reset = true) {
  if (reset) {
    state.offset = 0;
    $("experience-list").replaceChildren();
  }
  const requestedWindow = state.window;
  const data = await api("experiences?limit=100&offset=" + state.offset);
  if (requestedWindow !== state.window) return;
  for (const item of data.items) {
    const b = el("button", undefined, "experience-choice");
    b.type = "button";
    b.dataset.id = item.id;
    b.setAttribute("aria-pressed", String(state.current === item.id));
    b.append(
      el("strong", item.title),
      el(
        "small",
        (labels[item.experience.capture] || item.experience.capture) +
          " · " +
          (labels[item.experience.status] || item.experience.status),
      ),
    );
    b.onclick = () => show(item.id).catch((e) => message(e.message));
    $("experience-list").append(b);
  }
  state.next = data.next_offset;
  $("more-experiences").hidden = state.next === null;
  if (!$("experience-list").children.length)
    $("experience-list").append(
      el("p", "Nenhuma experiência preservada neste recorte."),
    );
}
async function show(id) {
  const windowAtRequest = state.window;
  const request = (state.detailRequest || 0) + 1;
  state.detailRequest = request;
  const data = await api("experiences/" + id);
  if (windowAtRequest !== state.window || request !== state.detailRequest)
    return;
  state.current = id;
  history.replaceState(
    null,
    "",
    "/experiencias?inquiry=" + encodeURIComponent(id),
  );
  for (const button of document.querySelectorAll(".experience-choice"))
    button.setAttribute("aria-pressed", String(button.dataset.id === id));
  const root = $("experience-detail"),
    meta = data.experience;
  root.replaceChildren(el("h2", data.inquiry.title));
  const badges = el("div", undefined, "experience-badges");
  for (const value of [
    meta.capture,
    meta.producer_declared,
    meta.purpose,
    meta.memory_policy,
    meta.status,
  ])
    badges.append(el("span", labels[value] || value));
  root.append(
    badges,
    el(
      "p",
      "Integridade dos bytes: " +
        data.integrity +
        ". Autoria e sustentação científica não são certificadas por essa verificação.",
    ),
  );
  root.append(el("p", meta.limitations, "experience-warning"));
  root.append(el("h3", "Proveniência da atenção"));
  if (meta.initial_focus) {
    root.append(
      el("p", meta.initial_focus.question),
      el("p", meta.initial_focus.reason),
      el(
        "p",
        "Foco registrado antes das consultas investigativas. Referências: " +
          meta.initial_focus.response_ids.join(", "),
      ),
    );
  } else
    root.append(
      el(
        "p",
        meta.capture === "secondary_report"
          ? "A escolha inicial não pode ser reconstituída deste relato. Justificativas no texto são retrospectivas ou de momento desconhecido."
          : "Não houve foco inicial aceito e registrado nesta execução.",
      ),
    );
  detail(root, "Exposição à memória anterior do NOESE", {
    policy: meta.memory_policy,
    queries: meta.memory_queries,
  });
  if (meta.capture === "trace") {
    root.append(el("h3", "Desfecho registrado"));
    root.append(
      el("p", data.trace.outcome.reason || labels[meta.status] || meta.status),
    );
    if (data.trace.draft) {
      root.append(
        el(
          "h3",
          meta.status === "CANDIDATE_REJECTED"
            ? "Rascunho do modelo · candidata rejeitada"
            : "Rascunho do modelo · sem homologação",
        ),
      );
      root.append(el("p", data.trace.draft.output.summary));
      root.append(
        el(
          "p",
          "Evento de origem: " +
            data.trace.draft.response_event +
            ". A integridade da captura não verifica as citações ou a sustentação deste rascunho.",
        ),
      );
      detail(
        root,
        "Afirmações, alternativas e limitações do rascunho original",
        data.trace.draft.output,
      );
    }
    if (meta.unanswered_model_requests || meta.unanswered_mcp_requests)
      root.append(
        el(
          "p",
          "Há requisições sem resposta preservada. Essa lacuna permanece explícita.",
          "experience-warning",
        ),
      );
    const trajectory = el("details");
    trajectory.append(
      el(
        "summary",
        "Percurso registrado · " + data.trace.event_count + " eventos",
      ),
    );
    for (const event of data.trace.events) {
      const box = el("details");
      box.append(
        el("summary", event.id + " · " + event.kind + " · " + event.at),
      );
      box.addEventListener("toggle", () => {
        if (box.open && box.children.length === 1)
          box.append(el("pre", JSON.stringify(event.payload, null, 2)));
      });
      trajectory.append(box);
    }
    root.append(trajectory);
  } else
    detail(
      root,
      "Relato original preservado · respostas brutas ausentes",
      data.report,
    );
  const links = el("div", undefined, "experience-links");
  for (const [title, href] of [
    ["Abrir percurso na mesa", "/mesa?inquiry=" + id],
    [
      "Exportar percurso com originais",
      "/api/v1/export/inquiries/" + id + "?window=" + state.window,
    ],
  ]) {
    const a = el("a", title);
    a.href = href;
    links.append(a);
  }
  root.append(links, el("h3", "Avaliações registradas"));
  if (!data.assessments.length)
    root.append(
      el(
        "p",
        "Avaliação humana pendente. Nenhuma conclusão é homologada automaticamente.",
      ),
    );
  for (const assessment of data.assessments)
    root.append(
      el(
        "p",
        assessment.text + " · " + assessment.created_by,
        "experience-assessment",
      ),
    );
  const form = el("form");
  const fields = {};
  for (const [key, title, options] of [
    [
      "pertinence",
      "Pertinência: a evidência sustenta a afirmação?",
      [
        ["pending", "Ainda não avaliada"],
        ["supported", "Sustentação adequada"],
        ["insufficient", "Sustentação insuficiente"],
      ],
    ],
    [
      "scope",
      "Alcance: a conclusão respeita os limites?",
      [
        ["pending", "Ainda não avaliado"],
        ["bounded", "Alcance delimitado"],
        ["exceeds", "Há extrapolação"],
      ],
    ],
    [
      "attention",
      "A contribuição merece atenção?",
      [
        ["pending", "Ainda não avaliada"],
        ["relevant", "Relevante"],
        ["irrelevant", "Sem relevância demonstrada"],
      ],
    ],
  ]) {
    const label = el("label", title),
      select = el("select");
    for (const [value, text] of options) select.append(new Option(text, value));
    fields[key] = select;
    label.append(select);
    form.append(label);
  }
  const noteLabel = el(
      "label",
      "Justificativa e limites da avaliação",
      "full-width",
    ),
    note = el("textarea");
  note.required = true;
  note.maxLength = 20000;
  noteLabel.append(note);
  const submit = el("button", "Registrar avaliação", "primary");
  submit.type = "submit";
  form.append(noteLabel, submit);
  form.onsubmit = async (event) => {
    event.preventDefault();
    submit.disabled = true;
    try {
      if (state.window !== windowAtRequest || state.current !== id)
        throw new Error(
          "O recorte mudou; reabra a experiência antes de avaliar.",
        );
      const dimensions = Object.fromEntries(
        Object.entries(fields).map(([k, v]) => [k, v.value]),
      );
      await api("assessments", {
        inquiry_id: id,
        target: { type: "inquiries", id },
        basis: [{ type: "inquiries", id }],
        text:
          "Pertinência: " +
          dimensions.pertinence +
          "; alcance: " +
          dimensions.scope +
          "; atenção: " +
          dimensions.attention +
          ".\n" +
          note.value,
        dimensions,
        status: "REPORTED",
      });
      await show(id);
      message("Avaliação registrada, sem promoção automática da conclusão.");
    } catch (e) {
      message(e.message);
    } finally {
      submit.disabled = false;
    }
  };
  root.append(form);
}
$("open-import").onclick = () => {
  $("import-panel").hidden = false;
  $("capture-title").focus();
};
$("cancel-import").onclick = () => {
  $("import-panel").hidden = true;
};
$("import-form").oninput = () => {
  state.requestId = crypto.randomUUID();
};
$("capture-file").onchange = async () => {
  const file = $("capture-file").files[0];
  if (!file) return;
  if (file.size > 650000) {
    message("Arquivo acima do limite de 650.000 bytes.");
    return;
  }
  if (!$("capture-title").value) $("capture-title").value = file.name;
  try {
    const first = JSON.parse((await file.text()).split("\n")[0]);
    if (first.kind === "mission") {
      $("capture-kind").value = "trace";
      $("capture-producer").value = "local_harness";
      $("capture-memory").value =
        first.payload.config?.memory_policy || "not_recorded";
      $("capture-purpose").value =
        first.payload.config?.experiment_purpose || "retrospective";
    }
  } catch {
    /* A secondary report is plain text; do not infer missing provenance. */
  }
};
$("import-form").onsubmit = async (event) => {
  event.preventDefault();
  const submit = $("save-experience");
  submit.disabled = true;
  try {
    const file = $("capture-file").files[0];
    if (!file || file.size > 650000)
      throw new Error("Selecione um arquivo de até 650.000 bytes.");
    const text = await file.text(),
      capture = $("capture-kind").value;
    const runId =
      capture === "trace"
        ? JSON.parse(text.split("\n")[0]).payload.run_id
        : state.requestId;
    const data = await api("experiences", {
      request_id: state.requestId,
      run_id: runId,
      title: $("capture-title").value,
      capture,
      producer: $("capture-producer").value,
      purpose: $("capture-purpose").value,
      memory_policy: $("capture-memory").value,
      limitations: $("capture-limitations").value,
      [capture === "trace" ? "journal" : "report"]: text,
    });
    $("import-panel").hidden = true;
    state.requestId = crypto.randomUUID();
    await list();
    await show(data.inquiry_id);
    message("Experiência preservada com suas limitações.");
  } catch (e) {
    message(e.message);
  } finally {
    submit.disabled = false;
  }
};
$("more-experiences").onclick = () => {
  state.offset = state.next;
  list(false).catch((e) => message(e.message));
};
$("window").value = String(state.window);
$("window").onchange = async () => {
  state.window = Number($("window").value);
  localStorage.setItem("noese.window", state.window);
  state.current = null;
  $("experience-detail").replaceChildren(el("h2", "Escolha uma experiência"));
  await list();
};
(async () => {
  try {
    state.token = (await api("session")).token;
    await list();
    const id = new URLSearchParams(location.search).get("inquiry");
    if (id) await show(id);
  } catch (e) {
    message(e.message);
  }
})();
