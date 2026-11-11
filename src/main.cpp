#include "Investigator.hpp"
#include "Operations.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sys/stat.h>
namespace {
bool env_truthy(const char *name) {
  const char *raw = std::getenv(name);
  if (!raw)
    return false;
  std::string v(raw);
  for (auto &c : v)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return v == "1" || v == "true" || v == "yes";
}
} // namespace
int main(int argc, char **argv) {
  umask(0077);
  try {
    fs::path data = "var";
    int port = 8787;
    bool mcp = false, investigate = false, allow_writes = false, port_set = false;
    bool mcp_allow_writes = false, mcp_token_set = false;
    std::string mcp_token;
    int window = 2;
    if (argc == 4 && std::string(argv[1]) == "restore") {
      Store::restore(argv[2], argv[3]);
      std::cout << "RESTORED " << argv[3] << '\n';
      return 0;
    }
    for (int i = 1; i < argc; ++i) {
      std::string a = argv[i];
      if (a == "investigate")
        investigate = true;
      else if (a == "--window" && i + 1 < argc) {
        std::string value = argv[++i];
        if (value != "0" && value != "1" && value != "2")
          throw std::runtime_error("Invalid window");
        window = std::stoi(value);
      } else if (a == "mcp")
        mcp = true;
      else if (a == "--allow-writes")
        allow_writes = true;
      else if (a == "--mcp-allow-writes")
        mcp_allow_writes = true;
      else if (a == "--mcp-token" && i + 1 < argc) {
        mcp_token_set = true;
        mcp_token = argv[++i];
      } else if (a == "--data" && i + 1 < argc)
        data = argv[++i];
      else if (a == "--port" && i + 1 < argc) {
        port_set = true;
        std::string p = argv[++i];
        if (p.empty() || p.find_first_not_of("0123456789") != std::string::npos)
          throw std::runtime_error("Invalid port");
        port = std::stoi(p);
        if (port < 1024 || port > 65535)
          throw std::runtime_error("Port must be 1024..65535");
      } else if (a == "--help") {
        std::cout
            << "noese [--data var] [--port 8787] [--mcp-allow-writes] [--mcp-token TOKEN]\n"
               "noese restore BACKUP NEW_DIRECTORY\n"
               "noese mcp [--data var] [--allow-writes]\n"
               "noese investigate --data var [--window 2]\n";
        return 0;
      } else
        throw std::runtime_error("Unknown argument: " + a);
    }
    if (mcp && investigate)
      throw std::runtime_error("Choose one transport");
    if (investigate && port_set)
      throw std::runtime_error("investigate does not start HTTP");
    if (allow_writes && !mcp)
      throw std::runtime_error("--allow-writes requires mcp mode");
    if ((mcp_allow_writes || mcp_token_set) && (mcp || investigate))
      throw std::runtime_error("--mcp-allow-writes/--mcp-token are HTTP server options");
    if (mcp && port_set)
      throw std::runtime_error("mcp uses stdio, not --port");
    Service service(data);
    if (investigate) {
      Investigator investigator(service);
      std::cout << investigator.once(window).dump() << '\n';
    } else if (mcp)
      serve_mcp(service, allow_writes);
    else {
      ServerOptions options;
      options.port = port;
      options.mcp_allow_writes = mcp_allow_writes || env_truthy("NOESE_MCP_ALLOW_WRITES");
      if (mcp_token_set)
        options.mcp_token = mcp_token;
      else if (const char *env_token = std::getenv("NOESE_MCP_TOKEN"))
        options.mcp_token = env_token;
      serve(service, options);
    }
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "NOESE: " << e.what() << '\n';
    return 1;
  }
}
