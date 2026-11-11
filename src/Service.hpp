#pragma once
#include "Store.hpp"
struct Service {
  Store store;
  explicit Service(fs::path root);
  json import_file(const std::string &name, const std::string &bytes, json meta, int window);
  json anchor(json input, int window);
  json record(const std::string &type, json input, int window);
  json search(const std::string &text, int window, int limit, int offset);
  json history(const std::string &type, const std::string &id, int window);
  json explore(const std::string &inquiry, int window, const std::string &artifact = "");
  json run(json input, int window);
  json export_inquiry(const std::string &id, int window);
  json import_lab(int window);
  json purge(const std::string &id, const std::string &confirm);
};
void serve(Service &service, int port);
