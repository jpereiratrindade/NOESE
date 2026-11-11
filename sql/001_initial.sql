CREATE TABLE IF NOT EXISTS schema_migrations(version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL);
INSERT OR IGNORE INTO schema_migrations VALUES(1,strftime('%Y-%m-%dT%H:%M:%SZ','now'));
CREATE TABLE IF NOT EXISTS events(id TEXT PRIMARY KEY, object_id TEXT NOT NULL, action TEXT NOT NULL,actor TEXT NOT NULL,recorded_at TEXT NOT NULL,detail TEXT NOT NULL CHECK(json_valid(detail)));
CREATE INDEX IF NOT EXISTS event_object ON events(object_id,recorded_at);
CREATE VIRTUAL TABLE IF NOT EXISTS artifact_fts USING fts5(id UNINDEXED,text,tokenize='unicode61');
