"use strict";
const $ = (id) => document.getElementById(id);
const state = {
  token: "",
  window: Number(localStorage.getItem("noese.window") ?? 2),
  inquiry: null,
  artifact: null,
  text: "",
  selection: null,
  artifacts: [],
  records: [],
  capabilities: [],
  sourceLimit: 100,
  noteLimit: 100,
};
const names = {
  statements: "Enunciado",
  relations: "Ligação candidata",
  opportunities: "Possibilidade",
  assessments: "Avaliação humana",
  contributions: "Contribuição",
  runs: "Resultado local",
};
function node(tag, text, cls) {
  const e = document.createElement(tag);
  if (text !== undefined) e.textContent = text;
  if (cls) e.className = cls;
  return e;
}
function status(text, error = false) {
  $("status").textContent = text;
  $("status").classList.toggle("error", error);
}
function failure(e) {
  status(e.message || String(e), true);
}
function action(text, handler, cls = "quiet") {
  const b = node("button", text, cls);
  b.type = "button";
  b.addEventListener("click", () =>
    Promise.resolve().then(handler).catch(failure),
  );
  return b;
}
async function api(path, options = {}) {
  const join = path.includes("?") ? "&" : "?";
  const headers = { "X-NOESE-Token": state.token, ...options.headers };
  if (options.body && !(options.body instanceof FormData)) {
    headers["Content-Type"] = "application/json";
    options.body = JSON.stringify(options.body);
  }
  const res = await fetch("/api/v1/" + path + join + "window=" + state.window, {
    ...options,
    headers,
  });
  const data = await res.json();
  if (!res.ok)
    throw new Error(
      (data.code || res.status) + ": " + (data.message || "Falha"),
    );
  return data;
}
function input(label, kind = "textarea", value = "") {
  const l = node("label", label);
  const e = node(kind);
  e.value = value;
  l.append(e);
  $("dialog-content").append(l);
  return e;
}
function select(label, options, value) {
  const e = input(label, "select");
  for (const [v, t] of options) {
    const o = node("option", t);
    o.value = v;
    e.append(o);
  }
  if (value !== undefined) e.value = value;
  return e;
}
let submitHandler = null;
function modal(title, build, onSubmit = null, submitText = "Salvar") {
  $("dialog-title").textContent = title;
  $("dialog-content").replaceChildren();
  $("dialog-submit").classList.toggle("hidden", !onSubmit);
  $("dialog-submit").textContent = submitText;
  submitHandler = onSubmit;
  build();
  if (!$("dialog").open) $("dialog").showModal();
}
function detail(title, data) {
  modal(title, () => {
    $("dialog-content").append(
      node("pre", JSON.stringify(data, null, 2), "detail"),
    );
  });
}
function requireInquiry() {
  if (!state.inquiry)
    throw new Error("Abra uma investigação para registrar este percurso.");
  return state.inquiry.id;
}
async function refreshInquiries() {
  const saved = state.inquiry?.id || localStorage.getItem("noese.inquiry");
  const data = await api("inquiries?limit=500");
  $("inquiry").replaceChildren(new Option("Escolha um percurso", ""));
  for (const item of data.items)
    $("inquiry").append(
      new Option(item.title || "Investigação aberta", item.id),
    );
  state.inquiry = data.items.find((i) => i.id === saved) || null;
  $("inquiry").value = state.inquiry?.id || "";
  if (state.inquiry) localStorage.setItem("noese.inquiry", state.inquiry.id);
  $("export").classList.toggle("hidden", !state.inquiry);
  $("timeline").classList.toggle("hidden", !state.inquiry);
  if (state.inquiry)
    $("export").href =
      "/api/v1/export/inquiries/" +
      state.inquiry.id +
      "?window=" +
      state.window;
  await refreshRecords();
}
async function refreshSources() {
  const term = $("search").value.trim();
  const data = term
    ? await api("search?q=" + encodeURIComponent(term) + "&limit=100")
    : await api("artifacts?limit=" + state.sourceLimit);
  state.artifacts = term ? data.items.map((i) => i.artifact) : data.items;
  $("source-count").textContent =
    String(state.artifacts.length) +
    (state.artifacts.length >= state.sourceLimit ? "+" : "");
  $("more-sources").classList.toggle(
    "hidden",
    !!term ||
      state.artifacts.length < state.sourceLimit ||
      state.sourceLimit >= 500,
  );
  $("source-list").replaceChildren();
  for (const a of state.artifacts) {
    const button = action(
      "",
      () => openSource(a),
      "source-item" + (a.id === state.artifact?.id ? " active" : ""),
    );
    button.append(
      node("span", a.filename, "source-name"),
      node(
        "small",
        (a.attributed_author || "Autoria desconhecida") +
          " · " +
          a.processing +
          " · J" +
          a.window,
      ),
    );
    $("source-list").append(button);
  }
  if (!state.artifacts.length)
    $("source-list").append(
      node(
        "p",
        term
          ? "Nenhum resultado neste recorte. Outras fontes podem não estar indexadas."
          : "Nenhuma fonte neste recorte.",
        "muted",
      ),
    );
}
async function openSource(a, anchor = null) {
  state.artifact = a;
  state.selection = null;
  state.text = "";
  $("reader-empty").classList.add("hidden");
  $("reader-body").classList.remove("hidden");
  $("source-title").textContent = a.filename;
  $("source-meta").replaceChildren();
  for (const text of [
    a.coverage,
    "autoria: " + (a.attributed_author || "desconhecida"),
    "ocorrência: " + (a.occurred_at || "sem data conhecida"),
    "importação: " + a.imported_at,
    "J" + a.window,
  ])
    $("source-meta").append(node("span", text, "badge"));
  $("source-text").classList.add("hidden");
  $("source-image").classList.add("hidden");
  $("preview-limit").classList.add("hidden");
  $("original").classList.remove("hidden");
  $("source-history").classList.remove("hidden");
  $("original").href =
    "/api/v1/artifacts/" + a.id + "/original?window=" + state.window;
  const url = "/api/v1/artifacts/" + a.id + "/content?window=" + state.window;
  if (a.processing === "INDEXED") {
    const res = await fetch(url);
    if (!res.ok) throw new Error("Falha ao abrir fonte");
    state.text = await res.text();
    $("source-text").textContent = state.text;
    $("source-text").classList.remove("hidden");
    if (anchor && anchor.status === "RESOLVED") {
      const bytes = new TextEncoder().encode(state.text),
        decoder = new TextDecoder();
      $("source-text").replaceChildren(
        document.createTextNode(decoder.decode(bytes.slice(0, anchor.start))),
        node("mark", decoder.decode(bytes.slice(anchor.start, anchor.end))),
        document.createTextNode(decoder.decode(bytes.slice(anchor.end))),
      );
      $("source-text")
        .querySelector("mark")
        .scrollIntoView({ block: "center", behavior: "smooth" });
    }
  } else if (a.mime.startsWith("image/")) {
    $("source-image").src = url;
    $("source-image").classList.remove("hidden");
  } else $("preview-limit").classList.remove("hidden");
  $("selection-label").textContent =
    "Selecione um trecho para ancorar uma nota.";
  await refreshSources();
}
function selection() {
  const s = window.getSelection();
  if (!s || s.isCollapsed || !s.rangeCount) return null;
  const range = s.getRangeAt(0);
  if (!$("source-text").contains(range.commonAncestorContainer)) return null;
  const before = range.cloneRange();
  before.selectNodeContents($("source-text"));
  before.setEnd(range.startContainer, range.startOffset);
  const encoder = new TextEncoder();
  const start = encoder.encode(before.toString()).length;
  return {
    artifact_id: state.artifact.id,
    start,
    end: start + encoder.encode(range.toString()).length,
    quote: range.toString(),
  };
}
document.addEventListener("selectionchange", () => {
  const s = selection();
  if (s) {
    state.selection = s;
    $("selection-label").textContent =
      s.quote.length + " caracteres selecionados · trecho exato";
  }
});
async function makeAnchor() {
  if (!state.artifact) return null;
  const payload =
    state.selection ||
    (state.artifact.processing === "INDEXED"
      ? {
          artifact_id: state.artifact.id,
          start: 0,
          end: new TextEncoder().encode(state.text).length,
        }
      : {
          artifact_id: state.artifact.id,
          unresolved: true,
          limitation:
            "Referência ao original inteiro; sem coordenadas interpretadas",
        });
  return api("anchors", { method: "POST", body: payload });
}
async function refreshRecords() {
  state.records = [];
  $("notes").replaceChildren();
  $("notes-empty").classList.toggle("hidden", !!state.inquiry);
  if (!state.inquiry) return;
  const types = [
    "statements",
    "relations",
    "opportunities",
    "contributions",
    "assessments",
    "runs",
  ];
  const results = await Promise.all(
    types.map((t) =>
      api(t + "?inquiry_id=" + state.inquiry.id + "&limit=" + state.noteLimit),
    ),
  );
  for (let i = 0; i < types.length; i++)
    for (const record of results[i].items)
      state.records.push({ ...record, _type: types[i] });
  state.records.sort((a, b) => b.created_at.localeCompare(a.created_at));
  $("more-notes").classList.toggle(
    "hidden",
    !results.some((r) => r.items.length >= state.noteLimit) ||
      state.noteLimit >= 500,
  );
  if (!state.records.length)
    $("notes").append(
      node(
        "p",
        "O percurso está aberto. Leia uma fonte, anote uma dúvida ou deixe uma primeira interpretação.",
        "small-empty",
      ),
    );
  for (const r of state.records) renderRecord(r);
}
function renderRecord(r) {
  const card = node("article", undefined, "card");
  card.append(
    node("span", names[r._type] || r._type, "eyebrow"),
    node(
      "span",
      r.status,
      "badge " +
        (r.status === "DISPUTED"
          ? "disputed"
          : r.status === "INFERRED_CANDIDATE"
            ? "candidate"
            : ""),
    ),
    node(
      "p",
      r.text ||
        (r.output?.equal
          ? "Os dois trechos são literalmente iguais."
          : "Os dois trechos apresentam diferenças literais."),
    ),
  );
  if (r._type === "runs") {
    card.append(
      node("small", "TRECHO A"),
      node("blockquote", r.output.left),
      node("small", "TRECHO B"),
      node("blockquote", r.output.right),
      node(
        "p",
        "Esta comparação não determina mudança de sentido ou influência. Avalie a consequência para o percurso.",
        "muted",
      ),
    );
  }
  card.append(
    node(
      "small",
      "v" +
        r.version +
        " · " +
        (r.created_by || "local_user") +
        " · J" +
        r.window,
    ),
  );
  if (r.basis_unresolved)
    card.append(node("p", "Limite: " + r.limitation, "muted"));
  const actions = node("div", undefined, "card-actions");
  if (r.basis?.length)
    actions.append(action("Ver suporte", () => showBasis(r.basis)));
  actions.append(
    action("Histórico", async () =>
      detail(
        "Versões e acontecimentos",
        await api("history/" + r._type + "/" + r.id),
      ),
    ),
  );
  if (r._type !== "runs" && r._type !== "contributions")
    actions.append(action("Revisar", () => editRecord(r)));
  if (r._type !== "assessments")
    actions.append(action("Avaliar", () => assess(r)));
  if (r._type === "opportunities") {
    actions.append(action("Propor contribuição", () => proposeContribution(r)));
    if (!["rejected", "deferred"].includes(r.status)) {
      actions.append(
        action("Adiar", () => decision(r, "deferred")),
        action("Rejeitar", () => decision(r, "rejected")),
      );
    }
  }
  if (r._type === "contributions" && r.status === "proposed")
    actions.append(action("Examinar autorização", () => authorize(r)));
  card.append(actions);
  $("notes").append(card);
}
async function showBasis(refs) {
  const entries = await Promise.all(
    refs.map(async (r) => ({ ref: r, record: await api(r.type + "/" + r.id) })),
  );
  modal("Suporte e localização", () => {
    for (const { ref, record } of entries) {
      $("dialog-content").append(
        node("pre", JSON.stringify(record, null, 2), "detail"),
      );
      if (ref.type === "anchors" || ref.type === "artifacts")
        $("dialog-content").append(
          action("Abrir no original", async () => {
            const a =
              ref.type === "artifacts"
                ? record
                : await api("artifacts/" + record.artifact_id);
            $("dialog").close();
            await openSource(a, ref.type === "anchors" ? record : null);
          }),
        );
    }
  });
}
function note(anchored) {
  requireInquiry();
  if (anchored && !state.artifact)
    throw new Error("Selecione uma fonte primeiro.");
  let text, statusSelect;
  modal(
    anchored ? "Anotar a partir da fonte" : "Enunciado sem fonte definida",
    () => {
      if (anchored)
        $("dialog-content").append(
          node(
            "p",
            state.selection
              ? "Trecho: “" + state.selection.quote + "”"
              : "A nota apontará para a fonte inteira.",
            "dialog-note",
          ),
        );
      text = input("Sua nota, interpretação ou pergunta");
      text.required = true;
      statusSelect = select("Estado da interpretação", [
        ["INFERRED_CANDIDATE", "Inferência candidata"],
        ["REPORTED", "Alegação atribuída"],
        ["OBSERVED_IN_CORPUS", "Observado no corpus"],
        ["UNRESOLVED", "Em aberto"],
        ["DISPUTED", "Contestado"],
      ]);
    },
    async () => {
      const a = anchored ? await makeAnchor() : null;
      await api("statements", {
        method: "POST",
        body: {
          inquiry_id: requireInquiry(),
          text: text.value,
          status: statusSelect.value,
          basis: a ? [{ type: "anchors", id: a.id }] : [],
          basis_unresolved: !a,
          limitation: a ? "" : "Enunciado livre, sem fonte vinculada",
        },
      });
      await refreshRecords();
      status(
        "Nota registrada; a fonte e sua interpretação permanecem separadas.",
      );
    },
  );
}
function editRecord(r) {
  let text, reason, epistate;
  modal(
    "Revisar preservando a versão anterior",
    () => {
      text = input("Enunciado", "textarea", r.text);
      text.required = true;
      reason = input("Razão da revisão", "textarea");
      reason.required = true;
      epistate = select(
        "Estado",
        [
          ["INFERRED_CANDIDATE", "Inferência candidata"],
          ["REPORTED", "Alegação atribuída"],
          ["OBSERVED_IN_CORPUS", "Observado no corpus"],
          ["DERIVED", "Derivado"],
          ["DISPUTED", "Contestado"],
          ["UNRESOLVED", "Em aberto"],
          ["WITHDRAWN", "Retirado"],
        ],
        r.status,
      );
    },
    async () => {
      await api(r._type + "/" + r.id, {
        method: "PUT",
        body: {
          version: r.version,
          text: text.value,
          revision_reason: reason.value,
          status: epistate.value,
        },
      });
      await refreshRecords();
      status("Nova versão registrada. A anterior continua no histórico.");
    },
  );
}
function assess(r) {
  requireInquiry();
  let text;
  modal(
    "Avaliar ou contrapor",
    () => {
      $("dialog-content").append(
        node("p", r.text || "Resultado de comparação local", "dialog-note"),
      );
      text = input(
        "Avaliação: o que mudou, o que não contribuiu, que limite permanece?",
      );
      text.required = true;
    },
    async () => {
      await api("assessments", {
        method: "POST",
        body: {
          inquiry_id: requireInquiry(),
          text: text.value,
          status: "REPORTED",
          target: { type: r._type, id: r.id },
          basis: [{ type: r._type, id: r.id }],
        },
      });
      await refreshRecords();
      status("Avaliação humana registrada com referência ao objeto avaliado.");
    },
  );
}
function decision(r, next) {
  let reason;
  modal(
    next === "deferred" ? "Adiar possibilidade" : "Rejeitar possibilidade",
    () => {
      reason = input("Razão da decisão");
      reason.required = true;
    },
    async () => {
      await api("opportunities/" + r.id, {
        method: "PUT",
        body: {
          version: r.version,
          status: next,
          revision_reason: reason.value,
        },
      });
      await refreshRecords();
    },
  );
}
function referenceOptions() {
  return [
    ...state.artifacts.map((a) => [
      "artifacts:" + a.id,
      "Fonte · " + a.filename,
    ]),
    ...state.records
      .filter((r) => r._type !== "runs")
      .map((r) => [
        r._type + ":" + r.id,
        (names[r._type] || r._type) + " · " + r.text.slice(0, 80),
      ]),
  ];
}
function parseRef(value) {
  const [type, id] = value.split(":");
  return { type, id };
}
function relate() {
  requireInquiry();
  const opts = referenceOptions();
  if (opts.length < 2)
    throw new Error(
      "Importe ou anote ao menos dois elementos para relacionar.",
    );
  let left, right, text;
  modal(
    "Propor uma relação",
    () => {
      left = select("Primeira referência", opts);
      right = select("Segunda referência", opts, opts[1][0]);
      text = input("Que relação você propõe e em que se apoia?");
      text.required = true;
      $("dialog-content").append(
        node(
          "p",
          "A ligação é uma interpretação candidata. Proximidade e sequência não demonstram influência.",
          "dialog-note",
        ),
      );
    },
    async () => {
      if (left.value === right.value)
        throw new Error("Escolha duas referências diferentes.");
      const refs = [parseRef(left.value), parseRef(right.value)];
      await api("relations", {
        method: "POST",
        body: {
          inquiry_id: requireInquiry(),
          text: text.value,
          rationale: text.value,
          status: "INFERRED_CANDIDATE",
          references: refs,
          basis: refs,
        },
      });
      await refreshRecords();
    },
  );
}
async function explore() {
  const id = requireInquiry();
  const result = await api("explore/" + id);
  $("candidate-list").replaceChildren();
  for (const c of result.items) {
    const card = node("article", undefined, "card candidate-card");
    card.append(
      node("span", "POSSIBILIDADE CANDIDATA", "eyebrow"),
      node("p", c.text),
      node("small", c.rule + " · J" + state.window),
    );
    card.append(
      action("Examinar e registrar", () => {
        let text;
        modal(
          "Considerar uma possibilidade",
          () => {
            text = input("Possibilidade proposta", "textarea", c.text);
            $("dialog-content").append(
              node("p", "Alternativa: " + c.alternative, "dialog-note"),
              node("p", result.limitation, "dialog-note"),
            );
          },
          async () => {
            await api("opportunities", {
              method: "POST",
              body: {
                inquiry_id: id,
                text: text.value,
                basis: c.basis,
                status: "INFERRED_CANDIDATE",
                rule: c.rule,
                alternative: c.alternative,
                scope: c.scope,
                proposed_capability: c.proposed_capability,
              },
            });
            await refreshRecords();
            status("Possibilidade registrada para deliberação.");
          },
        );
      }),
    );
    $("candidate-list").append(card);
  }
  status(
    result.items.length
      ? result.items.length +
          " possibilidades candidatas, sujeitas a avaliação. " +
          result.limitation
      : "Nada sustentado por enquanto. " + result.limitation,
  );
}
function proposeContribution(r) {
  let text, capability;
  modal(
    "Propor uma contribuição",
    () => {
      text = input("Contribuição e efeito esperado", "textarea", r.text);
      capability = select(
        "Capacidade catalogada",
        state.capabilities.map((c) => [
          c.id,
          c.purpose + " · " + c.availability,
        ]),
        r.proposed_capability || "human-assessment",
      );
      $("dialog-content").append(
        node(
          "p",
          "A proposta não executa uma ação. Consultas externas permanecem indisponíveis.",
          "dialog-note",
        ),
      );
    },
    async () => {
      await api("contributions", {
        method: "POST",
        body: {
          inquiry_id: requireInquiry(),
          opportunity_id: r.id,
          text: text.value,
          status: "proposed",
          capability_id: capability.value,
          basis: [{ type: "opportunities", id: r.id }],
        },
      });
      await refreshRecords();
    },
  );
}
function authorize(r) {
  let scope;
  modal(
    "Registrar autorização delimitada",
    () => {
      $("dialog-content").append(
        node("p", r.text, "dialog-note"),
        node(
          "p",
          "Disponibilidade: " +
            r.availability +
            ". A autorização registra uma decisão; não executa uma consulta externa.",
          "dialog-note",
        ),
      );
      scope = input("Escopo exato e limites da autorização");
      scope.required = true;
    },
    async () => {
      await api("contributions/" + r.id, {
        method: "PUT",
        body: {
          version: r.version,
          status: "authorized",
          authorization_scope: scope.value,
          revision_reason: "Autorização humana delimitada",
        },
      });
      await refreshRecords();
    },
    "Registrar autorização",
  );
}
async function compare() {
  const inquiry = requireInquiry();
  const anchors = (await api("anchors?limit=500")).items.filter(
    (a) => a.status === "RESOLVED",
  );
  if (anchors.length < 2)
    throw new Error("Anote dois trechos de fontes para compará-los.");
  let left, right;
  const opts = anchors.map((a) => [
    a.id,
    a.quote.slice(0, 90) || "(trecho vazio)",
  ]);
  modal(
    "Comparar dois trechos locais",
    () => {
      left = select("Trecho A", opts);
      right = select("Trecho B", opts, opts[1][0]);
      $("dialog-content").append(
        node(
          "p",
          "Esta ação registra a proposta e a autorização para estes dois trechos, executa a comparação literal e preserva o resultado. O efeito sobre a investigação exige sua avaliação.",
          "dialog-note",
        ),
      );
    },
    async () => {
      if (left.value === right.value)
        throw new Error("Selecione dois trechos diferentes.");
      const basis = [
        { type: "anchors", id: left.value },
        { type: "anchors", id: right.value },
      ];
      const opp = await api("opportunities", {
        method: "POST",
        body: {
          inquiry_id: inquiry,
          text: "Comparar literalmente dois trechos selecionados para examinar diferenças.",
          basis,
          status: "INFERRED_CANDIDATE",
          scope: "Dois trechos locais",
        },
      });
      let contribution = await api("contributions", {
        method: "POST",
        body: {
          inquiry_id: inquiry,
          opportunity_id: opp.id,
          text: "Comparação local dos dois trechos selecionados",
          status: "proposed",
          capability_id: "text-compare",
          basis,
        },
      });
      contribution = await api("contributions/" + contribution.id, {
        method: "PUT",
        body: {
          version: contribution.version,
          status: "authorized",
          authorization_scope: JSON.stringify({
            left_anchor: left.value,
            right_anchor: right.value,
          }),
          revision_reason: "Autorização pelo botão de comparação local",
        },
      });
      await api("runs", {
        method: "POST",
        body: {
          contribution_id: contribution.id,
          left_anchor: left.value,
          right_anchor: right.value,
        },
      });
      await refreshRecords();
      status(
        "Comparação executada. Avalie o resultado no percurso para registrar sua consequência.",
      );
    },
    "Autorizar e comparar localmente",
  );
}
async function upload(files) {
  for (const f of files) {
    const data = new FormData();
    data.append("file", f);
    status("Preservando " + f.name + "…");
    const a = await api("artifacts", { method: "POST", body: data });
    await refreshSources();
    await openSource(a);
  }
  status(
    "Originais preservados. Nenhum conteúdo importado é executado como instrução.",
  );
}
$("dialog-close").onclick = $("dialog-cancel").onclick = () =>
  $("dialog").close();
