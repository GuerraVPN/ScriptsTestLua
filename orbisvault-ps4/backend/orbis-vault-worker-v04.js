// ============================================================
// orbis-vault-api — Cloudflare Worker
// Complete API for Orbis Vault Admin Android
// ============================================================

const corsHeaders = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET, POST, PUT, PATCH, DELETE, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type, Authorization",
};

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "Content-Type": "application/json", ...corsHeaders },
  });
}

// ============================================================
// AUTHENTICATION
// ============================================================

async function verifySessionToken(token, secret) {
  const parts = token.split(".");
  if (parts.length !== 2) return null;
  const [payloadB64, sigHex] = parts;
  const enc = new TextEncoder();
  const key = await crypto.subtle.importKey(
    "raw", enc.encode(secret), { name: "HMAC", hash: "SHA-256" }, false, ["verify"]
  );
  if (!/^[0-9a-fA-F]+$/.test(sigHex) || sigHex.length % 2 !== 0) return null;
  const sigPairs = sigHex.match(/.{2}/g);
  if (!sigPairs) return null;
  const sigBytes = new Uint8Array(sigPairs.map(h => parseInt(h, 16)));
  const valid = await crypto.subtle.verify("HMAC", key, sigBytes, enc.encode(payloadB64));
  if (!valid) return null;
  let payload;
  try { payload = JSON.parse(atob(payloadB64)); } catch { return null; }
  if (payload.exp && Date.now() > payload.exp) return null;
  return payload;
}

async function requireAdmin(request, env) {
  const auth = request.headers.get("Authorization");
  if (!auth || !auth.startsWith("Bearer ")) {
    return { authorized: false, response: json({ error: "Missing or invalid Authorization header" }, 401) };
  }
  const token = auth.slice(7);
  const payload = await verifySessionToken(token, env.SESSION_SECRET);
  if (!payload) {
    return { authorized: false, response: json({ error: "Invalid or expired token" }, 401) };
  }
  return { authorized: true, payload };
}

// ============================================================
// REVISION MANAGEMENT
// ============================================================

async function bumpRevision(env) {
  const meta = await env.DB.prepare("SELECT value FROM catalog_meta WHERE key = 'revision'").first();
  let revision = 1;
  if (meta) {
    revision = parseInt(meta.value) + 1;
    await env.DB.prepare("UPDATE catalog_meta SET value = ? WHERE key = 'revision'").bind(String(revision)).run();
  } else {
    await env.DB.prepare("INSERT INTO catalog_meta (key, value) VALUES ('revision', '1')").run();
  }
  return revision;
}

// ============================================================
// PACKAGE GROUPING & TITLE FORMATTING
// ============================================================

function groupPackages(packages) {
  const base = {};
  const updates = [];
  const dlcs = [];
  for (const pkg of packages) {
    const p = {
      id: pkg.id,
      package_type: pkg.package_type,
      name: pkg.name,
      version: pkg.version,
      required_base_version: pkg.required_base_version || "",
      source_url: pkg.source_url || "",
      size_bytes: pkg.size_bytes || 0,
      sha256: pkg.sha256 || ""
    };
    if (pkg.package_type === "BASE") {
      Object.assign(base, p);
    } else if (pkg.package_type === "UPDATE") {
      updates.push(p);
    } else if (pkg.package_type === "DLC") {
      dlcs.push(p);
    }
  }
  return { base, updates, dlcs };
}

function formatTitle(title, packages) {
  const grouped = groupPackages(packages || []);
  return {
    id: title.id,
    title_id: title.title_id,
    name: title.name,
    description: title.description || "",
    category: title.category || "",
    region: title.region || "",
    cover_url: title.cover_url || "",
    featured: title.featured ? true : false,
    base: grouped.base,
    updates: grouped.updates,
    dlcs: grouped.dlcs
  };
}

// ============================================================
// COVER HELPERS
// ============================================================

const ALLOWED_MIME = {
  "image/jpeg": ".jpg",
  "image/jpg": ".jpg",
  "image/png": ".png",
  "image/webp": ".webp"
};

const MAX_COVER_SIZE = 5 * 1024 * 1024; // 5 MB

