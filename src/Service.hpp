#pragma once
#include "Store.hpp"
struct Service {
  Store store;
  explicit Service(fs::path root);
  json import_file(const std::string &name, const std::string &bytes, json meta, int window);
  json anchor(json input, int window);
  json record(const std::string &type, json input, int window);
  json search(const std::string &text, int window, int limit, int offset,
              const std::string &scope = "investigative");
  json artifacts(int window, int limit, int offset, const std::string &scope = "investigative");
  json classify_artifact(const std::string &id, const std::string &role, const std::string &reason,
                         int window, int version, const std::string &actor);
  void classify_experience_sources();
  json history(const std::string &type, const std::string &id, int window);
  json explore(const std::string &inquiry, int window, const std::string &artifact = "");
  json run(json input, int window);
  json export_inquiry(const std::string &id, int window);
  json preserve_experience(const json &input, int window, const std::string &actor);
  json experiences(int window, int limit = 100, int offset = 0);
  json experience(const std::string &id, int window, bool include_events = true);
  json import_lab(int window);
  json purge(const std::string &id, const std::string &confirm);
};
struct ServerOptions {
  int port = 8787;
  bool mcp_allow_writes = false;
  std::string mcp_token;
};
void serve(Service &service, const ServerOptions &options);
