# Integrando o remote_worker_patch.js ao Worker atual

Depois de executar `backend/remote_schema.sql` no D1, copie as funções de
`remote_worker_patch.js` para o Worker atual.

No `fetch()`, após criar `url`, `segments` e `method`, adicione estas rotas
antes do 404 final.

```js
// PUBLIC: PS4 starts pairing
if (segments.length === 4 &&
    segments[0] === "api" &&
    segments[1] === "device" &&
    segments[2] === "pair" &&
    segments[3] === "start" &&
    method === "POST") {
  return await handleDevicePairStart(request, env);
}

// PUBLIC: PS4 checks pairing status
if (segments.length === 4 &&
    segments[0] === "api" &&
    segments[1] === "device" &&
    segments[2] === "pair" &&
    segments[3] === "status" &&
    method === "GET") {
  return await handleDevicePairStatus(
    env,
    url.searchParams.get("device_id") || ""
  );
}

// ADMIN: confirm pairing code
if (segments.length === 5 &&
    segments[0] === "api" &&
    segments[1] === "admin" &&
    segments[2] === "devices" &&
    segments[3] === "pair" &&
    segments[4] === "confirm" &&
    method === "POST") {
  const auth = await requireAdmin(request, env);
  if (!auth.authorized) return auth.response;
  return await handleAdminPairConfirm(request, env);
}

// ADMIN: list paired devices
if (segments.length === 3 &&
    segments[0] === "api" &&
    segments[1] === "admin" &&
    segments[2] === "devices" &&
    method === "GET") {
  const auth = await requireAdmin(request, env);
  if (!auth.authorized) return auth.response;
  return await handleAdminListDevices(env);
}

// ADMIN: queue install/sync command
if (segments.length === 4 &&
    segments[0] === "api" &&
    segments[1] === "admin" &&
    segments[2] === "device" &&
    segments[3] === "commands" &&
    method === "POST") {
  const auth = await requireAdmin(request, env);
  if (!auth.authorized) return auth.response;
  return await handleAdminCreateDeviceCommand(request, env);
}

// DEVICE: poll commands
if (segments.length === 3 &&
    segments[0] === "api" &&
    segments[1] === "device" &&
    segments[2] === "commands" &&
    method === "GET") {
  return await handleDeviceCommands(request, env, url);
}

// DEVICE: report command status
if (segments.length === 4 &&
    segments[0] === "api" &&
    segments[1] === "device" &&
    segments[2] === "commands" &&
    method === "PATCH") {
  return await handleDeviceCommandStatus(request, env, segments[3]);
}

// DEVICE: heartbeat
if (segments.length === 3 &&
    segments[0] === "api" &&
    segments[1] === "device" &&
    segments[2] === "heartbeat" &&
    method === "POST") {
  return await handleDeviceHeartbeat(request, env);
}
```

## Observação importante

O `device_token` nunca deve ser enviado para o Android Admin e nunca deve ser
armazenado no D1 em texto puro. O Worker guarda apenas SHA-256(token).

O código de pareamento expira em 10 minutos.

## Exemplo de comando remoto criado pelo Android Admin

```json
{
  "device_id": "ps4_4F8A...",
  "action": "INSTALL_SELECTED",
  "title_db_id": 12,
  "title_id": "BREW00001",
  "package_ids": [31, 32, 35]
}
```

Para `INSTALL_ALL`, `package_ids` pode ficar vazio: o PS4 sincroniza o catálogo
e resolve Base/Update/DLC do título antes de iniciar.