function sanitizeTitleId(titleId) {
  if (!titleId || typeof titleId !== "string") return null;
  // Only allow uppercase letters, digits, and hyphens — prevents path traversal
  const sanitized = titleId.replace(/[^A-Za-z0-9-]/g, "").toUpperCase();
  if (!sanitized || sanitized.length > 50) return null;
  return sanitized;
}

// ============================================================
// MAIN HANDLER
// ============================================================

export default {
  async fetch(request, env, ctx) {
    // CORS preflight — handles all OPTIONS including multipart
    if (request.method === "OPTIONS") {
      return new Response(null, { headers: corsHeaders });
    }

    const url = new URL(request.url);
    const path = url.pathname;
    const method = request.method;
    const segments = path.split("/").filter(s => s.length > 0);

    try {
      // ---- PUBLIC: GET /api/catalog ----
      if (segments.length === 2 && segments[0] === "api" && segments[1] === "catalog" && method === "GET") {
        return await handleGetCatalog(env);
      }

      // ---- PUBLIC: GET /api/catalog/version ----
      if (segments.length === 3 && segments[0] === "api" && segments[1] === "catalog" && segments[2] === "version" && method === "GET") {
        return await handleGetCatalogVersion(env);
      }

      // ---- PUBLIC: POST /api/auth/login ----
      if (segments.length === 3 && segments[0] === "api" && segments[1] === "auth" && segments[2] === "login" && method === "POST") {
        return await handleLogin(request, env);
      }

      // ---- PUBLIC: GET /api/titles ----
      if (segments.length === 2 && segments[0] === "api" && segments[1] === "titles" && method === "GET") {
        return await handleListTitles(env);
      }

      // ---- PUBLIC: GET /api/titles/:id/packages ----
      if (segments.length === 4 && segments[0] === "api" && segments[1] === "titles" && segments[3] === "packages" && method === "GET") {
        return await handleGetTitlePackages(env, segments[2]);
      }

      // ---- PUBLIC: GET /api/titles/:id ----
      if (segments.length === 3 && segments[0] === "api" && segments[1] === "titles" && method === "GET") {
        return await handleGetTitle(env, segments[2]);
      }

      // ---- ADMIN: POST /api/titles ----
      if (segments.length === 2 && segments[0] === "api" && segments[1] === "titles" && method === "POST") {
        const auth = await requireAdmin(request, env);
        if (!auth.authorized) return auth.response;
        return await handleCreateTitle(request, env);
      }

      // ---- ADMIN: PUT /api/titles/:id ----
      if (segments.length === 3 && segments[0] === "api" && segments[1] === "titles" && method === "PUT") {
        const auth = await requireAdmin(request, env);
        if (!auth.authorized) return auth.response;
        return await handleUpdateTitle(request, env, segments[2]);
      }

      // ---- ADMIN: DELETE /api/titles/:id ----
      if (segments.length === 3 && segments[0] === "api" && segments[1] === "titles" && method === "DELETE") {
        const auth = await requireAdmin(request, env);
        if (!auth.authorized) return auth.response;
        return await handleDeleteTitle(env, segments[2]);
      }

      // ---- ADMIN: POST /api/titles/:id/packages ----
      if (segments.length === 4 && segments[0] === "api" && segments[1] === "titles" && segments[3] === "packages" && method === "POST") {
        const auth = await requireAdmin(request, env);
        if (!auth.authorized) return auth.response;
        return await handleCreatePackage(request, env, segments[2]);
      }

      // ---- ADMIN: PUT /api/packages/:id ----
      if (segments.length === 3 && segments[0] === "api" && segments[1] === "packages" && method === "PUT") {
        const auth = await requireAdmin(request, env);
        if (!auth.authorized) return auth.response;
        return await handleUpdatePackage(request, env, segments[2]);
      }

      // ---- ADMIN: DELETE /api/packages/:id ----
      if (segments.length === 3 && segments[0] === "api" && segments[1] === "packages" && method === "DELETE") {
        const auth = await requireAdmin(request, env);
        if (!auth.authorized) return auth.response;
        return await handleDeletePackage(env, segments[2]);
      }

      // ---- ADMIN: POST /api/upload/cover ----
      if (segments.length === 3 && segments[0] === "api" && segments[1] === "upload" && segments[2] === "cover" && method === "POST") {
        const auth = await requireAdmin(request, env);
        if (!auth.authorized) return auth.response;
        return await handleUploadCover(request, env, url);
      }

      // ---- ADMIN: DELETE /api/upload/cover/:title_id ----
      if (segments.length === 4 && segments[0] === "api" && segments[1] === "upload" && segments[2] === "cover" && method === "DELETE") {
        const auth = await requireAdmin(request, env);
        if (!auth.authorized) return auth.response;
        return await handleDeleteCover(env, segments[3]);
      }


      // ---- PUBLIC: POST /api/device/pair/start ----
      if (segments.length === 4 && segments[0] === "api" && segments[1] === "device" && segments[2] === "pair" && segments[3] === "start" && method === "POST") {
        return await handleDevicePairStart(request, env);
      }

      // ---- PUBLIC: GET /api/device/pair/status ----
      if (segments.length === 4 && segments[0] === "api" && segments[1] === "device" && segments[2] === "pair" && segments[3] === "status" && method === "GET") {
        return await handleDevicePairStatus(env, url.searchParams.get("device_id") || "");
      }

      // ---- ADMIN: POST /api/admin/devices/pair/confirm ----
      if (segments.length === 5 && segments[0] === "api" && segments[1] === "admin" && segments[2] === "devices" && segments[3] === "pair" && segments[4] === "confirm" && method === "POST") {
        const auth = await requireAdmin(request, env);
        if (!auth.authorized) return auth.response;
        return await handleAdminPairConfirm(request, env);
      }

      // ---- ADMIN: GET /api/admin/devices ----
      if (segments.length === 3 && segments[0] === "api" && segments[1] === "admin" && segments[2] === "devices" && method === "GET") {
        const auth = await requireAdmin(request, env);
        if (!auth.authorized) return auth.response;
        return await handleAdminListDevices(env);
      }

      // ---- ADMIN: POST /api/admin/device/commands ----
      if (segments.length === 4 && segments[0] === "api" && segments[1] === "admin" && segments[2] === "device" && segments[3] === "commands" && method === "POST") {
        const auth = await requireAdmin(request, env);
        if (!auth.authorized) return auth.response;
        return await handleAdminCreateDeviceCommand(request, env);
      }

      // ---- ADMIN: GET /api/admin/device/commands ----
      if (segments.length === 4 && segments[0] === "api" && segments[1] === "admin" && segments[2] === "device" && segments[3] === "commands" && method === "GET") {
        const auth = await requireAdmin(request, env);
        if (!auth.authorized) return auth.response;
        return await handleAdminListDeviceCommands(env, url);
      }

      // ---- ADMIN: POST /api/admin/device/commands/:id/cancel ----
      if (segments.length === 6 && segments[0] === "api" && segments[1] === "admin" && segments[2] === "device" && segments[3] === "commands" && segments[5] === "cancel" && method === "POST") {
        const auth = await requireAdmin(request, env);
        if (!auth.authorized) return auth.response;
        return await handleAdminCancelDeviceCommand(env, segments[4]);
      }

      // ---- DEVICE: GET /api/device/commands ----
      if (segments.length === 3 && segments[0] === "api" && segments[1] === "device" && segments[2] === "commands" && method === "GET") {
        return await handleDeviceCommands(request, env, url);
      }

      // ---- DEVICE: PATCH /api/device/commands/:id ----
      if (segments.length === 4 && segments[0] === "api" && segments[1] === "device" && segments[2] === "commands" && method === "PATCH") {
        return await handleDeviceCommandStatus(request, env, segments[3]);
      }

      // ---- DEVICE: POST /api/device/heartbeat ----
      if (segments.length === 3 && segments[0] === "api" && segments[1] === "device" && segments[2] === "heartbeat" && method === "POST") {
        return await handleDeviceHeartbeat(request, env);
      }

      // ---- PUBLIC: GET /covers/:title_id ----
      if (segments.length === 2 && segments[0] === "covers" && method === "GET") {
        return await handleServeCover(env, segments[1]);
      }

      // ---- 404 ----
      return json({ error: "Not found" }, 404);

    } catch (err) {
      console.error("Worker error:", err);
      return json({ error: "Internal server error" }, 500);
    }
  }
};

