-- Orbis Vault remote install queue (Cloudflare D1)
-- Does not contain admin passwords or device tokens in plaintext.

CREATE TABLE IF NOT EXISTS devices (
  device_id TEXT PRIMARY KEY,
  device_name TEXT NOT NULL,
  token_hash TEXT NOT NULL,
  paired INTEGER NOT NULL DEFAULT 0,
  enabled INTEGER NOT NULL DEFAULT 1,
  last_seen TEXT,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  paired_at TEXT
);

CREATE TABLE IF NOT EXISTS device_pairings (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  device_id TEXT NOT NULL,
  code_hash TEXT NOT NULL,
  expires_at TEXT NOT NULL,
  consumed INTEGER NOT NULL DEFAULT 0,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY(device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_pairings_code
ON device_pairings(code_hash, consumed);

CREATE TABLE IF NOT EXISTS device_commands (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  device_id TEXT NOT NULL,
  action TEXT NOT NULL,
  title_db_id INTEGER,
  title_id TEXT,
  package_ids_json TEXT NOT NULL DEFAULT '[]',
  status TEXT NOT NULL DEFAULT 'QUEUED',
  progress INTEGER NOT NULL DEFAULT 0,
  message TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  started_at TEXT,
  completed_at TEXT,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY(device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_device_commands_poll
ON device_commands(device_id, status, id);
