#include "Service.hpp"
#include <algorithm>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace {
void require(bool condition, const std::string &message) {
  if (!condition)
    throw Error(422, "INVALID_EXPERIENCE", message);
}
json inspect_trace(const std::string &raw, const std::string &run_id,
                   const std::string &memory_policy) {
  json events = json::array(), focus = nullptr, memory = json::array(), draft = nullptr;
  std::map<std::string, json> requests;
  std::set<std::string> responses, purposes;
  int mcp_responses = 0, model_requests = 0, model_responses = 0;
  bool investigated = false;
  std::istringstream stream(raw);
  std::string line;
  while (std::getline(stream, line)) {
    if (line.empty())
      continue;
    require(events.size() < 200, "Percurso excede 200 eventos");
    auto event = json::parse(line);
    std::ostringstream expected;
    expected << 'e' << std::setw(4) << std::setfill('0') << events.size() + 1;
    require(event.is_object() && event.value("id", "") == expected.str() &&
                event.contains("payload") && event["payload"].is_object() &&
                event.contains("at") && event["at"].is_string(),
            "Evento sem identidade, ordem ou data de registro válida");
    auto kind = event.value("kind", "");
    const auto &payload = event["payload"];
    if (kind == "mcp_request" || kind == "mcp_response") {
      auto wire = payload.at("wire").get<std::string>();
      require(sha256(wire) == payload.at("sha256").get<std::string>(), "Hash de mensagem MCP divergente");
      auto rpc = json::parse(wire);
      auto server = payload.at("server").get<std::string>();
      if (kind == "mcp_request" && rpc.contains("id")) {
        requests[server + ":" + rpc["id"].dump()] = rpc;
        if (server == "noese" && rpc.value("method", "") == "tools/call") {
          auto tool = rpc.at("params").at("name").get<std::string>();
          if (tool != "capabilities") {
            require(memory_policy != "none", "Memória declarada excluída, mas houve consulta ao acervo");
            memory.push_back({{"request_event", event["id"]}, {"tool", tool},
                              {"arguments", rpc["params"].value("arguments", json::object())}});
          }
        }
      }
      if (kind == "mcp_response") {
        auto key = server + ":" + rpc.at("id").dump();
        require(requests.contains(key), "Resposta MCP sem requisição anterior");
        const auto request = requests[key];
        if (request.value("method", "") == "tools/call" && rpc.contains("result") &&
            !rpc["result"].value("isError", false))
          responses.insert(event["id"].get<std::string>());
        requests.erase(key);
        ++mcp_responses;
      }
    }
    if (kind == "model_request")
      ++model_requests;
    if (kind == "model_response" || kind == "model_http_error")
      ++model_responses;
    if (kind == "model_response" && payload.contains("response") &&
        payload["response"].contains("message") && payload["response"]["message"].contains("content")) {
      auto content = payload["response"]["message"]["content"];
      if (content.is_string()) {
        auto parsed = json::parse(content.get<std::string>(), nullptr, false);
        if (parsed.is_object() && parsed.contains("outcome") && parsed.contains("summary"))
          draft = {{"response_event", event["id"]}, {"output", parsed}, {"validation", "NOT_CERTIFIED_BY_CAPTURE"}};
      }
    }
    if (kind == "initial_focus") {
      require(focus.is_null() && !investigated, "Foco inicial duplicado ou registrado após a investigação");
      require(payload.contains("question") && payload["question"].is_string() &&
                  !payload["question"].get<std::string>().empty() &&
                  payload.contains("reason") && payload["reason"].is_string() &&
                  !payload["reason"].get<std::string>().empty(), "Foco sem questão e justificativa");
      require(payload.contains("response_ids") && payload["response_ids"].is_array() &&
                  !payload["response_ids"].empty(), "Foco sem proveniência anterior");
      for (const auto &ref : payload["response_ids"])
        require(ref.is_string() && responses.contains(ref.get<std::string>()),
                "Foco referencia resposta inexistente ou posterior");
      focus = payload;
    }
    if (kind == "instrument_decision") {
      auto purpose = payload.value("purpose", "");
      if (purpose == "support" || purpose == "alternative") {
        require(!focus.is_null(), "Consulta investigativa anterior ao foco inicial");
        investigated = true;
        purposes.insert(purpose);
      }
    }
    events.push_back(event);
  }
  require(events.size() >= 2 && events.front().value("kind", "") == "mission" &&
              events.back().value("kind", "") == "outcome", "Percurso sem missão e desfecho");
  require(events.front()["payload"].value("run_id", "") == run_id, "Identidade da execução divergente");
  const auto &config = events.front()["payload"].value("config", json::object());
  require(config.value("memory_policy", "not_recorded") == memory_policy,
          "Política de memória diverge da missão preservada");
  const auto &outcome = events.back()["payload"];
  if (outcome.contains("initial_focus"))
    require(outcome["initial_focus"] == focus, "Síntese altera o foco inicial preservado");
  auto status = outcome.value("status", "NOT_RECORDED");
  if (status == "CANDIDATE" || status == "ABSTENTION")
    require(!focus.is_null() && purposes.contains("support") && purposes.contains("alternative"),
            "Desfecho investigativo sem foco, apoio e consulta alternativa");
  return {{"events", events}, {"initial_focus", focus}, {"outcome", outcome}, {"draft", draft},
          {"status", status}, {"event_count", events.size()}, {"mcp_response_count", mcp_responses},
          {"memory_queries", memory}, {"unanswered_mcp_requests", requests.size()},
          {"unanswered_model_requests", std::max(0, model_requests - model_responses)},
          {"focus_provenance", focus.is_null() ? "NOT_RECORDED" : "RECORDED_BEFORE_INVESTIGATION"}};
}
} // namespace