// ============================================================
// HANDLER FUNCTIONS — EXISTING ROUTES (PRESERVED)
// ============================================================

async function handleGetCatalog(env) {
  const titles = await env.DB.prepare("SELECT * FROM titles ORDER BY id").all();
  const catalog = [];
  for (const title of titles.results) {
    const packages = await env.DB.prepare("SELECT * FROM packages WHERE title_fk = ? ORDER BY id").bind(title.id).all();
    catalog.push(formatTitle(title, packages.results));
  }
  const meta = await env.DB.prepare("SELECT value FROM catalog_meta WHERE key = 'revision'").first();
  const revision = meta ? parseInt(meta.value) : 1;
  return json({ revision, catalog, message: "Catálogo recuperado com sucesso" });
}

async function handleGetCatalogVersion(env) {
  const meta = await env.DB.prepare("SELECT value FROM catalog_meta WHERE key = 'revision'").first();
  const revision = meta ? parseInt(meta.value) : 1;
  return json({ revision, api_version: "1.1" });
}

async function handleLogin(request, env) {
  const body = await request.json();
  const { email, password } = body;
  if (!email || !password) {
    return json({ error: "Email and password are required" }, 400);
  }
  const admin = await env.DB.prepare("SELECT * FROM admins WHERE email = ?").bind(email).first();
  if (!admin) {
    return json({ error: "Invalid credentials" }, 401);
  }
  // Recalculate PBKDF2 with stored salt
  const saltBytes = new Uint8Array(admin.password_salt.match(/.{2}/g).map(h => parseInt(h, 16)));
  const enc = new TextEncoder();
  const keyMaterial = await crypto.subtle.importKey("raw", enc.encode(password), "PBKDF2", false, ["deriveBits"]);
  const derivedBits = await crypto.subtle.deriveBits(
    { name: "PBKDF2", salt: saltBytes, iterations: 100000, hash: "SHA-256" },
    keyMaterial,
    256
  );
  const computedHash = Array.from(new Uint8Array(derivedBits)).map(b => b.toString(16).padStart(2, "0")).join("");
  if (computedHash !== admin.password_hash) {
    return json({ error: "Invalid credentials" }, 401);
  }
  // Generate session token (HMAC-SHA256 signed with SESSION_SECRET, 24h expiry)
  const payload = { email, exp: Date.now() + 86400000 };
  const payloadB64 = btoa(JSON.stringify(payload));
  const key = await crypto.subtle.importKey("raw", enc.encode(env.SESSION_SECRET), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  const sig = await crypto.subtle.sign("HMAC", key, enc.encode(payloadB64));
  const sigHex = Array.from(new Uint8Array(sig)).map(b => b.toString(16).padStart(2, "0")).join("");
  const token = `${payloadB64}.${sigHex}`;
  return json({ token, expires_in: 86400, message: "Login successful" });
}

async function handleListTitles(env) {
  const titles = await env.DB.prepare("SELECT * FROM titles ORDER BY id").all();
  return json({ titles: titles.results, message: "Titles retrieved successfully" });
}

async function handleGetTitlePackages(env, idStr) {
  const id = parseInt(idStr);
  if (isNaN(id)) {
    return json({ error: "Invalid title ID" }, 400);
  }
  const title = await env.DB.prepare("SELECT * FROM titles WHERE id = ?").bind(id).first();
  if (!title) {
    return json({ error: "Title not found" }, 404);
  }
  const packages = await env.DB.prepare("SELECT * FROM packages WHERE title_fk = ? ORDER BY id").bind(id).all();
  return json({ packages: packages.results, message: "Packages retrieved successfully" });
}

async function handleCreateTitle(request, env) {
  const body = await request.json();
  const { title_id, name, description, category, region, cover_url, featured } = body;
  if (!title_id || !name) {
    return json({ error: "title_id and name are required" }, 400);
  }
  const existing = await env.DB.prepare("SELECT id FROM titles WHERE title_id = ?").bind(title_id).first();
  if (existing) {
    return json({ error: "title_id already exists" }, 409);
  }
  const result = await env.DB.prepare(
    "INSERT INTO titles (title_id, name, description, category, region, cover_url, featured) VALUES (?, ?, ?, ?, ?, ?, ?)"
  ).bind(
    title_id, name, description || "", category || "", region || "", cover_url || "", featured ? 1 : 0
  ).run();
  await bumpRevision(env);
  const title = await env.DB.prepare("SELECT * FROM titles WHERE id = ?").bind(result.meta.last_row_id).first();
  return json({ title, message: "Title created successfully" }, 201);
}

async function handleUpdateTitle(request, env, idStr) {
  const id = parseInt(idStr);
  if (isNaN(id)) {
    return json({ error: "Invalid title ID" }, 400);
  }
  const title = await env.DB.prepare("SELECT * FROM titles WHERE id = ?").bind(id).first();
  if (!title) {
    return json({ error: "Title not found" }, 404);
  }
  const body = await request.json();
  const { title_id, name, description, category, region, cover_url, featured } = body;
  await env.DB.prepare(
    "UPDATE titles SET title_id = ?, name = ?, description = ?, category = ?, region = ?, cover_url = ?, featured = ? WHERE id = ?"
  ).bind(
    title_id || title.title_id,
    name || title.name,
    description !== undefined ? description : title.description,
    category || title.category,
    region || title.region,
    cover_url !== undefined ? cover_url : title.cover_url,
    featured !== undefined ? (featured ? 1 : 0) : title.featured,
    id
  ).run();
  await bumpRevision(env);
  const updated = await env.DB.prepare("SELECT * FROM titles WHERE id = ?").bind(id).first();
  return json({ title: updated, message: "Title updated successfully" });
}

async function handleDeleteTitle(env, idStr) {
  const id = parseInt(idStr);
  if (isNaN(id)) {
    return json({ error: "Invalid title ID" }, 400);
  }
  const title = await env.DB.prepare("SELECT * FROM titles WHERE id = ?").bind(id).first();
  if (!title) {
    return json({ error: "Title not found" }, 404);
  }
  await env.DB.prepare("DELETE FROM packages WHERE title_fk = ?").bind(id).run();
  await env.DB.prepare("DELETE FROM titles WHERE id = ?").bind(id).run();
  await bumpRevision(env);
  return json({ message: "Title deleted successfully" });
}

async function handleCreatePackage(request, env, idStr) {
  const id = parseInt(idStr);
  if (isNaN(id)) {
    return json({ error: "Invalid title ID" }, 400);
  }
  const title = await env.DB.prepare("SELECT * FROM titles WHERE id = ?").bind(id).first();
  if (!title) {
    return json({ error: "Title not found" }, 404);
  }
  const body = await request.json();
  const { package_type, name, version, required_base_version, source_url, size_bytes, sha256 } = body;
  if (!package_type || !["BASE", "UPDATE", "DLC"].includes(package_type)) {
    return json({ error: "package_type must be BASE, UPDATE, or DLC" }, 400);
  }
  if (!name) {
    return json({ error: "name is required" }, 400);
  }
  const result = await env.DB.prepare(
    "INSERT INTO packages (title_fk, package_type, name, version, required_base_version, source_url, size_bytes, sha256) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"
  ).bind(
    id, package_type, name, version || "", required_base_version || "", source_url || "", size_bytes || 0, sha256 || ""
  ).run();
  await bumpRevision(env);
  const pkg = await env.DB.prepare("SELECT * FROM packages WHERE id = ?").bind(result.meta.last_row_id).first();
  return json({ package: pkg, message: "Package created successfully" }, 201);
}

async function handleUpdatePackage(request, env, idStr) {
  const id = parseInt(idStr);
  if (isNaN(id)) {
    return json({ error: "Invalid package ID" }, 400);
  }
  const pkg = await env.DB.prepare("SELECT * FROM packages WHERE id = ?").bind(id).first();
  if (!pkg) {
    return json({ error: "Package not found" }, 404);
  }
  const body = await request.json();
  const { name, version, source_url, size_bytes, sha256, required_base_version } = body;
  await env.DB.prepare(
    "UPDATE packages SET name = ?, version = ?, source_url = ?, size_bytes = ?, sha256 = ?, required_base_version = ? WHERE id = ?"
  ).bind(
    name || pkg.name,
    version || pkg.version,
    source_url !== undefined ? source_url : pkg.source_url,
    size_bytes !== undefined ? size_bytes : pkg.size_bytes,
    sha256 !== undefined ? sha256 : pkg.sha256,
    required_base_version !== undefined ? required_base_version : pkg.required_base_version,
    id
  ).run();
  await bumpRevision(env);
  const updated = await env.DB.prepare("SELECT * FROM packages WHERE id = ?").bind(id).first();
  return json({ package: updated, message: "Package updated successfully" });
}

async function handleDeletePackage(env, idStr) {
  const id = parseInt(idStr);
  if (isNaN(id)) {
    return json({ error: "Invalid package ID" }, 400);
  }
  const pkg = await env.DB.prepare("SELECT * FROM packages WHERE id = ?").bind(id).first();
  if (!pkg) {
    return json({ error: "Package not found" }, 404);
  }
  await env.DB.prepare("DELETE FROM packages WHERE id = ?").bind(id).run();
  await bumpRevision(env);
  return json({ message: "Package deleted successfully" });
}

// ============================================================
// GET /api/titles/:id — Individual title with grouped packages
// ============================================================

async function handleGetTitle(env, idStr) {
  const id = parseInt(idStr);
  if (isNaN(id)) {
    return json({ error: "Invalid title ID" }, 400);
  }
  const title = await env.DB.prepare("SELECT * FROM titles WHERE id = ?").bind(id).first();
  if (!title) {
    return json({ error: "Title not found" }, 404);
  }
  const packages = await env.DB.prepare("SELECT * FROM packages WHERE title_fk = ? ORDER BY id").bind(id).all();
  return json(formatTitle(title, packages.results));
}

// ============================================================
// POST /api/upload/cover — Upload cover image to R2
// ============================================================

async function handleUploadCover(request, env, url) {
  if (!env.COVERS) {
    return json({ error: "R2 binding 'COVERS' is not configured on this Worker" }, 500);
  }

  let formData;
  try {
    formData = await request.formData();
  } catch (e) {
    return json({ error: "Failed to parse multipart/form-data: " + e.message }, 400);
  }

  const file = formData.get("file");
  const titleId = formData.get("title_id");

  if (!file || !titleId) {
    return json({ error: "file and title_id are required" }, 400);
  }

  if (typeof file === "string" || !(file instanceof Blob)) {
    return json({ error: "file must be a valid image upload" }, 400);
  }

  const contentType = file.type;
  if (!ALLOWED_MIME[contentType]) {
    return json({ error: "Invalid file type. Only image/jpeg, image/png, and image/webp are allowed" }, 400);
  }

  if (file.size > MAX_COVER_SIZE) {
    return json({ error: "File too large. Maximum size is 5 MB" }, 400);
  }

  const sanitizedTitleId = sanitizeTitleId(titleId);
  if (!sanitizedTitleId) {
    return json({ error: "Invalid title_id format" }, 400);
  }

  const ext = ALLOWED_MIME[contentType];
  const key = `covers/${sanitizedTitleId}/cover${ext}`;

  // Remove capas anteriores do mesmo title_id para evitar PNG/JPG antigos coexistindo
  const prefix = `covers/${sanitizedTitleId}/`;
  const oldCovers = await env.COVERS.list({ prefix });

  for (const old of oldCovers.objects || []) {
    await env.COVERS.delete(old.key);
  }

  try {
    await env.COVERS.put(key, await file.arrayBuffer(), {
      httpMetadata: { contentType },
    });
  } catch (e) {
    return json({ error: "R2 upload failed: " + e.message }, 500);
  }

  // Usa automaticamente o domínio/origin atual do Worker
  const publicUrl = `${url.origin}/covers/${sanitizedTitleId}`;

  // Mantém o catálogo sincronizado com a capa recém-enviada.
  const title = await env.DB.prepare(
    "SELECT id, cover_url FROM titles WHERE UPPER(title_id) = ?"
  ).bind(sanitizedTitleId).first();

  if (title && title.cover_url !== publicUrl) {
    await env.DB.prepare(
      "UPDATE titles SET cover_url = ? WHERE id = ?"
    ).bind(publicUrl, title.id).run();
    await bumpRevision(env);
  }

  return json({
    url: publicUrl,
    key: key,
    message: "Capa enviada com sucesso"
  }, 201);
}

// ============================================================
// DELETE /api/upload/cover/:title_id — Delete cover from R2
// ============================================================

async function handleDeleteCover(env, titleIdStr) {
  if (!env.COVERS) {
    return json({ error: "R2 binding 'COVERS' is not configured on this Worker" }, 500);
  }

  const sanitizedTitleId = sanitizeTitleId(titleIdStr);
  if (!sanitizedTitleId) {
    return json({ error: "Invalid title_id format" }, 400);
  }

  const prefix = `covers/${sanitizedTitleId}/`;
  let listed;
  try {
    listed = await env.COVERS.list({ prefix });
  } catch (e) {
    return json({ error: "R2 list failed: " + e.message }, 500);
  }

  if (!listed.objects || listed.objects.length === 0) {
    return json({ error: "Cover not found" }, 404);
  }

  for (const obj of listed.objects) {
    await env.COVERS.delete(obj.key);
  }

  const title = await env.DB.prepare(
    "SELECT id, cover_url FROM titles WHERE UPPER(title_id) = ?"
  ).bind(sanitizedTitleId).first();

  if (title && title.cover_url) {
    await env.DB.prepare(
      "UPDATE titles SET cover_url = '' WHERE id = ?"
    ).bind(title.id).run();
    await bumpRevision(env);
  }

  return json({ message: "Cover deleted successfully" });
}

// ============================================================
// GET /covers/:title_id — Serve cover image from R2 (public)
// ============================================================

async function handleServeCover(env, titleIdStr) {
  const sanitizedTitleId = sanitizeTitleId(titleIdStr);
  if (!sanitizedTitleId) {
    return new Response("Not found", { status: 404 });
  }

  if (!env.COVERS) {
    return new Response("R2 binding not configured", { status: 500 });
  }

  const prefix = `covers/${sanitizedTitleId}/`;
  let listed;
  try {
    listed = await env.COVERS.list({ prefix });
  } catch (e) {
    return new Response("Storage error", { status: 500 });
  }

  if (!listed.objects || listed.objects.length === 0) {
    return new Response("Not found", { status: 404 });
  }

  const objKey = listed.objects[0].key;
  const obj = await env.COVERS.get(objKey);
  if (!obj) {
    return new Response("Not found", { status: 404 });
  }

  const headers = new Headers();
  obj.writeHttpMetadata(headers);
  headers.set("Content-Type", obj.httpMetadata?.contentType || "image/jpeg");

  // Cache curto para permitir troca de capa sem ficar preso por 1 ano
  headers.set("Cache-Control", "public, max-age=300");

  headers.set("Access-Control-Allow-Origin", "*");
  headers.set("ETag", obj.httpEtag || "");

  return new Response(obj.body, { headers });
}

// ============================================================
// REMOTE PS4 PAIRING / COMMAND QUEUE
// Integrated from remote_worker_patch.js
// ============================================================

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
  "CANCEL_REQUESTED",
  "CANCELLED"
]);

