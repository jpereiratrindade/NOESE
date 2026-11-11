#include "Store.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sstream>
#include <sys/stat.h>
namespace {
const std::vector<std::string> types = {"artifacts",       "occurrences",   "anchors",
                                        "statements",      "relations",     "inquiries",
                                        "inquiry_members", "opportunities", "contributions",
                                        "assessments",     "runs",          "capabilities"};
void valid_type(const std::string &t) {
  if (std::find(types.begin(), types.end(), t) == types.end())
    throw Error(400, "INVALID_INPUT", "Tipo desconhecido");
}
struct Stmt {
  sqlite3_stmt *p = nullptr;
  ~Stmt() {
    sqlite3_finalize(p);
  }
};
void prepare(sqlite3 *db, Stmt &s, const std::string &sql, const std::vector<std::string> &args) {
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &s.p, nullptr) != SQLITE_OK)
    throw std::runtime_error(sqlite3_errmsg(db));
  for (size_t i = 0; i < args.size(); ++i)
    if (sqlite3_bind_text(s.p, i + 1, args[i].data(), args[i].size(), SQLITE_TRANSIENT) !=
        SQLITE_OK)
      throw std::runtime_error("bind failed");
}
json manifest(const fs::path &directory) {
  json entries = json::array();
  for (auto &p : fs::directory_iterator(directory / "objects"))
    if (p.is_regular_file()) {
      auto bytes = read_file(p.path());
      auto h = sha256(bytes);
      if (h != p.path().filename().string())
        throw std::runtime_error("Object integrity failure");
      entries.push_back({{"sha256", h}, {"size", bytes.size()}});
    }
  return entries;
}
} // namespace
std::string sha256(const std::string &bytes) {
  unsigned char result[32];
  unsigned int len = 0;
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (!ctx)
    throw std::runtime_error("EVP allocation");
  bool ok = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
            EVP_DigestUpdate(ctx, bytes.data(), bytes.size()) == 1 &&
            EVP_DigestFinal_ex(ctx, result, &len) == 1;
  EVP_MD_CTX_free(ctx);
  if (!ok)
    throw std::runtime_error("SHA256 failure");
  std::ostringstream out;
  for (unsigned char c : result)
    out << std::hex << std::setw(2) << std::setfill('0') << int(c);
  return out.str();
}
std::string uuid() {
  unsigned char b[16];
  if (RAND_bytes(b, 16) != 1)
    throw std::runtime_error("Random generation failure");
  std::ostringstream o;
  for (auto c : b)
    o << std::hex << std::setw(2) << std::setfill('0') << int(c);
  return o.str();
}
std::string now() {
  auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm tm{};
  gmtime_r(&t, &tm);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}
