# Orbis Vault PS4 — v0.1

Cliente PS4 do ecossistema Orbis Vault. O aplicativo do console não usa login
administrativo: ele lê o catálogo público, mantém cache local e pode ser
pareado com o Orbis Vault Admin por um token exclusivo do dispositivo.

API:
`https://orbis-vault-api.guerraf1000.workers.dev`

## Implementado

- interface gráfica SDL2 em 1920x1080;
- catálogo em grade com capas;
- tela de detalhes;
- cache do catálogo por `revision`;
- cache de capas;
- modo offline com o último catálogo válido;
- download HTTPS para armazenamento local;
- retomada por HTTP Range quando o servidor oferece suporte;
- verificação SHA-256 quando o hash está cadastrado;
- pipeline Base -> Update mais recente -> DLCs;
- adaptador de instalação nativa;
- detecção de título instalado;
- botão **Abrir jogo** para títulos já instalados;
- tela de downloads e progresso;
- pareamento PS4 <-> Android por código de 6 dígitos;
- fila remota via Cloudflare;
- heartbeat e status remoto;
- cancelamento remoto durante download;
- build PKG automatizada em GitHub Actions com OpenOrbis v0.5.4;
- validação automática da sintaxe do Worker integrado.

## Backend remoto

O Worker integrado está em:

`backend/orbis-vault-worker-v04.js`

Ele preserva catálogo, autenticação administrativa e capas, e acrescenta:

- dispositivos pareados;
- códigos de pareamento temporários;
- fila de comandos;
- progresso remoto;
- cancelamento;
- heartbeat;
- criação automática das tabelas D1 necessárias.

O Worker ao vivo precisa ser atualizado com esse arquivo para que pareamento e
instalação remota funcionem.

## Fluxo no PS4

```
Abrir Orbis Vault
       |
       v
Sincronizar revision/catalog
       |
       v
Grade de títulos
       |
       +----> X -> Detalhes
                    |
                    +----> Baixar / instalar
                    |
                    +----> Abrir, quando já instalado
```

Durante uma instalação:

```
DOWNLOAD -> VERIFICACAO -> INSTALACAO -> CONCLUIDO
```

## Fluxo remoto

```
Android Admin
     |
     | HTTPS + admin Bearer
     v
Cloudflare Worker + D1
     |
     | token exclusivo do PS4
     v
Orbis Vault PS4
     |
     +--> sincroniza catálogo
     +--> baixa
     +--> verifica
     +--> instala
     +--> devolve status/progresso
```

## Segurança

- o PS4 não recebe `SESSION_SECRET`;
- o PS4 não recebe senha de administrador;
- o token do dispositivo é armazenado no D1 somente como SHA-256;
- o código de pareamento expira;
- comandos remotos aceitos são enumerados;
- não existe execução arbitrária de shell/comandos remotos.

## Teste em hardware

A compilação e empacotamento estão passando no CI. A etapa seguinte é validar
no console real: inicialização SDL, acesso HTTPS, permissões de `/data`,
AppInstUtil, detecção de títulos e lançamento via SystemService.

O projeto é destinado a homebrew, backups próprios e pacotes que o usuário
tenha autorização para instalar/distribuir. Não contém bypass de DRM.