async function ensureRemoteSchema(env) {
  await env.DB.batch([
    env.DB.prepare(`
      CREATE TABLE IF NOT EXISTS devices (
        device_id TEXT PRIMARY KEY,
        device_name TEXT NOT NULL,
        token_hash TEXT NOT NULL,
        paired INTEGER NOT NULL DEFAULT 0,
        enabled INTEGER NOT NULL DEFAULT 1,
        last_seen TEXT,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        paired_at TEXT
      )`
    ),
    env.DB.prepare(`
      CREATE TABLE IF NOT EXISTS device_pairings (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        device_id TEXT NOT NULL,
        code_hash TEXT NOT NULL,
        expires_at TEXT NOT NULL,
        consumed INTEGER NOT NULL DEFAULT 0,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY(device_id) REFERENCES devices(device_id) ON DELETE CASCADE
      )`
    ),
    env.DB.prepare(`
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
      )`
    ),
    env.DB.prepare(
      "CREATE INDEX IF NOT EXISTS idx_pairings_code ON device_pairings(code_hash, consumed)"
    ),
    env.DB.prepare(
      "CREATE INDEX IF NOT EXISTS idx_device_commands_poll ON device_commands(device_id, status, id)"
    )
  ]);
}

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
  await ensureRemoteSchema(env);
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
  await ensureRemoteSchema(env);
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
  await ensureRemoteSchema(env);
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
  await ensureRemoteSchema(env);
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
  await ensureRemoteSchema(env);
  const result = await env.DB.prepare(
    `SELECT device_id, device_name, paired, enabled, last_seen, created_at, paired_at
     FROM devices
     ORDER BY COALESCE(last_seen, created_at) DESC`
  ).all();

  return json({ devices: result.results || [] });
}