$("dialog-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  if (!submitHandler) return;
  const handler = submitHandler;
  $("dialog-submit").disabled = true;
  try {
    await handler();
    $("dialog").close();
  } catch (err) {
    failure(err);
  } finally {
    $("dialog-submit").disabled = false;
  }
});
$("new-inquiry").onclick = () => {
  let title;
  modal(
    "Abrir uma investigação",
    () => {
      title = input("Título opcional", "input");
      title.placeholder = "Investigação aberta";
      $("dialog-content").append(
        node(
          "p",
          "Você pode começar sem hipótese, categoria ou pergunta prévia.",
          "dialog-note",
        ),
      );
    },
    async () => {
      state.inquiry = await api("inquiries", {
        method: "POST",
        body: {
          title: title.value || "Investigação aberta",
          status: "UNRESOLVED",
        },
      });
      await refreshInquiries();
      status("Percurso aberto.");
    },
  );
};
$("inquiry").onchange = async () => {
  try {
    state.inquiry = $("inquiry").value
      ? await api("inquiries/" + $("inquiry").value)
      : null;
    localStorage.setItem("noese.inquiry", state.inquiry?.id || "");
    await refreshInquiries();
    $("candidate-list").replaceChildren();
  } catch (e) {
    failure(e);
  }
};
$("window").value = String(state.window);
$("window").onchange = async () => {
  state.window = Number($("window").value);
  localStorage.setItem("noese.window", String(state.window));
  state.artifact = null;
  state.selection = null;
  state.text = "";
  $("reader-empty").classList.remove("hidden");
  $("reader-body").classList.add("hidden");
  $("original").classList.add("hidden");
  $("source-history").classList.add("hidden");
  $("candidate-list").replaceChildren();
  try {
    await Promise.all([refreshSources(), refreshInquiries()]);
    status(
      "Recorte J" +
        state.window +
        " aplicado a fontes, busca, registros, histórico e exportação.",
    );
  } catch (e) {
    failure(e);
  }
};
$("upload").onchange = () => upload($("upload").files).catch(failure);
$("drop").onkeydown = (e) => {
  if (e.key === "Enter" || e.key === " ") {
    e.preventDefault();
    $("upload").click();
  }
};
for (const name of ["dragenter", "dragover"])
  $("drop").addEventListener(name, (e) => {
    e.preventDefault();
    $("drop").classList.add("drag");
  });
