#pragma once
#include <filesystem>
#include <json.hpp>
#include <mutex>
#include <sqlite3.h>
#include <stdexcept>
using json = nlohmann::json;
namespace fs = std::filesystem;
struct Error : std::runtime_error {
  int http;
  std::string code;
  Error(int h, std::string c, std::string m) : runtime_error(m), http(h), code(std::move(c)) {}
};
std::string sha256(const std::string &bytes);
std::string uuid();
std::string now();
std::string read_file(const fs::path &path);
void write_file(const fs::path &path, const std::string &bytes);
// Cooperating HTTP/MCP processes serialize an entire operation, including objects.
class ArchiveMutex {
  std::recursive_mutex thread_mutex;
  int descriptor = -1;
  unsigned depth = 0;

public:
  void open(const fs::path &path);
  void lock();
  void unlock();
  ~ArchiveMutex();
};
struct Store {
  sqlite3 *db = nullptr;
  fs::path root;
  ArchiveMutex mutex;
  explicit Store(fs::path path);
  ~Store();
  void exec(const std::string &sql);
  json query(const std::string &sql, const std::vector<std::string> &args = {});
  void change(const std::string &sql, const std::vector<std::string> &args = {});
  json get(const std::string &type, const std::string &id, int window = 2);
  json list(const std::string &type, int window = 2, const std::string &inquiry = "",
            int limit = 100, int offset = 0);
  json save(const std::string &type, json record, int expected = 0);
  void event(const std::string &object, const std::string &action, const std::string &actor,
             json detail = json::object());
  std::string object(const std::string &hash);
  void backup(const fs::path &destination);
  static void restore(const fs::path &source, const fs::path &destination);
};
struct Transaction {
  Store &s;
  bool committed = false;
  std::string savepoint;
  explicit Transaction(Store &store) : s(store) {
    if (sqlite3_get_autocommit(s.db))
      s.exec("BEGIN IMMEDIATE");
    else {
      savepoint = "op_" + uuid();
      s.exec("SAVEPOINT " + savepoint);
    }
  }
  void commit() {
    s.exec(savepoint.empty() ? "COMMIT" : "RELEASE SAVEPOINT " + savepoint);
    committed = true;
  }
  ~Transaction() {
    if (!committed) {
      auto sql = savepoint.empty()
                     ? "ROLLBACK"
                     : "ROLLBACK TO SAVEPOINT " + savepoint + "; RELEASE SAVEPOINT " + savepoint;
      sqlite3_exec(s.db, sql.c_str(), nullptr, nullptr, nullptr);
    }
  }
};