// ADMIN: sends one command to a paired PS4.
async function handleAdminCreateDeviceCommand(request, env) {
  await ensureRemoteSchema(env);
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

// ADMIN: reads recent command history/progress for one or all consoles.
async function handleAdminListDeviceCommands(env, url) {
  await ensureRemoteSchema(env);

  const deviceId = url.searchParams.get("device_id");
  const limitRaw = Number(url.searchParams.get("limit") || 20);
  const limit = Math.max(1, Math.min(100, Number.isFinite(limitRaw) ? Math.floor(limitRaw) : 20));

  const result = deviceId
    ? await env.DB.prepare(
        `SELECT id, device_id, action, title_db_id, title_id, package_ids_json,
                status, progress, message, created_at, started_at, completed_at, updated_at
           FROM device_commands
           WHERE device_id = ?
           ORDER BY id DESC
           LIMIT ?`
      ).bind(deviceId, limit).all()
    : await env.DB.prepare(
        `SELECT id, device_id, action, title_db_id, title_id, package_ids_json,
                status, progress, message, created_at, started_at, completed_at, updated_at
           FROM device_commands
           ORDER BY id DESC
           LIMIT ?`
      ).bind(limit).all();

  const commands = (result.results || []).map(row => ({
    ...row,
    package_ids: (() => {
      try { return JSON.parse(row.package_ids_json || "[]"); }
      catch { return []; }
    })()
  }));

  return json({ commands });
}

// ADMIN: requests cancellation of an in-flight command.
async function handleAdminCancelDeviceCommand(env, commandIdStr) {
  await ensureRemoteSchema(env);

  const id = Number(commandIdStr);
  if (!Number.isInteger(id) || id <= 0) {
    return json({ error: "Invalid command id" }, 400);
  }

  const row = await env.DB.prepare(
    "SELECT id, status FROM device_commands WHERE id = ?"
  ).bind(id).first();

  if (!row) return json({ error: "Command not found" }, 404);

  if (["COMPLETED", "ERROR", "CANCELLED"].includes(row.status)) {
    return json({
      id,
      status: row.status,
      message: "Command is already finished"
    });
  }

  await env.DB.prepare(
    `UPDATE device_commands
       SET status = 'CANCEL_REQUESTED',
           message = 'Cancelamento solicitado pelo aplicativo',
           updated_at = CURRENT_TIMESTAMP
       WHERE id = ?`
  ).bind(id).run();

  return json({
    id,
    status: "CANCEL_REQUESTED",
    message: "Cancellation requested"
  });
}

// DEVICE: fetches pending/current commands.
async function handleDeviceCommands(request, env, url) {
  await ensureRemoteSchema(env);
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
         AND status IN ('QUEUED','ACCEPTED','DOWNLOADING','VERIFYING','INSTALLING','CANCEL_REQUESTED')
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
  await ensureRemoteSchema(env);
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
  await ensureRemoteSchema(env);
  const body = await request.json();
  const deviceId = String(body.device_id || "");
  const auth = await requireDevice(request, env, deviceId);
  if (!auth.authorized) return auth.response;

  await env.DB.prepare(
    "UPDATE devices SET last_seen = CURRENT_TIMESTAMP WHERE device_id = ?"
  ).bind(deviceId).run();

  return json({ ok: true });
}
