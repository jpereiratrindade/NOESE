#include "Investigator.hpp"
#include "Operations.hpp"
#include <httplib.h>
#include <iostream>
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
} // namespace
void serve(Service &svc, int port) {
  httplib::Server server;
  Investigator investigator(svc);
  const std::string token = uuid() + uuid();
  const std::string host = "127.0.0.1:" + std::to_string(port), origin = "http://" + host;
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
    if (req.get_header_value("Host") != host) {
      send(res, {{"code", "INVALID_INPUT"}, {"message", "Host local inválido; use " + origin}},
           403);
      return httplib::Server::HandlerResponse::Handled;
    }
    auto request_origin = req.get_header_value("Origin");
    if ((!request_origin.empty() && request_origin != origin) ||
        req.get_header_value("Sec-Fetch-Site") == "cross-site") {
      send(res, {{"code", "AUTHORITY_REQUIRED"}, {"message", "Origem externa recusada"}}, 403);
      return httplib::Server::HandlerResponse::Handled;
    }
    if (req.method != "GET" && req.method != "HEAD" &&
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
       {"index.html", "workspace.html", "app.js", "style.css", "attention.js", "attention.css"}) {
    std::string route = std::string(file) == "index.html"       ? "/"
                        : std::string(file) == "workspace.html" ? "/mesa"
                                                                : "/" + std::string(file);
    server.Get(route, [file](const auto &, auto &res) {
      res.set_content(read_file(fs::path(NOESE_ROOT) / "web" / file),
                      std::string(file).ends_with(".js")    ? "application/javascript"
                      : std::string(file).ends_with(".css") ? "text/css"
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
               {"transport", "MCP stdio"},
               {"default_mode", "read_only"}});
  });
  server.Get("/api/v1/session", [&](const auto &, auto &res) {
    send(res, {{"token", token}, {"authority", "local single user"}});
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
                         integer(req, "offset", 0, 100000)));
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
  if (!server.bind_to_port("127.0.0.1", port))
    throw std::runtime_error("Cannot bind loopback port");
  investigator.start();
  std::cout << "NOESE http://127.0.0.1:" << port << "  data=" << svc.store.root << std::endl;
  if (!server.listen_after_bind())
    throw std::runtime_error("Cannot bind loopback port");
}
