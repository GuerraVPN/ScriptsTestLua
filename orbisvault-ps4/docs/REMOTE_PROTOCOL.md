# Orbis Vault — Protocolo remoto Android ↔ PS4

## Princípio

O APK Android continua sendo o administrador.
O PS4 recebe somente um token exclusivo de dispositivo.

Nenhuma senha de administrador, `SESSION_SECRET` ou credencial Cloudflare é enviada ao console.

## Pareamento

1. PS4 gera localmente:
   - `device_id` aleatório;
   - `device_token` aleatório de alta entropia;
   - código de 6 dígitos para pareamento.
2. PS4 envia ao Worker:
   - device_id;
   - nome do console;
   - SHA-256(device_token);
   - SHA-256(código);
3. PS4 mostra o código na TV.
4. Usuário abre Orbis Vault Admin e informa o código.
5. Admin confirma via rota protegida por Bearer admin.
6. Worker marca o device como pareado.
7. PS4 continua usando o token que já gerou localmente.

## Rotas planejadas

### PS4
- `POST /api/device/pair/start`
- `GET /api/device/pair/status?device_id=...`
- `GET /api/device/commands?device_id=...`
- `PATCH /api/device/commands/:id`
- `POST /api/device/heartbeat`

As rotas de device, exceto pair/start e pair/status pendente, validam:
`Authorization: Bearer <device_token>`.

### Android Admin
- `GET /api/admin/devices`
- `POST /api/admin/devices/pair/confirm`
- `POST /api/admin/device/commands`
- `DELETE /api/admin/device/commands/:id`

Todas exigem o token de administrador atual.

## Ações

- `INSTALL_SELECTED`
- `INSTALL_ALL`
- `CANCEL_COMMAND`
- `SYNC_CATALOG`

A primeira versão não envia comando para executar arbitrariamente arquivos nem comandos shell.

## Estados

- QUEUED
- ACCEPTED
- DOWNLOADING
- VERIFYING
- INSTALLING
- COMPLETED
- ERROR
- CANCELLED

## Regra de instalação

Para um título:
1. BASE;
2. UPDATE selecionado/compatível;
3. DLCs selecionados.

O PS4 reporta progresso ao Worker para o Android exibir mesmo fora da rede local.
