// Orbis Vault remote-device API patch for the existing Cloudflare Worker.
//
// Assumes the main Worker already provides:
//   json(data, status)
//   requireAdmin(request, env)
//   env.DB
//
// Add the route snippets from docs/REMOTE_WORKER_INTEGRATION.md to fetch().

const DEVICE_ACTIONS = new Set([
  "INSTALL_SELECTED",
  "INSTALL_ALL",
  "CANCEL_COMMAND",
  "SYNC_CATALOG"
]);

const DEVICE_STATES = new Set([
  "QUEUED",
  "ACCEPTED",
  "DOWNLOADING",
  "VERIFYING",
  "INSTALLING",
  "COMPLETED",
  "ERROR",
  "CANCELLED"
]);

async function sha256Hex(value) {
  const data = new TextEncoder().encode(value);
  const digest = await crypto.subtle.digest("SHA-256", data);
  return Array.from(new Uint8Array(digest))
    .map(b => b.toString(16).padStart(2, "0"))
    .join("");
}

function validHex256(v) {
  return typeof v === "string" && /^[a-fA-F0-9]{64}$/.test(v);
}

function validDeviceId(v) {
  return typeof v === "string" && /^[A-Za-z0-9_-]{12,80}$/.test(v);
}

async function requireDevice(request, env, expectedDeviceId = null) {
  const auth = request.headers.get("Authorization") || "";
  if (!auth.startsWith("Bearer ")) {
    return {
      authorized: false,
      response: json({ error: "Missing device authorization" }, 401)
    };
  }

  const rawToken = auth.slice(7).trim();
  if (rawToken.length < 32 || rawToken.length > 256) {
    return {
      authorized: false,
      response: json({ error: "Invalid device authorization" }, 401)
    };
  }

  const tokenHash = await sha256Hex(rawToken);

  const device = expectedDeviceId
    ? await env.DB.prepare(
        "SELECT device_id, device_name, paired, enabled FROM devices WHERE device_id = ? AND token_hash = ?"
      ).bind(expectedDeviceId, tokenHash).first()
    : await env.DB.prepare(
        "SELECT device_id, device_name, paired, enabled FROM devices WHERE token_hash = ?"
      ).bind(tokenHash).first();

  if (!device || !device.paired || !device.enabled) {
    return {
      authorized: false,
      response: json({ error: "Invalid or unpaired device" }, 401)
    };
  }

  return { authorized: true, device };
}

function randomHex(bytes) {
  const data = new Uint8Array(bytes);
  crypto.getRandomValues(data);
  return Array.from(data).map(b => b.toString(16).padStart(2, "0")).join("");
}

function randomPairCode() {
  const data = new Uint32Array(1);
  crypto.getRandomValues(data);
  return String(data[0] % 1000000).padStart(6, "0");
}

// PUBLIC: PS4 starts pairing.
// Worker generates the device token and 6-digit code.
// Plain token/code are returned only once over HTTPS to the PS4.
// D1 stores only SHA-256(token) and SHA-256(code).
async function handleDevicePairStart(request, env) {
  const body = await request.json();
  const deviceId = String(body.device_id || "");
  const deviceName = String(body.device_name || "PS4").slice(0, 80);

  if (!validDeviceId(deviceId)) {
    return json({ error: "Invalid device_id" }, 400);
  }

  const deviceToken = randomHex(32);
  const pairCode = randomPairCode();
  const tokenHash = await sha256Hex(deviceToken);
  const codeHash = await sha256Hex(pairCode);

  await env.DB.prepare(
    `INSERT INTO devices (device_id, device_name, token_hash, paired, enabled, last_seen)
     VALUES (?, ?, ?, 0, 1, CURRENT_TIMESTAMP)
     ON CONFLICT(device_id) DO UPDATE SET
       device_name = excluded.device_name,
       token_hash = excluded.token_hash,
       paired = 0,
       enabled = 1,
       last_seen = CURRENT_TIMESTAMP`
  ).bind(deviceId, deviceName, tokenHash).run();

  await env.DB.prepare(
    "UPDATE device_pairings SET consumed = 1 WHERE device_id = ? AND consumed = 0"
  ).bind(deviceId).run();

  await env.DB.prepare(
    `INSERT INTO device_pairings (device_id, code_hash, expires_at, consumed)
     VALUES (?, ?, datetime('now', '+10 minutes'), 0)`
  ).bind(deviceId, codeHash).run();

  return json({
    device_id: deviceId,
    device_token: deviceToken,
    pairing_code: pairCode,
    expires_in: 600,
    message: "Pairing started"
  }, 201);
}

