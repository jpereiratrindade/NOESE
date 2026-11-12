#include "Investigator.hpp"
#include "Operations.hpp"
#include <algorithm>
#include <cctype>
#include <httplib.h>
#include <iostream>
#include <openssl/crypto.h>
namespace {
int integer(const httplib::Request &req, const std::string &key, int def, int max) {
  if (!req.has_param(key))
    return def;
  auto s = req.get_param_value(key);
  if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos || s.size() > 7)
    throw Error(400, "INVALID_INPUT", "Parâmetro inválido: " + key);
  int n = std::stoi(s);
  if (n > max)
    throw Error(400, "INVALID_INPUT", "Parâmetro fora do limite: " + key);
  return n;
}
json body(const httplib::Request &req) {
  auto j = json::parse(req.body);
  if (!j.is_object())
    throw Error(400, "INVALID_INPUT", "Esperado objeto JSON");
  return j;
}
void send(httplib::Response &res, const json &j, int status = 200) {
  res.status = status;
  res.set_content(j.dump(), "application/json; charset=utf-8");
}
bool is_mcp_path(const std::string &path) {
  return path == "/mcp" || (path.rfind("/mcp/", 0) == 0 && path.find('/', 5) == std::string::npos);
}
bool constant_time_equals(const std::string &a, const std::string &b) {
  return !a.empty() && a.size() == b.size() && CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}
std::string media_type(std::string value) {
  auto semi = value.find(';');
  if (semi != std::string::npos)
    value.resize(semi);
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
  value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
  for (auto &c : value)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}