$("drop").addEventListener("dragleave", () =>
  $("drop").classList.remove("drag"),
);
$("drop").addEventListener("drop", (e) => {
  e.preventDefault();
  $("drop").classList.remove("drag");
  upload(e.dataTransfer.files).catch(failure);
});
$("search-form").onsubmit = (e) => {
  e.preventDefault();
  refreshSources().catch(failure);
};
$("search").addEventListener("input", () => {
  if (!$("search").value) refreshSources().catch(failure);
});
$("annotate").onclick = () => {
  try {
    note(true);
  } catch (e) {
    failure(e);
  }
};
$("free-note").onclick = () => {
  try {
    note(false);
  } catch (e) {
    failure(e);
  }
};
$("relate").onclick = () => {
  try {
    relate();
  } catch (e) {
    failure(e);
  }
};
$("compare").onclick = () => compare().catch(failure);
$("explore").onclick = () => explore().catch(failure);
$("lab").onclick = async () => {
  try {
    const r = await api("lab/import", { method: "POST", body: {} });
    await refreshSources();
    status(
      "LAB-001: " +
        r.artifacts.length +
        " excertos disponíveis neste recorte. " +
        r.limitation,
    );
  } catch (e) {
    failure(e);
  }
};
$("backup").onclick = async () => {
  try {
    const r = await api("backups", { method: "POST", body: {} });
    detail("Backup consistente criado", r);
    status("Backup criado com banco, originais e manifesto de hashes.");
  } catch (e) {
    failure(e);
  }
};
$("source-history").onclick = async () => {
  try {
    const a = state.artifact;
    const h = await api("history/artifacts/" + a.id);
    modal("Proveniência e integridade", () => {
      $("dialog-content").append(
        node("pre", JSON.stringify({ artifact: a, ...h }, null, 2), "detail"),
      );
      $("dialog-content").append(
        action("Excluir fonte e registros dependentes", () => {
          let hash;
          modal(
            "Exclusão de dados sensíveis",
            () => {
              $("dialog-content").append(
                node(
                  "p",
                  "Remove o original e registros que dependem dele deste acervo. A operação é irreversível. Backups e exportações anteriores devem ser excluídos separadamente.",
                  "dialog-note",
                ),
                node("pre", a.sha256, "detail"),
              );
              hash = input("Digite o SHA-256 acima para confirmar", "input");
            },
            async () => {
              await api("artifacts/" + a.id, {
                method: "DELETE",
                body: { confirm_sha256: hash.value },
              });
              state.artifact = null;
              $("reader-body").classList.add("hidden");
              $("reader-empty").classList.remove("hidden");
              $("original").classList.add("hidden");
              $("source-history").classList.add("hidden");
              await Promise.all([refreshSources(), refreshRecords()]);
              status("Fonte e registros dependentes excluídos deste acervo.");
            },
            "Excluir definitivamente",
          );
        }),
      );
    });
  } catch (e) {
    failure(e);
  }
};
$("capabilities").onclick = () =>
  detail("Capacidades e limites", state.capabilities);
$("more-sources").onclick = () => {
  state.sourceLimit = Math.min(500, state.sourceLimit + 100);
  refreshSources().catch(failure);
};
$("more-notes").onclick = () => {
  state.noteLimit = Math.min(500, state.noteLimit + 100);
  refreshRecords().catch(failure);
};
(async () => {
  try {
    state.token = (await api("session")).token;
    state.capabilities = (await api("capabilities")).items;
    await Promise.all([refreshSources(), refreshInquiries()]);
  } catch (e) {
    failure(e);
  }
})();

$("timeline").onclick = async () => {
  try {
    detail(
      "Trajetória · ordem de registro, sem causalidade",
      await api("history/inquiries/" + requireInquiry()),
    );
  } catch (e) {
    failure(e);
  }
};