std::string read_file(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f)
    throw std::runtime_error("Cannot read " + p.string());
  return {std::istreambuf_iterator<char>(f), {}};
}
void write_file(const fs::path &p, const std::string &bytes) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f || !f.write(bytes.data(), bytes.size()))
    throw std::runtime_error("Cannot write " + p.string());
}
Store::Store(fs::path p) : root(fs::absolute(p)) {
  fs::create_directories(root / "objects");
  chmod(root.c_str(), 0700);
  chmod((root / "objects").c_str(), 0700);
  if (sqlite3_open((root / "noese.sqlite").c_str(), &db) != SQLITE_OK)
    throw std::runtime_error("SQLite open failed");
  sqlite3_busy_timeout(db, 5000);
  exec("PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA secure_delete=ON;");
  exec(read_file(fs::path(NOESE_ROOT) / "sql/001_initial.sql"));
  for (auto &t : types) {
    exec("CREATE TABLE IF NOT EXISTS " + t +
         "(id TEXT NOT NULL,version INTEGER NOT NULL,payload TEXT NOT NULL "
         "CHECK(json_valid(payload)),PRIMARY KEY(id,version));");
    exec("CREATE TRIGGER IF NOT EXISTS " + t + "_no_update BEFORE UPDATE ON " + t +
         " BEGIN SELECT RAISE(ABORT,'immutable revision'); END;");
  }
  exec("CREATE TRIGGER IF NOT EXISTS events_no_update BEFORE UPDATE ON events BEGIN SELECT "
       "RAISE(ABORT,'append only events'); END;");
}
Store::~Store() {
  if (db)
    sqlite3_close(db);
}
void Store::exec(const std::string &sql) {
  char *msg = nullptr;
  if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &msg) != SQLITE_OK) {
    std::string m = msg ? msg : "SQLite error";
    sqlite3_free(msg);
    throw std::runtime_error(m);
  }
}
json Store::query(const std::string &sql, const std::vector<std::string> &args) {
  Stmt s;
  prepare(db, s, sql, args);
  json rows = json::array();
  int rc;
  while ((rc = sqlite3_step(s.p)) == SQLITE_ROW) {
    json row = json::object();
    for (int c = 0; c < sqlite3_column_count(s.p); ++c) {
      std::string name = sqlite3_column_name(s.p, c);
      if (sqlite3_column_type(s.p, c) == SQLITE_NULL)
        row[name] = nullptr;
      else
        row[name] = reinterpret_cast<const char *>(sqlite3_column_text(s.p, c));
    }
    rows.push_back(row);
  }
  if (rc != SQLITE_DONE)
    throw std::runtime_error(sqlite3_errmsg(db));
  return rows;
}
void Store::change(const std::string &sql, const std::vector<std::string> &args) {
  Stmt s;
  prepare(db, s, sql, args);
  if (sqlite3_step(s.p) != SQLITE_DONE)
    throw std::runtime_error(sqlite3_errmsg(db));
}
json Store::get(const std::string &t, const std::string &id, int window) {
  valid_type(t);
  auto rows = query("SELECT payload FROM " + t +
                        " WHERE id=? AND CAST(json_extract(payload,'$.window') AS INTEGER)<=? "
                        "ORDER BY version DESC LIMIT 1",
                    {id, std::to_string(window)});
  if (rows.empty())
    throw Error(404, "NOT_FOUND", "Registro não encontrado");
  auto r = json::parse(rows[0]["payload"].get<std::string>());
  if (r.value("window", 0) > window)
    throw Error(404, "NOT_FOUND", "Indisponível nesta janela");
  return r;
}
json Store::list(const std::string &t, int window, const std::string &inquiry, int limit,
                 int offset) {
  valid_type(t);
  auto rows =
      query("SELECT a.payload FROM " + t + " a WHERE a.version=(SELECT MAX(b.version) FROM " + t +
                " b WHERE b.id=a.id AND CAST(json_extract(b.payload,'$.window') AS INTEGER)<=?) "
                "AND CAST(json_extract(a.payload,'$.window') AS INTEGER)<=? AND (?='' OR "
                "json_extract(a.payload,'$.inquiry_id')=?) ORDER BY rowid DESC LIMIT ? OFFSET ?",
            {std::to_string(window), std::to_string(window), inquiry, inquiry,
             std::to_string(limit), std::to_string(offset)});
  json out = json::array();
  for (auto &row : rows)
    out.push_back(json::parse(row["payload"].get<std::string>()));
  return out;
}
json Store::save(const std::string &t, json r, int expected) {
  valid_type(t);
  std::string id = r.value("id", uuid());
  auto rows = query("SELECT MAX(version) AS v FROM " + t + " WHERE id=?", {id});
  int v = rows[0]["v"].is_null() ? 0 : std::stoi(rows[0]["v"].get<std::string>());
  if (v != expected)
    throw Error(409, "CONFLICT", "Versão mudou; recarregue antes de revisar");
  r["id"] = id;
  r["version"] = v + 1;
  r["created_at"] = now();
  r["supersedes_version"] = v ? json(v) : json(nullptr);
  if (!r.contains("window"))
    r["window"] = 0;
  change("INSERT INTO " + t + "(id,version,payload) VALUES(?,?,?)",
         {id, std::to_string(v + 1), r.dump()});
  return r;
}
void Store::event(const std::string &id, const std::string &action, const std::string &actor,
                  json detail) {
  change("INSERT INTO events(id,object_id,action,actor,recorded_at,detail) VALUES(?,?,?,?,?,?)",
         {uuid(), id, action, actor, now(), detail.dump()});
}
std::string Store::object(const std::string &h) {
  if (h.size() != 64 || h.find_first_not_of("0123456789abcdef") != std::string::npos)
    throw Error(400, "INVALID_INPUT", "Hash inválido");
  auto bytes = read_file(root / "objects" / h);
  if (sha256(bytes) != h)
    throw Error(500, "INTEGRITY_FAILURE", "Original alterado");
  return bytes;
}
void Store::backup(const fs::path &dest) {
  if (fs::exists(dest))
    throw Error(409, "CONFLICT", "Destino já existe");
  fs::create_directories(dest / "objects");
  chmod(dest.c_str(), 0700);
  sqlite3 *output = nullptr;
  if (sqlite3_open((dest / "noese.sqlite").c_str(), &output) != SQLITE_OK)
    throw std::runtime_error("backup open");
  auto b = sqlite3_backup_init(output, "main", db, "main");
  if (!b) {
    sqlite3_close(output);
    throw std::runtime_error("backup init");
  }
  int rc = sqlite3_backup_step(b, -1);
  int finish = sqlite3_backup_finish(b);
  sqlite3_close(output);
  if (rc != SQLITE_DONE || finish != SQLITE_OK)
    throw std::runtime_error("backup failed");
  for (auto &p : fs::directory_iterator(root / "objects"))
    fs::copy_file(p.path(), dest / "objects" / p.path().filename());
  write_file(dest / "manifest.json",
             json{{"format", 1},
                  {"database_sha256", sha256(read_file(dest / "noese.sqlite"))},
                  {"objects", manifest(dest)},
                  {"created_at", now()}}
                 .dump(2));
}
void Store::restore(const fs::path &source, const fs::path &dest) {
  if (fs::exists(dest))
    throw Error(409, "CONFLICT", "Restauração exige destino novo");
  auto m = json::parse(read_file(source / "manifest.json"));
  if (sha256(read_file(source / "noese.sqlite")) != m.at("database_sha256").get<std::string>())
    throw std::runtime_error("Database hash mismatch");
  for (auto &entry : m.at("objects")) {
    auto h = entry.at("sha256").get<std::string>();
    if (h.size() != 64 || h.find_first_not_of("0123456789abcdef") != std::string::npos ||
        sha256(read_file(source / "objects" / h)) != h)
      throw std::runtime_error("Backup object mismatch");
  }
  fs::create_directories(dest / "objects");
  chmod(dest.c_str(), 0700);
  fs::copy_file(source / "noese.sqlite", dest / "noese.sqlite");
  for (auto &entry : m.at("objects")) {
    auto h = entry.at("sha256").get<std::string>();
    fs::copy_file(source / "objects" / h, dest / "objects" / h);
  }
}