// PUBLIC: PS4 checks whether the code was confirmed.
// No token is returned because the console already owns its device_token.
async function handleDevicePairStatus(env, deviceId) {
  if (!validDeviceId(deviceId)) {
    return json({ error: "Invalid device_id" }, 400);
  }

  const device = await env.DB.prepare(
    "SELECT device_id, device_name, paired, enabled FROM devices WHERE device_id = ?"
  ).bind(deviceId).first();

  if (!device) return json({ error: "Device not found" }, 404);

  return json({
    device_id: device.device_id,
    device_name: device.device_name,
    paired: !!device.paired,
    enabled: !!device.enabled
  });
}

// ADMIN: confirms the 6-digit code typed in Android.
async function handleAdminPairConfirm(request, env) {
  const body = await request.json();
  const code = String(body.code || "").trim();

  if (!/^\d{6}$/.test(code)) {
    return json({ error: "Pairing code must contain 6 digits" }, 400);
  }

  const codeHash = await sha256Hex(code);
  const pairing = await env.DB.prepare(
    `SELECT id, device_id
       FROM device_pairings
       WHERE code_hash = ?
         AND consumed = 0
         AND expires_at > CURRENT_TIMESTAMP
       ORDER BY id DESC
       LIMIT 1`
  ).bind(codeHash).first();

  if (!pairing) {
    return json({ error: "Pairing code not found or expired" }, 404);
  }

  await env.DB.batch([
    env.DB.prepare(
      "UPDATE device_pairings SET consumed = 1 WHERE id = ?"
    ).bind(pairing.id),
    env.DB.prepare(
      "UPDATE devices SET paired = 1, paired_at = CURRENT_TIMESTAMP WHERE device_id = ?"
    ).bind(pairing.device_id)
  ]);

  return json({
    device_id: pairing.device_id,
    paired: true,
    message: "PS4 paired successfully"
  });
}

async function handleAdminListDevices(env) {
  const result = await env.DB.prepare(
    `SELECT device_id, device_name, paired, enabled, last_seen, created_at, paired_at
     FROM devices
     ORDER BY COALESCE(last_seen, created_at) DESC`
  ).all();

  return json({ devices: result.results || [] });
}

// ADMIN: sends one command to a paired PS4.
async function handleAdminCreateDeviceCommand(request, env) {
  const body = await request.json();
  const deviceId = String(body.device_id || "");
  const action = String(body.action || "");
  const titleDbId = body.title_db_id == null ? null : Number(body.title_db_id);
  const titleId = body.title_id == null ? null : String(body.title_id);
  const packageIds = Array.isArray(body.package_ids)
    ? body.package_ids.map(Number).filter(Number.isInteger)
    : [];

  if (!validDeviceId(deviceId)) {
    return json({ error: "Invalid device_id" }, 400);
  }
  if (!DEVICE_ACTIONS.has(action)) {
    return json({ error: "Invalid action" }, 400);
  }

  const device = await env.DB.prepare(
    "SELECT device_id FROM devices WHERE device_id = ? AND paired = 1 AND enabled = 1"
  ).bind(deviceId).first();

  if (!device) {
    return json({ error: "Device not found, disabled, or unpaired" }, 404);
  }

  const result = await env.DB.prepare(
    `INSERT INTO device_commands
       (device_id, action, title_db_id, title_id, package_ids_json, status, progress, message)
     VALUES (?, ?, ?, ?, ?, 'QUEUED', 0, '')`
  ).bind(
    deviceId,
    action,
    titleDbId,
    titleId,
    JSON.stringify(packageIds)
  ).run();

  return json({
    id: result.meta.last_row_id,
    status: "QUEUED",
    message: "Command queued"
  }, 201);
}

