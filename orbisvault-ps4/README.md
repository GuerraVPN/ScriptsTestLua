# Orbis Vault PS4 — v0.1 scaffold

Cliente PS4 do Orbis Vault, separado do APK Android Admin.

## Objetivo

- Sem login de administrador no console.
- Sincronizar o catálogo público do Cloudflare.
- Cache local por `revision`.
- Mostrar títulos, capa, Base, Updates e DLCs.
- Fila de downloads.
- Instalação local de pacotes autorizados após download.
- Pareamento com o app Android para comandos remotos.
- Status remoto: QUEUED, DOWNLOADING, VERIFYING, INSTALLING, COMPLETED, ERROR.

API pública atual:

`https://orbis-vault-api.guerraf1000.workers.dev`

## Segurança

O PS4 nunca recebe `SESSION_SECRET` nem credenciais de administrador.
O pareamento remoto usa um token exclusivo do console.

## Conteúdo

Este projeto foi estruturado para homebrew, backups próprios e pacotes que o usuário tenha autorização para instalar/distribuir. Não contém bypass de DRM nem fontes de conteúdo comercial.

## Estado da v0.1

Implementado nesta base:
- arquitetura do cliente;
- cliente HTTP;
- modelo de catálogo;
- cache/revision;
- gerenciador de fila;
- adaptador de instalação;
- protocolo de pareamento/fila remota;
- migration SQL para Cloudflare D1.

Próximos marcos:
1. parser JSON do catálogo;
2. UI gráfica em grade;
3. download com retomada;
4. integração do adaptador de instalação no ambiente OpenOrbis;
5. pareamento Android ↔ PS4;
6. build PKG e teste real no console.
