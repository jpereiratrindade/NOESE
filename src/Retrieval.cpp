#include "Service.hpp"
#include <set>
#include <sstream>

json Service::artifacts(int window, int limit, int offset, const std::string &scope) {
  std::lock_guard lock(store.mutex);
  if (scope != "investigative" && scope != "technical" && scope != "all")
    throw Error(400, "INVALID_INPUT", "Escopo de recuperação desconhecido");
  const std::string role = "COALESCE(json_extract(a.payload,'$.investigative_role'),'source')";
  auto filter = scope == "all" ? "1" : role + (scope == "technical" ? "=" : "<>") + "'instrumental_event'";
  const std::string selection =
      " FROM artifacts a WHERE CAST(json_extract(a.payload,'$.window') AS INTEGER)<=? "
      "AND a.version=(SELECT MAX(v.version) FROM artifacts v WHERE v.id=a.id AND "
      "CAST(json_extract(v.payload,'$.window') AS INTEGER)<=?) AND " + filter;
  auto rows = store.query("SELECT a.id" + selection + " ORDER BY a.rowid DESC LIMIT ? OFFSET ?",
      {std::to_string(window), std::to_string(window), std::to_string(limit), std::to_string(offset)});
  json items = json::array();
  for (auto &row : rows) {
    auto artifact = store.get("artifacts", row["id"], window);
    if (!artifact.contains("investigative_role")) {
      artifact["investigative_role"] = "source";
      artifact["role_basis"] = "not_classified";
    }
    items.push_back(artifact);
  }
  auto total = store.query("SELECT count(*) AS n" + selection,
                          {std::to_string(window), std::to_string(window)});
  return {{"items", items}, {"retrieval_scope", scope}, {"total", std::stoi(total[0]["n"].get<std::string>())},
          {"next_offset", items.size() == size_t(limit) ? json(offset + limit) : json(nullptr)},
          {"window", window}, {"limit", limit}, {"offset", offset}};
}

json Service::classify_artifact(const std::string &id, const std::string &role,
                                const std::string &reason, int window, int version,
                                const std::string &actor) {
  std::lock_guard lock(store.mutex);
  const std::set<std::string> roles = {"source", "primary_source", "secondary_report", "instrumental_event", "interpretation", "critique"};
  if (!roles.contains(role) || reason.empty() || reason.size() > 10000)
    throw Error(400, "INVALID_INPUT", "Declare papel e razão da classificação");
  auto artifact = store.get("artifacts", id, window);
  artifact["investigative_role"] = role;
  artifact["role_basis"] = reason;
  artifact["window"] = window;
  Transaction tx(store);
  artifact = store.save("artifacts", artifact, version);
  store.event(id, "classify_role", actor, {{"role", role}, {"reason", reason},
      {"version", artifact["version"]}, {"window", window}, {"scientific_validation", "NOT_ESTABLISHED"}});
  tx.commit();
  return artifact;
}

void Service::classify_experience_sources() {
  // Reconstruct roles from the preserved capture and exact linked bytes, never
  // from filenames or a word found in a document. Prior versions stay available.
  for (int offset = 0; ; offset += 500) {
    auto inquiries = store.list("inquiries", 2, "", 500, offset);
    for (const auto &inquiry : inquiries) {
      if (!inquiry.contains("experience")) continue;
      const auto &meta = inquiry["experience"];
      if (!meta.contains("artifact_id")) continue;
      json source;
      try { source = store.get("artifacts", meta["artifact_id"], 2); }
      catch (const Error &e) { if (e.http == 404) continue; throw; }
      auto raw = store.object(source["sha256"]);
      auto desired = meta.value("capture", "") == "trace" ? "instrumental_event" : "secondary_report";
      if (!source.contains("investigative_role"))
        classify_artifact(source["id"], desired, "derived_from_preserved_experience_capture", 2,
                          source["version"], "noese:provenance-recovery");
      if (meta.value("capture", "") != "trace") continue;
      std::set<std::string> hashes;
      std::istringstream stream(raw);
      std::string line;
      while (std::getline(stream, line)) {
        if (line.empty()) continue;
        for (size_t start = 0; start < line.size();) {
          size_t end = std::min(start + 80000, line.size());
          while (end < line.size() && ((unsigned char)line[end] & 0xc0) == 0x80) --end;
          hashes.insert(sha256(line.substr(start, end - start)));
          start = end;
        }
      }
      for (const auto &note : store.list("statements", 2, inquiry["id"], 10000))
        for (const auto &ref : note.value("basis", json::array())) {
          if (ref.value("type", "") != "anchors") continue;
          auto anchor = store.get("anchors", ref["id"], 2);
          auto artifact = store.get("artifacts", anchor["artifact_id"], 2);
          if (artifact.contains("investigative_role") ||
              !hashes.contains(artifact["sha256"].get<std::string>())) continue;
          classify_artifact(artifact["id"], "instrumental_event", "linked_anchor_and_exact_event_hash_in_preserved_trace",
                            2, artifact["version"], "noese:provenance-recovery");
        }
    }
    if (inquiries.size() < 500) break;
  }
}