// DEVICE: fetches pending/current commands.
async function handleDeviceCommands(request, env, url) {
  const deviceId = url.searchParams.get("device_id") || "";
  const auth = await requireDevice(request, env, deviceId);
  if (!auth.authorized) return auth.response;

  await env.DB.prepare(
    "UPDATE devices SET last_seen = CURRENT_TIMESTAMP WHERE device_id = ?"
  ).bind(deviceId).run();

  const result = await env.DB.prepare(
    `SELECT id, action, title_db_id, title_id, package_ids_json,
            status, progress, message, created_at, started_at, completed_at, updated_at
       FROM device_commands
       WHERE device_id = ?
         AND status IN ('QUEUED','ACCEPTED','DOWNLOADING','VERIFYING','INSTALLING')
       ORDER BY id ASC
       LIMIT 20`
  ).bind(deviceId).all();

  const commands = (result.results || []).map(row => ({
    ...row,
    package_ids: (() => {
      try { return JSON.parse(row.package_ids_json || "[]"); }
      catch { return []; }
    })()
  }));

  return json({ commands });
}

// DEVICE: reports progress/state.
async function handleDeviceCommandStatus(request, env, commandIdStr) {
  const id = Number(commandIdStr);
  if (!Number.isInteger(id) || id <= 0) {
    return json({ error: "Invalid command id" }, 400);
  }

  const row = await env.DB.prepare(
    "SELECT id, device_id, status FROM device_commands WHERE id = ?"
  ).bind(id).first();

  if (!row) return json({ error: "Command not found" }, 404);

  const auth = await requireDevice(request, env, row.device_id);
  if (!auth.authorized) return auth.response;

  const body = await request.json();
  const status = String(body.status || "");
  const progress = Math.max(0, Math.min(100, Number(body.progress || 0)));
  const message = String(body.message || "").slice(0, 500);

  if (!DEVICE_STATES.has(status)) {
    return json({ error: "Invalid command state" }, 400);
  }

  const startedAt =
    row.status === "QUEUED" && status !== "QUEUED"
      ? "CURRENT_TIMESTAMP"
      : "started_at";

  const terminal = ["COMPLETED", "ERROR", "CANCELLED"].includes(status);

  await env.DB.prepare(
    `UPDATE device_commands
       SET status = ?,
           progress = ?,
           message = ?,
           started_at = CASE
             WHEN started_at IS NULL AND ? <> 'QUEUED' THEN CURRENT_TIMESTAMP
             ELSE started_at
           END,
           completed_at = CASE
             WHEN ? IN ('COMPLETED','ERROR','CANCELLED') THEN CURRENT_TIMESTAMP
             ELSE completed_at
           END,
           updated_at = CURRENT_TIMESTAMP
       WHERE id = ?`
  ).bind(status, progress, message, status, status, id).run();

  await env.DB.prepare(
    "UPDATE devices SET last_seen = CURRENT_TIMESTAMP WHERE device_id = ?"
  ).bind(row.device_id).run();

  return json({ id, status, progress, terminal });
}

async function handleDeviceHeartbeat(request, env) {
  const body = await request.json();
  const deviceId = String(body.device_id || "");
  const auth = await requireDevice(request, env, deviceId);
  if (!auth.authorized) return auth.response;

  await env.DB.prepare(
    "UPDATE devices SET last_seen = CURRENT_TIMESTAMP WHERE device_id = ?"
  ).bind(deviceId).run();

  return json({ ok: true });
}