json Service::preserve_experience(const json &input, int window, const std::string &actor) {
  const bool trace = input.at("capture") == "trace";
  const std::string run_id = input.at("run_id"), policy = input.at("memory_policy");
  const std::string content = input.value(trace ? "journal" : "report", "");
  require(!content.empty() && content.size() <= 650000, "Captura vazia ou acima de 650.000 bytes");
  require(!input.contains(trace ? "report" : "journal"), "Não misture relatório secundário com captura instrumental");
  auto inspected = trace ? inspect_trace(content, run_id, policy) : json::object();
  auto existing = store.query("SELECT id FROM inquiries WHERE json_extract(payload,'$.experience.run_id')=?", {run_id});
  require(existing.empty(), "Execução já preservada; reutilize request_id para repetição idempotente");
  auto inquiry = input.contains("inquiry_id") ? store.get("inquiries", input["inquiry_id"], window)
      : json{{"title", input.at("title")}, {"status", "UNRESOLVED"}, {"window", window},
             {"created_by", actor}, {"source_type", "mcp_client"}, {"source_ref", actor}};
  require(!inquiry.contains("experience"), "Percurso já possui captura; preserve uma nova experiência");
  Transaction tx(store);
  auto source = import_file(trace ? "experience-trace.jsonl" : "experience-report.md", content,
      {{"origin", actor}, {"imported_by", actor},
       {"source_type", trace ? "instrumental_trace" : "secondary_report"},
       {"coverage", trace ? "RECORDED_EVENTS_NOT_AUTHENTICATED" : "SECONDARY_REPORT_NO_RAW_RESPONSES"}}, window);
  auto anchored = anchor({{"artifact_id", source["id"]}, {"start", 0}, {"end", content.size()},
                         {"created_by", actor}}, window);
  json meta = {{"format", "NOESE-EXPERIENCE-1"}, {"run_id", run_id}, {"capture", input["capture"]},
               {"producer_declared", input["producer"]}, {"purpose", input["purpose"]},
               {"memory_policy", policy}, {"artifact_id", source["id"]}, {"sha256", source["sha256"]},
               {"limitations", input["limitations"]}, {"human_evaluation", "PENDING"},
               {"source_authentication", "NOT_ESTABLISHED"},
               {"initial_focus", trace ? inspected["initial_focus"] : json(nullptr)},
               {"focus_provenance", trace ? inspected["focus_provenance"] : json("RETROSPECTIVE_OR_UNKNOWN")},
               {"status", trace ? inspected["status"] : json("SECONDARY_REPORT")},
               {"event_count", trace ? inspected["event_count"] : json(0)},
               {"memory_queries", trace ? inspected["memory_queries"] : json::array()},
               {"unanswered_mcp_requests", trace ? inspected["unanswered_mcp_requests"] : json(nullptr)},
               {"unanswered_model_requests", trace ? inspected["unanswered_model_requests"] : json(nullptr)}};
  inquiry["experience"] = meta;
  inquiry["window"] = window;
  int version = inquiry.value("version", 0);
  inquiry = store.save("inquiries", inquiry, version);
  store.event(inquiry["id"], "experience_capture", actor,
              {{"capture", input["capture"]}, {"artifact_id", source["id"]},
               {"version", inquiry["version"]}, {"scope", "byte_integrity_and_recorded_order_only"}});
  record("statements", {{"inquiry_id", inquiry["id"]}, {"created_by", actor},
      {"text", "Experiência preservada: " + input.at("title").get<std::string>() +
               ". Pertinência, alcance e relevância continuam sujeitos à avaliação humana."},
      {"status", "INFERRED_CANDIDATE"}, {"basis", json::array({{{"type", "anchors"}, {"id", anchored["id"]}}})}}, window);
  tx.commit();
  return {{"inquiry_id", inquiry["id"]}, {"artifact_id", source["id"]}, {"experience", meta}};
}

json Service::experiences(int window, int limit, int offset) {
  auto rows = store.list("inquiries", window, "", limit, offset);
  json items = json::array();
  for (const auto &row : rows)
    if (row.contains("experience"))
      items.push_back({{"id", row["id"]}, {"title", row["title"]}, {"window", row["window"]},
                       {"experience", row["experience"]}});
  return {{"items", items}, {"next_offset", rows.size() == size_t(limit) ? json(offset + limit) : json(nullptr)},
          {"window", window}};
}

json Service::experience(const std::string &id, int window, bool include_events) {
  auto inquiry = store.get("inquiries", id, window);
  if (!inquiry.contains("experience"))
    throw Error(404, "NOT_FOUND", "Percurso sem captura de experiência");
  auto meta = inquiry["experience"];
  auto source = store.get("artifacts", meta.at("artifact_id"), window);
  auto raw = store.object(source["sha256"]);
  require(sha256(raw) == meta["sha256"].get<std::string>(), "Captura preservada com hash divergente");
  json result = {{"inquiry", inquiry}, {"experience", meta}, {"integrity", "VERIFIED"},
                 {"assessments", store.list("assessments", window, id, 500)},
                 {"limitation", "Integridade dos bytes e ordem registrada não autenticam autoria nem sustentação científica"}};
  if (meta["capture"] == "trace") {
    auto trace = inspect_trace(raw, meta.at("run_id"), meta.at("memory_policy"));
    if (!include_events)
      trace.erase("events");
    trace["events_included"] = include_events;
    result["trace"] = trace;
  } else
    result["report"] = raw;
  return result;
}
