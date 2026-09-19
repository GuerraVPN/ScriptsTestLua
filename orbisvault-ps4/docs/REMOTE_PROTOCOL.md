# Orbis Vault — Protocolo remoto Android ↔ PS4

## Princípio

O APK Android continua sendo o administrador.
O PS4 recebe somente um token exclusivo de dispositivo.

Nenhuma senha de administrador, `SESSION_SECRET` ou credencial Cloudflare é enviada ao console.

## Pareamento

1. PS4 gera apenas um `device_id` persistente e envia ao Worker junto com o nome do console.
2. Worker gera um `device_token` aleatório e um código de 6 dígitos.
3. O Worker grava somente SHA-256(token) e SHA-256(código) no D1.
4. Token e código em texto puro são devolvidos uma única vez ao PS4 pela conexão HTTPS.
5. PS4 salva o token localmente e mostra o código na TV.
6. Usuário abre Orbis Vault Admin e informa o código.
7. Admin confirma via rota protegida por Bearer admin.
8. Worker marca o device como pareado.
9. Nas chamadas seguintes, o PS4 usa o device token como Bearer.

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
