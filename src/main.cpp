#include "Investigator.hpp"
#include "Operations.hpp"
#include <iostream>
#include <sys/stat.h>
int main(int argc, char **argv) {
  umask(0077);
  try {
    fs::path data = "var";
    int port = 8787;
    bool mcp = false, investigate = false, allow_writes = false, port_set = false;
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
      else if (a == "--data" && i + 1 < argc)
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
            << "noese [--data var] [--port 8787]\nnoese restore BACKUP NEW_DIRECTORY\nnoese "
               "mcp [--data var] [--allow-writes]\nnoese investigate --data var [--window 2]\n";
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
    if (mcp && port_set)
      throw std::runtime_error("mcp uses stdio, not --port");
    Service service(data);
    if (investigate) {
      Investigator investigator(service);
      std::cout << investigator.once(window).dump() << '\n';
    } else if (mcp)
      serve_mcp(service, allow_writes);
    else
      serve(service, port);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "NOESE: " << e.what() << '\n';
    return 1;
  }
}
