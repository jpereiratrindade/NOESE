#include "Service.hpp"
#include <set>
namespace {
const std::set<std::string> epistates = {
    "SOURCE_RECORDED",    "REPORTED", "OBSERVED_IN_CORPUS", "DERIVED",
    "INFERRED_CANDIDATE", "DISPUTED", "UNRESOLVED",         "WITHDRAWN"};
std::string require_text(const json &j, const std::string &key) {
  if (!j.contains(key) || !j[key].is_string() || j[key].template get<std::string>().empty() ||
      j[key].template get<std::string>().size() > 100000)
    throw Error(400, "INVALID_INPUT", "Campo obrigatório: " + key);
  return j[key].template get<std::string>();
}
bool textual(const std::string &b) {
  if (b.find('\0') != std::string::npos)
    return false;
  try {
    json(b).dump();
    return true;
  } catch (...) {
    return false;
  }
}
std::string mime(const std::string &b) {
  if (b.starts_with("\x89PNG\r\n\x1a\n"))
    return "image/png";
  if (b.size() > 3 && (unsigned char)b[0] == 255 && (unsigned char)b[1] == 216 &&
      (unsigned char)b[2] == 255)
    return "image/jpeg";
  if (b.starts_with("%PDF-"))
    return "application/pdf";
  return textual(b) ? "text/plain; charset=utf-8" : "application/octet-stream";
}
json cap(std::string id, std::string purpose, bool local) {
  return {{"id", id},
          {"purpose", purpose},
          {"input_contract", "referências locais e janela explícita"},
          {"output_contract", "resultado com proveniência e limites"},
          {"availability", local ? "AVAILABLE" : "CAPABILITY_UNAVAILABLE"},
          {"local_or_external", local ? "local" : "external"},
          {"required_permissions", local ? "local_session" : "scoped_external_authority"},
          {"cost_or_resource_estimate", "limitado ao corpus local; até 16 MiB por fonte"},
          {"version", "0.1.0"},
          {"failure_modes", json::array({"incomplete_source", "unresolved_anchor"})},
          {"evidence_policy", "resultado derivado não comprova verdade externa"},
          {"window", 0}};
}
} // namespace
Service::Service(fs::path root) : store(root) {
  std::lock_guard lock(store.mutex);
  Transaction tx(store);
  for (auto c :
       {cap("search", "Recuperar texto indexado com âncoras", true),
        cap("text-compare", "Comparação literal de dois fragmentos", true),
        cap("chronology", "Reconstituir eventos registrados, sem causalidade", true),
        cap("coverage-review", "Regras de cobertura e perguntas candidatas", true),
        cap("human-assessment", "Avaliação humana persistente", true),
        cap("external-lookup", "Consulta externa: somente proposta; sem conector", false)}) {
    try {
      store.get("capabilities", c["id"]);
    } catch (const Error &e) {
      if (e.http != 404)
        throw;
      store.save("capabilities", c);
    }
  }
  try {
    store.get("capabilities", "semantic-exploration");
  } catch (const Error &e) {
    if (e.http != 404)
      throw;
    auto semantic =
        cap("semantic-exploration",
            "Investigar relações de sentido com modelo local, fontes e contrapontos", true);
    semantic["availability"] = "REQUIRES_LOCAL_MODEL";
    semantic["required_permissions"] = "local_processing_configuration";
    store.save("capabilities", semantic);
  }
  tx.commit();
}
json Service::import_file(const std::string &filename, const std::string &bytes, json meta,
                          int window) {
  if (filename.empty() || filename.size() > 250 ||
      filename.find_first_of("/\\\r\n") != std::string::npos || filename == "." || filename == "..")
    throw Error(400, "INVALID_INPUT", "Nome de arquivo inválido");
  if (bytes.size() > 16 * 1024 * 1024)
    throw Error(413, "INVALID_INPUT", "Limite: 16 MiB");
  auto hash = sha256(bytes);
  auto detected = mime(bytes);
  bool indexed = detected.starts_with("text/");
  auto path = store.root / "objects" / hash;
  if (!fs::exists(path))
    write_file(path, bytes);
  else
    store.object(hash);
  json a = {
      {"filename", filename},
      {"sha256", hash},
      {"size", bytes.size()},
      {"mime", detected},
      {"declared_mime", meta.value("declared_mime", "unknown")},
      {"processing", indexed ? "INDEXED" : "NOT_INDEXED"},
      {"preview", indexed || detected.starts_with("image/") ? "AVAILABLE" : "PREVIEW_UNAVAILABLE"},
      {"origin", meta.value("origin", "local_upload")},
      {"created_by", meta.value("imported_by", "local_user")},
      {"attributed_author", meta.value("attributed_author", json(nullptr))},
      {"author_certainty", meta.value("author_certainty", "unknown")},
      {"occurred_at", meta.value("occurred_at", json(nullptr))},
      {"source_timezone", meta.value("source_timezone", json(nullptr))},
      {"imported_at", now()},
      {"recorded_at", now()},
      {"coverage", meta.value("coverage", "UNSPECIFIED")},
      {"window", window},
      {"source_metadata", meta}};
  Transaction tx(store);
  a = store.save("artifacts", a);
  store.save("occurrences", {{"artifact_id", a["id"]},
                             {"sha256", hash},
                             {"window", window},
                             {"context", a["origin"]},
                             {"independent_evidence", false},
                             {"created_by", a["created_by"]}});
  if (indexed)
    store.change("INSERT INTO artifact_fts(id,text) VALUES(?,?)", {a["id"], bytes});
  store.event(a["id"], "import", a["created_by"], {{"sha256", hash}, {"window", window}});
  tx.commit();
  return a;
}
json Service::anchor(json in, int window) {
  auto a = store.get("artifacts", require_text(in, "artifact_id"), window);
  in["window"] = a["window"];
  in["source_sha256"] = a["sha256"];
  in["created_by"] = in.value("created_by", "local_user");
  if (in.value("unresolved", false)) {
    require_text(in, "limitation");
    in["status"] = "UNRESOLVED";
  } else {
    auto b = store.object(a["sha256"]);
    if (a["processing"] != "INDEXED")
      throw Error(422, "UNRESOLVED_ANCHOR", "Formato sem coordenadas textuais; use unresolved");
    int64_t start = in.value("start", int64_t(-1)), end = in.value("end", int64_t(-1));
    if (start < 0 || end < start || size_t(end) > b.size())
      throw Error(422, "UNRESOLVED_ANCHOR", "Intervalo inválido (bytes UTF-8)");
    if ((size_t(start) < b.size() && ((unsigned char)b[start] & 0xc0) == 0x80) ||
        (size_t(end) < b.size() && ((unsigned char)b[end] & 0xc0) == 0x80))
      throw Error(422, "UNRESOLVED_ANCHOR", "Intervalo divide caractere UTF-8");
    auto quote = b.substr(start, end - start);
    if (in.contains("quote") && in["quote"].get<std::string>() != quote)
      throw Error(422, "UNRESOLVED_ANCHOR", "Trecho diverge do original");
    in["quote"] = quote;
    in["fragment_sha256"] = sha256(quote);
    in["status"] = "RESOLVED";
    in["offset_unit"] = "utf8_bytes";
  }
  in.erase("id");
  Transaction tx(store);
  auto out = store.save("anchors", in);
  store.event(out["id"], "anchor", in["created_by"]);
  tx.commit();
  return out;
}
json Service::record(const std::string &type, json in, int window) {
  if (in.contains("experience"))
    throw Error(400, "INVALID_INPUT", "Capturas exigem preserve_experience; metadados não são editáveis diretamente");
  const std::set<std::string> allowed = {"inquiries",      "statements",  "relations",
                                         "opportunities",  "assessments", "contributions",
                                         "inquiry_members"};
  if (!allowed.contains(type))
    throw Error(400, "INVALID_INPUT", "Recurso não editável");
  int expected = in.value("version", 0);
  json old;
  if (expected) {
    old = store.get(type, require_text(in, "id"), window);
    require_text(in, "revision_reason");
    json merged = old;
    merged.update(in);
    in = merged;
  } else if (in.contains("id"))
    throw Error(400, "INVALID_INPUT", "ID é gerado pelo servidor");
  std::string actor = in.value("created_by", "local_user");
  if (actor.empty())
    throw Error(400, "INVALID_INPUT", "Autoria da operação obrigatória");
  in["created_by"] = actor;
  int effective = window;
  auto validate_ref = [&](const json &ref) {
    auto target = store.get(require_text(ref, "type"), require_text(ref, "id"), window);
    effective = std::max(effective, target.value("window", 0));
  };
  if (type == "inquiries") {
    in["title"] = in.value("title", "Investigação aberta");
  } else {
    auto inquiry = store.get("inquiries", require_text(in, "inquiry_id"), window);
    effective = std::max(effective, inquiry.value("window", 0));
    if (type != "inquiry_members")
      require_text(in, "text");
  }
  if (in.contains("basis")) {
    if (!in["basis"].is_array())
      throw Error(400, "INVALID_INPUT", "basis deve ser uma lista");
    for (auto &ref : in["basis"]) {
      validate_ref(ref);
      if (type == "statements" && ref.value("type", "") == "artifacts")
        throw Error(422, "UNRESOLVED_ANCHOR",
                    "Enunciados devem citar uma âncora; crie uma âncora da fonte inteira ou marque "
                    "unresolved");
    }
  }
  bool basis = in.contains("basis") && !in["basis"].empty();
  if (type != "inquiries" && type != "inquiry_members" && !basis) {
    if (!in.value("basis_unresolved", false))
      throw Error(422, "UNRESOLVED_ANCHOR", "Indique suporte ou declare a limitação");
    require_text(in, "limitation");
  }
  if (type == "inquiry_members") {
    validate_ref(in.at("reference"));
  }
  if (type == "relations") {
    if (!in.contains("references") || !in["references"].is_array() || in["references"].size() < 2)
      throw Error(400, "INVALID_INPUT", "Ligação exige ao menos duas referências");
    for (auto &ref : in["references"])
      validate_ref(ref);
    require_text(in, "rationale");
  }
  std::string status = in.value("status", "INFERRED_CANDIDATE");
  if (type == "contributions") {
    auto opportunity = store.get("opportunities", require_text(in, "opportunity_id"), window);
    if (opportunity["inquiry_id"] != in["inquiry_id"])
      throw Error(400, "INVALID_INPUT", "Possibilidade pertence a outra investigação");
    if (status == "authorized") {
      require_text(in, "authorization_scope");
      in["authorized_by"] = actor;
      in["authorized_at"] = now();
      if (!expected || old["status"] != "proposed")
        throw Error(409, "CONFLICT", "Autorize uma proposta existente");
    } else if (status != "proposed" && status != "declined")
      throw Error(400, "INVALID_INPUT", "Execução só pode ser registrada por uma capacidade real");
    if (expected && old["status"] != "proposed")
      throw Error(409, "CONFLICT", "Decisão final preservada; crie outra proposta");
    auto capability = store.get("capabilities", require_text(in, "capability_id"), window);
    in["availability"] = capability["availability"];
  } else if (!epistates.contains(status) &&
             !(type == "opportunities" && (status == "rejected" || status == "deferred")))
    throw Error(400, "INVALID_INPUT", "Estado epistêmico inválido");
  in["status"] = status;
  if (type == "opportunities") {
    in["execution"] = "NOT_PERFORMED";
    in["authorization"] = "NOT_REQUESTED";
  }
  if (type == "assessments") {
    validate_ref(in.at("target"));
    in["evaluated_at"] = now();
  }
  in["window"] = effective;
  Transaction tx(store);
  auto out = store.save(type, in, expected);
  store.event(out["id"], expected ? "revision" : "create", actor,
              {{"type", type},
               {"version", out["version"]},
               {"reason", in.value("revision_reason", "")},
               {"source_type", in.value("source_type", "local_session")},
               {"source_ref", in.value("source_ref", "local")},
               {"inquiry_id", in.value("inquiry_id", out["id"].get<std::string>())},
               {"window", effective}});
  tx.commit();
  return out;
}
json Service::search(const std::string &term, int window, int limit, int offset) {
  json out = json::array();
  if (term.empty())
    return {{"items", out},
            {"scope", window},
            {"limitation", "Busca apenas no texto indexado; ausência não implica inexistência"}};
  std::string quoted = "\"";
  for (char c : term) {
    quoted += c;
    if (c == '"')
      quoted += '"';
  }
  quoted += '"';
  auto rows = store.query(
      "SELECT id FROM artifact_fts WHERE artifact_fts MATCH ? AND id IN(SELECT id FROM artifacts "
      "WHERE CAST(json_extract(payload,'$.window') AS INTEGER)<=?) ORDER BY rank LIMIT ? OFFSET ?",
      {quoted, std::to_string(window), std::to_string(limit), std::to_string(offset)});
  for (auto &r : rows) {
    auto a = store.get("artifacts", r["id"], window);
    auto bytes = store.object(a["sha256"]);
    size_t pos = bytes.find(term);
    json match = {
        {"artifact", a},
        {"anchor",
         {{"artifact_id", a["id"]},
          {"unresolved", true},
          {"limitation", "FTS normaliza acentos e caixa; selecione o trecho no original"}}}};
    if (pos != std::string::npos)
      match["anchor"] = {{"artifact_id", a["id"]},          {"start", pos},
                         {"end", pos + term.size()},        {"quote", term},
                         {"fragment_sha256", sha256(term)}, {"offset_unit", "utf8_bytes"}};
    out.push_back(match);
  }
  return {{"items", out},
          {"scope", window},
          {"limit", limit},
          {"offset", offset},
          {"limitation",
           "Somente texto indexado disponível nesta janela; similaridade não prova influência"}};
}
json Service::history(const std::string &type, const std::string &id, int window) {
  store.get(type, id, window);
  auto rows = store.query("SELECT payload FROM " + type + " WHERE id=? ORDER BY version", {id});
  json versions = json::array();
  for (auto &row : rows) {
    auto v = json::parse(row["payload"].get<std::string>());
    if (v.value("window", 0) <= window)
      versions.push_back(v);
  }
  json events = type == "inquiries"
                    ? store.query("SELECT * FROM events WHERE object_id=? OR "
                                  "json_extract(detail,'$.inquiry_id')=? ORDER BY rowid",
                                  {id, id})
                    : store.query("SELECT * FROM events WHERE object_id=? ORDER BY rowid", {id});
  json filtered = json::array();
  for (auto event : events) {
    auto detail = json::parse(event["detail"].get<std::string>());
    if (detail.value("window", 0) <= window) {
      event["detail"] = detail;
      filtered.push_back(event);
    }
  }
  return {{"versions", versions},
          {"events", filtered},
          {"limitation", "Ordem de registro não é data da fala nem relação causal"}};
}
json Service::explore(const std::string &inquiry, int window, const std::string &artifact) {
  if (!inquiry.empty())
    store.get("inquiries", inquiry, window);
  json candidates = json::array();
  auto sources = artifact.empty() ? store.list("artifacts", window, "", 500)
                                  : json::array({store.get("artifacts", artifact, window)});
  for (auto &a : sources) {
    if (a["processing"] == "NOT_INDEXED")
      candidates.push_back(
          {{"rule", "coverage.not_indexed.v1"},
           {"text", "Esta fonte foi preservada, mas seu conteúdo não pode ser recuperado pela "
                    "busca textual. Considerar transcrição humana."},
           {"basis", json::array({{{"type", "artifacts"}, {"id", a["id"]}}})},
           {"proposed_capability", "human-assessment"},
           {"alternative", "Manter o original sem interpretação e registrar o limite"},
           {"status", "INFERRED_CANDIDATE"},
           {"scope", "Fontes locais catalogadas nesta janela"}});
  }
  if (!artifact.empty()) {
    auto a = sources[0];
    if (a["processing"] == "INDEXED") {
      auto bytes = store.object(a["sha256"]);
      auto question = bytes.find('?');
      if (question != std::string::npos) {
        auto line = bytes.rfind('\n', question);
        size_t start = line == std::string::npos ? 0 : line + 1;
        if (question - start > 1000) {
          start = question - 1000;
          while (start < question && ((unsigned char)bytes[start] & 0xc0) == 0x80)
            ++start;
        }
        while (start < question &&
               (bytes[start] == ' ' || bytes[start] == '\r' || bytes[start] == '\t'))
          ++start;
        auto quote = bytes.substr(start, question + 1 - start);
        candidates.push_back({{"rule", "source.question.review.v1"},
                              {"text", "Examinar a pergunta registrada nesta fonte e confrontá-la "
                                       "com outras passagens antes de propor uma contribuição."},
                              {"quote", quote},
                              {"anchor",
                               {{"artifact_id", a["id"]},
                                {"start", start},
                                {"end", question + 1},
                                {"quote", quote}}},
                              {"basis", json::array({{{"type", "artifacts"}, {"id", a["id"]}}})},
                              {"proposed_capability", "human-assessment"},
                              {"alternative", "A pergunta pode já ter resposta em outra passagem; "
                                              "deixá-la em aberto também é uma decisão possível."},
                              {"status", "INFERRED_CANDIDATE"},
                              {"scope", "Primeira interrogação literal da fonte selecionada; não "
                                        "foi verificada a existência de resposta."}});
      }
    }
  }
  auto notes = inquiry.empty() ? json::array() : store.list("statements", window, inquiry, 500);
  auto assessments =
      inquiry.empty() ? json::array() : store.list("assessments", window, inquiry, 500);
  for (auto &n : notes) {
    if (n.value("text", "").find('?') == std::string::npos)
      continue;
    bool reviewed = false;
    for (auto &a : assessments)
      if (a.contains("target") && a["target"].value("id", "") == n["id"].get<std::string>())
        reviewed = true;
    if (!reviewed)
      candidates.push_back(
          {{"rule", "question.no_assessment.v1"},
           {"text", "Enunciado com interrogação sem avaliação vinculada nesta investigação. "
                    "Considerar um contraponto ou uma fonte adicional."},
           {"basis", json::array({{{"type", "statements"}, {"id", n["id"]}}})},
           {"proposed_capability", "human-assessment"},
           {"alternative", "Deixar em aberto; pode haver resposta fora do recorte"},
           {"status", "INFERRED_CANDIDATE"},
           {"scope", "Até 500 enunciados e avaliações desta investigação nesta janela"}});
  }
  return {{"items", candidates},
          {"status", candidates.empty() ? "NO_GROUNDED_OPPORTUNITY" : "CANDIDATES"},
          {"window", window},
          {"scope",
           {{"artifact_id", artifact.empty() ? json(nullptr) : json(artifact)},
            {"inquiry_id", inquiry.empty() ? json(nullptr) : json(inquiry)},
            {"source_count", sources.size()},
            {"statement_count", notes.size()}}},
          {"rules",
           json::array({"coverage.not_indexed.v1: processing == NOT_INDEXED",
                        "question.no_assessment.v1: '?' no texto e ausência de avaliação vinculada",
                        "source.question.review.v1: primeira interrogação literal da fonte "
                        "selecionada; não verifica resposta"})},
          {"limitation", "Regras literais e desmontáveis; não detectam importância científica, "
                         "respostas ou ausência no mundo. Só procura perguntas na fonte "
                         "selecionada e nos enunciados do percurso aberto. Até 500 fontes/itens."}};
}
json Service::run(json in, int window) {
  auto contribution = store.get("contributions", require_text(in, "contribution_id"), window);
  if (contribution["status"] != "authorized")
    throw Error(403, "AUTHORITY_REQUIRED", "Exige contribuição autorizada");
  auto capability = store.get("capabilities", contribution["capability_id"], window);
  if (capability["local_or_external"] == "external")
    throw Error(422, "CAPABILITY_UNAVAILABLE",
                "Nenhum conector externo implementado; a autorização não executa uma consulta");
  if (capability["id"] != "text-compare")
    throw Error(
        422, "CAPABILITY_UNAVAILABLE",
        "Esta rota executa apenas text-compare; demais capacidades têm suas rotas próprias");
  auto left = store.get("anchors", require_text(in, "left_anchor"), window);
  auto right = store.get("anchors", require_text(in, "right_anchor"), window);
  if (left["status"] != "RESOLVED" || right["status"] != "RESOLVED")
    throw Error(422, "UNRESOLVED_ANCHOR", "Comparação exige dois trechos resolvidos");
  auto scope = json::parse(contribution["authorization_scope"].get<std::string>(), nullptr, false);
  if (scope.is_discarded() || !scope.is_object() ||
      scope.value("left_anchor", "") != in["left_anchor"].get<std::string>() ||
      scope.value("right_anchor", "") != in["right_anchor"].get<std::string>())
    throw Error(403, "AUTHORITY_REQUIRED",
                "Escopo deve indicar exatamente left_anchor e right_anchor em JSON");
  std::string a = left["quote"], b = right["quote"];
  size_t prefix = 0;
  while (prefix < std::min(a.size(), b.size()) && a[prefix] == b[prefix])
    ++prefix;
  while (prefix > 0 && prefix < a.size() && ((unsigned char)a[prefix] & 0xc0) == 0x80)
    --prefix;
  json output = {
      {"equal", a == b},
      {"common_prefix_bytes", prefix},
      {"left", a},
      {"right", b},
      {"left_remainder", a.substr(prefix)},
      {"right_remainder", b.substr(prefix)},
      {"limitation",
       "Comparação literal UTF-8; divergência não prova transformação conceitual ou causalidade"}};
  Transaction tx(store);
  json r = store.save("runs", {{"capability_id", "text-compare"},
                               {"capability_version", "0.1.0"},
                               {"contribution_id", contribution["id"]},
                               {"inquiry_id", contribution["inquiry_id"]},
                               {"input", in},
                               {"output", output},
                               {"status", "performed"},
                               {"window", window},
                               {"created_by", "local_user"}});
  int v = contribution["version"];
  contribution["status"] = "performed";
  contribution["run_id"] = r["id"];
  contribution["performed_at"] = now();
  store.save("contributions", contribution, v);
  store.event(
      contribution["id"], "performed", "local_user",
      {{"run_id", r["id"]}, {"window", window}, {"inquiry_id", contribution["inquiry_id"]}});
  store.event(r["id"], "run", "local_user",
              {{"window", window}, {"inquiry_id", contribution["inquiry_id"]}});
  tx.commit();
  return r;
}
json Service::export_inquiry(const std::string &id, int window) {
  auto inquiry = store.get("inquiries", id, window);
  json bundle = {
      {"format", "NOESE-INQUIRY-1"},
      {"exported_at", now()},
      {"inquiry", inquiry},
      {"window", window},
      {"limits", json::array({"Não é comprovação científica",
                              "Originais codificados como bytes hexadecimais; hashes verificáveis",
                              "Fontes parciais continuam parciais"})}};
  std::set<std::pair<std::string, std::string>> refs;
  refs.emplace("inquiries", id);
  for (auto t : {"statements", "relations", "opportunities", "contributions", "assessments",
                 "inquiry_members", "runs"}) {
    auto items = store.list(t, window, id, 10000);
    if (items.size() == 10000)
      throw Error(422, "INCOMPLETE_SOURCE",
                  "Exportação excede 10.000 registros de um tipo; reduza o percurso");
    for (auto &item : items)
      refs.emplace(t, item["id"].get<std::string>());
  }
  auto collect = [&](const auto &self, const json &j) -> void {
    if (j.is_object()) {
      if (j.contains("type") && j.contains("id") && j["type"].is_string() && j["id"].is_string())
        refs.emplace(j["type"].get<std::string>(), j["id"].get<std::string>());
      for (auto key : {"artifact_id", "left_anchor", "right_anchor", "opportunity_id",
                       "contribution_id", "capability_id"})
        if (j.contains(key) && j[key].is_string())
          refs.emplace(std::string(key) == "artifact_id"      ? "artifacts"
                       : std::string(key).ends_with("anchor") ? "anchors"
                       : std::string(key) == "opportunity_id" ? "opportunities"
                       : std::string(key) == "capability_id"  ? "capabilities"
                                                              : "contributions",
                       j[key].template get<std::string>());
      for (auto &[k, v] : j.items())
        self(self, v);
    } else if (j.is_array())
      for (auto &v : j)
        self(self, v);
  };
  std::set<std::pair<std::string, std::string>> visited;
  json entities = json::object();
  while (visited.size() < refs.size()) {
    auto copy = refs;
    for (auto &ref : copy)
      if (visited.insert(ref).second) {
        auto record = store.get(ref.first, ref.second, window);
        auto h = history(ref.first, ref.second, window);
        entities[ref.first].push_back({{"record", record}, {"history", h}});
        collect(collect, h["versions"]);
        if (ref.first == "artifacts") {
          for (auto &occurrence : store.query(
                   "SELECT id FROM occurrences WHERE json_extract(payload,'$.artifact_id')=?",
                   {ref.second}))
            refs.emplace("occurrences", occurrence["id"].get<std::string>());
        }
      }
  }
  json originals = json::array();
  for (auto &ref : visited)
    if (ref.first == "artifacts") {
      auto a = store.get("artifacts", ref.second, window);
      auto bytes = store.object(a["sha256"]);
      std::string hex;
      const char *digits = "0123456789abcdef";
      hex.reserve(bytes.size() * 2);
      for (unsigned char c : bytes) {
        hex += digits[c >> 4];
        hex += digits[c & 15];
      }
      originals.push_back(
          {{"artifact_id", a["id"]}, {"sha256", a["sha256"]}, {"encoding", "hex"}, {"bytes", hex}});
    }
  bundle["entities"] = entities;
  bundle["originals"] = originals;
  bundle["manifest"] = {{"originals", originals.size()},
                        {"entity_groups", entities.size()},
                        {"payload_sha256", sha256(entities.dump() + originals.dump())}};
  return bundle;
}
json Service::import_lab(int window) {
  auto root = fs::path(NOESE_ROOT) / "fixtures/lab001";
  auto path = root / "NOESE-LAB-001-corpus.jsonl";
  if (!fs::exists(path))
    throw Error(422, "INCOMPLETE_SOURCE",
                "Corpus privado não disponível; importe os arquivos compartilhados localmente");
  auto bytes = read_file(path);
  auto manifest = json::parse(read_file(root / "NOESE-LAB-001-manifest.json"));
  if (sha256(bytes) != manifest["sha256"].get<std::string>())
    throw Error(422, "INCOMPLETE_SOURCE", "Hash do corpus diverge do manifesto");
  json imported = json::array();
  std::istringstream lines(bytes);
  std::string line;
  while (std::getline(lines, line)) {
    auto entry = json::parse(line);
    int order = entry["curated_order"];
    int w = order <= 10 ? 0 : order <= 15 ? 1 : 2;
    if (w > window)
      continue;
    std::string origin = "LAB001:" + entry["id"].get<std::string>();
    auto found =
        store.query("SELECT id FROM artifacts WHERE json_extract(payload,'$.origin')=?", {origin});
    if (!found.empty()) {
      imported.push_back(store.get("artifacts", found[0]["id"], window));
      continue;
    }
    json meta = entry;
    meta["origin"] = origin;
    meta["coverage"] = "PARTIAL_EXCERPTS";
    meta["attributed_author"] = entry["role"];
    meta["author_certainty"] = "attributed_by_curator";
    meta["parent_corpus_sha256"] = manifest["sha256"];
    imported.push_back(
        import_file(entry["id"].get<std::string>() + ".txt", entry["text"], meta, w));
  }
  return {{"artifacts", imported},
          {"coverage", "PARTIAL_EXCERPTS"},
          {"window", window},
          {"limitation",
           "21 excertos curados; sem timestamps individuais; nenhum README externo foi importado"}};
}
json Service::purge(const std::string &id, const std::string &confirm) {
  auto artifact = store.get("artifacts", id);
  if (confirm != artifact["sha256"].get<std::string>())
    throw Error(400, "INVALID_INPUT",
                "Confirme SHA-256 da fonte para excluir e remover registros dependentes");
  std::set<std::string> removed = {id};
  const std::vector<std::string> types = {"anchors",     "occurrences",     "statements",
                                          "relations",   "opportunities",   "contributions",
                                          "assessments", "inquiry_members", "runs"};
  std::set<std::pair<std::string, std::string>> rows;
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto &t : types)
      for (auto &row : store.query("SELECT id,payload FROM " + t)) {
        std::string payload = row["payload"];
        for (auto &target : removed)
          if (payload.find(target) != std::string::npos) {
            std::string rid = row["id"];
            rows.insert({t, rid});
            if (removed.insert(rid).second)
              changed = true;
            break;
          }
      }
  }
  Transaction tx(store);
  for (auto &[t, rid] : rows)
    store.change("DELETE FROM " + t + " WHERE id=?", {rid});
  for (auto &rid : removed) {
    store.change("UPDATE operation_requests SET result='{\"erased\":true}',revoked=1 WHERE "
                 "instr(result,?)>0",
                 {rid});
  }
  store.change("DELETE FROM artifacts WHERE id=?", {id});
  store.change("DELETE FROM artifact_fts WHERE id=?", {id});
  for (auto &rid : removed)
    store.change("DELETE FROM events WHERE object_id=?", {rid});
  store.event(uuid(), "sensitive_data_erasure", "local_user",
              {{"removed_records", removed.size()}, {"content_retained", false}});
  tx.commit();
  auto others = store.query("SELECT id FROM artifacts WHERE json_extract(payload,'$.sha256')=?",
                            {artifact["sha256"]});
  if (others.empty())
    fs::remove(store.root / "objects" / artifact["sha256"].get<std::string>());
  store.exec("INSERT INTO artifact_fts(artifact_fts) VALUES('rebuild'); PRAGMA "
             "wal_checkpoint(TRUNCATE); VACUUM;");
  return {{"removed_records", removed.size()},
          {"limitation", "Cópias exportadas e backups anteriores exigem exclusão separada pelo "
                         "responsável; sem garantia de apagamento físico em SSD"}};
}
