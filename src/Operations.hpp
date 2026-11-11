#pragma once
#include "Service.hpp"
struct ClientContext {
  bool allow_writes = false;
  std::string name = "unknown";
};
// Both transports use Service; this catalogue defines the MCP contracts explicitly.
json operation_catalog(bool allow_writes);
json execute_operation(Service &service, const std::string &name, const json &arguments,
                       const ClientContext &client);
void serve_mcp(Service &service, bool allow_writes);