bool check_mcp_auth(const httplib::Request &req, httplib::Response &res,
                    const ServerOptions &options, const std::string &origin) {
  if (options.mcp_token.empty())
    return true;
  std::string path_token;
  if (req.path.size() > 5 && req.path.rfind("/mcp/", 0) == 0) {
    path_token = req.path.substr(5);
    if (path_token.find('/') != std::string::npos)
      path_token.clear();
  }
  std::string auth = req.get_header_value("Authorization");
  std::string bearer;
  if (auth.size() > 7) {
    std::string scheme = auth.substr(0, 7);
    for (auto &c : scheme)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (scheme == "bearer ")
      bearer = auth.substr(7);
  }
  bool bearer_ok = constant_time_equals(bearer, options.mcp_token);
  bool path_ok = constant_time_equals(path_token, options.mcp_token);
  if (!bearer_ok && !path_ok) {
    res.set_header("WWW-Authenticate", "Bearer");
    send(res, {{"code", "AUTHORITY_REQUIRED"}, {"message", "Token MCP inválido ou ausente"}}, 401);
    return false;
  }
  auto request_origin = req.get_header_value("Origin");
  if (req.get_header_value("Sec-Fetch-Site") == "cross-site" ||
      (!request_origin.empty() && request_origin != origin && !path_ok)) {
    send(res,
         {{"code", "AUTHORITY_REQUIRED"}, {"message", "Origem não local recusada no endpoint MCP"}},
         403);
    return false;
  }
  return true;
}
} // namespace
void serve(Service &svc, const ServerOptions &options) {
  httplib::Server server;
  Investigator investigator(svc);
  McpHttpDispatcher mcp_dispatcher(svc, options.mcp_allow_writes);
  const std::string token = uuid() + uuid();
  const std::string host = "127.0.0.1:" + std::to_string(options.port), origin = "http://" + host;
  server.new_task_queue = [] { return new httplib::ThreadPool(4); };
  server.set_payload_max_length(17 * 1024 * 1024);
  server.set_read_timeout(15);
  server.set_write_timeout(30);
  server.set_keep_alive_max_count(20);
  server.set_pre_routing_handler([&](const httplib::Request &req, httplib::Response &res) {
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("X-Frame-Options", "DENY");
    res.set_header("Referrer-Policy", "no-referrer");
    res.set_header("Cache-Control", "no-store");
    res.set_header(
        "Content-Security-Policy",
        "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' blob:; "
        "connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
    bool mcp_route = is_mcp_path(req.path);
    if (mcp_route && !options.mcp_token.empty())
      return httplib::Server::HandlerResponse::Unhandled;
    if (req.get_header_value("Host") != host) {
      send(res, {{"code", "INVALID_INPUT"}, {"message", "Host local inválido; use " + origin}},
           403);
      return httplib::Server::HandlerResponse::Handled;
    }
    auto request_origin = req.get_header_value("Origin");
    const bool page_navigation =
        (req.method == "GET" || req.method == "HEAD") &&
        (req.path == "/" || req.path == "/mesa" || req.path == "/experiencias") &&
        req.get_header_value("Sec-Fetch-Mode") == "navigate" &&
        req.get_header_value("Sec-Fetch-Dest") == "document";
    if ((!request_origin.empty() && request_origin != origin) ||
        (req.get_header_value("Sec-Fetch-Site") == "cross-site" && !page_navigation)) {
      send(res, {{"code", "AUTHORITY_REQUIRED"}, {"message", "Origem externa recusada"}}, 403);
      return httplib::Server::HandlerResponse::Handled;
    }
    if (!mcp_route && req.method != "GET" && req.method != "HEAD" &&
        req.get_header_value("X-NOESE-Token") != token) {
      send(res, {{"code", "AUTHORITY_REQUIRED"}, {"message", "Sessão local necessária"}}, 403);
      return httplib::Server::HandlerResponse::Handled;
    }
    return httplib::Server::HandlerResponse::Unhandled;
  });
  server.set_exception_handler([](const auto &, auto &res, std::exception_ptr ep) {
    try {
      if (ep)
        std::rethrow_exception(ep);
    } catch (const Error &e) {
      send(res, {{"code", e.code}, {"message", e.what()}}, e.http);
    } catch (const json::exception &e) {
      std::cerr << "JSON contract: " << e.what() << '\n';
      send(res, {{"code", "INVALID_INPUT"}, {"message", "JSON ou contrato de campo inválido"}},
           400);
    } catch (const std::exception &e) {
      std::cerr << "request error: " << e.what() << '\n';
      send(res, {{"code", "INTERNAL_ERROR"}, {"message", "Falha interna; consulte o log local"}},
           500);
    }
  });
  server.set_error_handler([](const auto &, auto &res) {
    if (res.body.empty())
      send(res,
           {{"code", res.status == 404 ? "NOT_FOUND" : "INVALID_INPUT"},
            {"message", "Requisição recusada"}},
           res.status);
  });
  for (auto file :
       {"index.html", "workspace.html", "app.js", "style.css", "attention.js", "attention.css",
        "identity.css", "identity.js", "noese-symbol.svg", "favicon.svg",
        "experiences.html", "experiences.js", "experiences.css"}) {
    std::string route = std::string(file) == "index.html"       ? "/"
                        : std::string(file) == "workspace.html" ? "/mesa"
                        : std::string(file) == "experiences.html" ? "/experiencias"
                                                                : "/" + std::string(file);
    server.Get(route, [file](const auto &, auto &res) {
      res.set_content(read_file(fs::path(NOESE_ROOT) / "web" / file),
                      std::string(file).ends_with(".js")    ? "application/javascript"
                      : std::string(file).ends_with(".css") ? "text/css"
                      : std::string(file).ends_with(".svg") ? "image/svg+xml"
                                                            : "text/html; charset=utf-8");
    });
  }
  server.Get("/api/v1/attention", [&](const auto &req, auto &res) {
    send(res, investigator.view(integer(req, "window", 2, 2)));
  });
  server.Get("/api/v1/attention/settings",
             [&](const auto &, auto &res) { send(res, investigator.settings()); });
  server.Post("/api/v1/attention/settings",
              [&](const auto &req, auto &res) { send(res, investigator.configure(body(req))); });
  server.Post(R"(/api/v1/attention/([a-f0-9]{32})/critique)", [&](const auto &req, auto &res) {
    send(res, investigator.feedback(req.matches[1], body(req), integer(req, "window", 2, 2)), 201);
  });
  server.Get("/api/v1/tools", [&](const auto &, auto &res) {
    send(res, {{"tools", operation_catalog(true)},
               {"transport", "MCP stdio + Streamable HTTP"},
               {"default_mode", "read_only"},
               {"http_endpoint", "/mcp"},
               {"http_writes", options.mcp_allow_writes},
               {"http_auth", !options.mcp_token.empty()}});
  });
  const std::string mcp_pattern = R"(/mcp(?:/[^/]+)?)";
  server.Options(mcp_pattern, [&](const auto &, auto &res) {
    res.set_header("Allow", "POST, OPTIONS");
    res.status = 204;
    res.set_content("", "text/plain; charset=utf-8");
  });
  server.Get(mcp_pattern, [&](const auto &, auto &res) {
    res.set_header("Allow", "POST, OPTIONS");
    send(res,
         {{"code", "METHOD_NOT_ALLOWED"},
          {"message", "Use POST /mcp para JSON-RPC MCP Streamable HTTP"}},
         405);
  });
  server.Delete(mcp_pattern, [&](const auto &req, auto &res) {
    if (!check_mcp_auth(req, res, options, origin))
      return;
    res.status = 204;
    res.set_content("", "text/plain; charset=utf-8");
  });
  server.Post(mcp_pattern, [&](const auto &req, auto &res) {
    if (!check_mcp_auth(req, res, options, origin))
      return;
    if (media_type(req.get_header_value("Content-Type")) != "application/json") {
      send(res, {{"code", "INVALID_INPUT"}, {"message", "Envie application/json"}}, 400);
      return;
    }
    if (req.body.size() > 1024 * 1024) {
      send(res, {{"code", "INVALID_INPUT"}, {"message", "Corpo excede limite de 1 MiB"}}, 400);
      return;
    }
    auto result = mcp_dispatcher.dispatch(req.body, req.get_header_value("Mcp-Session-Id"));
    if (!result.session_id.empty())
      res.set_header("Mcp-Session-Id", result.session_id);
    if (result.response.is_null()) {
      res.status = 202;
      res.set_content("", "text/plain; charset=utf-8");
    } else {
      send(res, result.response, 200);
    }
  });
  server.Get("/api/v1/session", [&](const auto &, auto &res) {
    send(res, {{"token", token}, {"authority", "local single user"}});
  });
  server.Get("/api/v1/experiences", [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    send(res, svc.experiences(integer(req, "window", 2, 2), integer(req, "limit", 100, 500),
                              integer(req, "offset", 0, 100000)));
  });
  server.Get(R"(/api/v1/experiences/([a-f0-9]{32}))", [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    send(res, svc.experience(req.matches[1], integer(req, "window", 2, 2)));
  });
  server.Post("/api/v1/experiences", [&](const auto &req, auto &res) {
    auto input = body(req);
    input["window"] = integer(req, "window", 2, 2);
    send(res, execute_operation(svc, "preserve_experience", input, {true, "web-experiences"}), 201);
  });
  server.Get("/health", [&](const auto &, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    send(res, {{"status", "technical_readiness"},
               {"version", "0.1.0"},
               {"standard", "C++26"},
               {"sqlite", sqlite3_libversion()},
               {"database", svc.store.query("PRAGMA quick_check")[0]},
               {"milestone", "NOT_READY"},
               {"limitation", "Aceitação humana e gates de produto ainda pendentes"}});
  });
  server.Post("/api/v1/artifacts", [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    if (!req.form.has_file("file"))
      throw Error(400, "INVALID_INPUT", "Envie um arquivo multipart no campo file");
    auto f = req.form.get_file("file");
    json meta = json::object();
    if (req.form.has_field("metadata"))
      meta = json::parse(req.form.get_field("metadata"));
    if (!meta.is_object())
      throw Error(400, "INVALID_INPUT", "metadata deve ser objeto");
    meta["declared_mime"] = f.content_type;
    send(res, svc.import_file(f.filename, f.content, meta, integer(req, "window", 2, 2)), 201);
  });
  server.Get(
      R"(/api/v1/artifacts/([a-f0-9]{32})/(original|content))", [&](const auto &req, auto &res) {
        std::lock_guard lock(svc.store.mutex);
        auto a = svc.store.get("artifacts", req.matches[1], integer(req, "window", 2, 2));
        auto bytes = svc.store.object(a["sha256"]);
        if (req.matches[2] == "content") {
          std::string type = a["mime"];
          if (a["preview"] != "AVAILABLE")
            throw Error(422, "CAPABILITY_UNAVAILABLE", "Prévia indisponível; baixe o original");
          res.set_content(bytes, type);
        } else {
          res.set_header("Content-Disposition", "attachment; filename=\"original-" +
                                                    a["id"].template get<std::string>() + "\"");
          res.set_content(bytes, "application/octet-stream");
        }
      });
  server.Delete(R"(/api/v1/artifacts/([a-f0-9]{32}))", [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    auto in = body(req);
    send(res, svc.purge(req.matches[1], in.value("confirm_sha256", "")));
  });
  server.Post("/api/v1/anchors", [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    send(res, svc.anchor(body(req), integer(req, "window", 2, 2)), 201);
  });
  server.Get("/api/v1/search", [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    auto q = req.get_param_value("q");
    if (q.size() > 1000)
      throw Error(400, "INVALID_INPUT", "Consulta longa demais");
    send(res, svc.search(q, integer(req, "window", 2, 2), integer(req, "limit", 50, 100),
                         integer(req, "offset", 0, 100000), req.has_param("scope") ? req.get_param_value("scope") : "investigative"));
  });
  server.Get(R"(/api/v1/history/([a-z_]+)/([a-zA-Z0-9-]+))", [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    send(res, svc.history(req.matches[1], req.matches[2], integer(req, "window", 2, 2)));
  });
  server.Get("/api/v1/explore", [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    send(res, svc.explore("", integer(req, "window", 2, 2), req.get_param_value("artifact_id")));
  });
  server.Get(R"(/api/v1/explore/([a-f0-9]{32}))", [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    send(res, svc.explore(req.matches[1], integer(req, "window", 2, 2),
                          req.get_param_value("artifact_id")));
  });
  server.Post("/api/v1/runs", [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    send(res, svc.run(body(req), integer(req, "window", 2, 2)), 201);
  });
  server.Post("/api/v1/lab/import", [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    send(res, svc.import_lab(integer(req, "window", 2, 2)), 201);
  });
  server.Post("/api/v1/backups", [&](const auto &, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    auto id = uuid();
    auto path = svc.store.root / "backups" / id;
    svc.store.backup(path);
    send(res, {{"id", id}, {"path", path.string()}, {"status", "BACKUP_CREATED"}}, 201);
  });
  server.Get(R"(/api/v1/export/inquiries/([a-f0-9]{32}))", [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    res.set_header("Content-Disposition", "attachment; filename=\"noese-inquiry.json\"");
    send(res, svc.export_inquiry(req.matches[1], integer(req, "window", 2, 2)));
  });
  const std::string resources =
      "(artifacts|occurrences|anchors|inquiries|statements|relations|opportunities|assessments|"
      "contributions|inquiry_members|runs|capabilities)";
  server.Get("/api/v1/" + resources, [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    int limit = integer(req, "limit", 100, 500), offset = integer(req, "offset", 0, 100000);
    if (req.matches[1] == "artifacts") {
      send(res, svc.artifacts(integer(req, "window", 2, 2), limit, offset,
                             req.has_param("scope") ? req.get_param_value("scope") : "investigative"));
      return;
    }
    send(res, {{"items", svc.store.list(req.matches[1], integer(req, "window", 2, 2),
                                        req.get_param_value("inquiry_id"), limit, offset)},
               {"limit", limit},
               {"offset", offset}});
  });
  server.Get("/api/v1/" + resources + "/([a-zA-Z0-9-]+)", [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    send(res, svc.store.get(req.matches[1], req.matches[2], integer(req, "window", 2, 2)));
  });
  server.Post("/api/v1/" + resources, [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    send(res, svc.record(req.matches[1], body(req), integer(req, "window", 2, 2)), 201);
  });
  server.Put("/api/v1/" + resources + "/([a-zA-Z0-9-]+)", [&](const auto &req, auto &res) {
    std::lock_guard lock(svc.store.mutex);
    auto in = body(req);
    in["id"] = std::string(req.matches[2]);
    if (!in.contains("version") || !in["version"].is_number_integer() ||
        in["version"].template get<int>() < 1)
      throw Error(400, "INVALID_INPUT", "Versão atual obrigatória");
    send(res, svc.record(req.matches[1], in, integer(req, "window", 2, 2)));
  });
  if (!server.bind_to_port("127.0.0.1", options.port))
    throw std::runtime_error("Cannot bind loopback port");
  investigator.start();
  std::cout << "NOESE http://127.0.0.1:" << options.port << "  data=" << svc.store.root
            << "  mcp_http=" << (options.mcp_allow_writes ? "read_write" : "read_only")
            << (options.mcp_token.empty() ? ",local" : ",token") << std::endl;
  if (!server.listen_after_bind())
    throw std::runtime_error("Cannot bind loopback port");
}
