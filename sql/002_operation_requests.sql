CREATE TABLE IF NOT EXISTS operation_requests (
  request_id TEXT PRIMARY KEY,
  signature TEXT NOT NULL,
  revoked INTEGER NOT NULL DEFAULT 0 CHECK(revoked IN (0,1)),
  result TEXT NOT NULL CHECK(json_valid(result)),
  created_at TEXT NOT NULL
);
INSERT OR IGNORE INTO schema_migrations VALUES(2,strftime('%Y-%m-%dT%H:%M:%SZ','now'));
