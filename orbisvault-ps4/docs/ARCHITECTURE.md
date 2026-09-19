# Arquitetura Orbis Vault PS4

```
Android Admin
    |
    | HTTPS + admin Bearer
    v
Cloudflare Worker
    |-- D1: catálogo / devices / fila remota
    |-- R2: capas
    |
    | HTTPS público + device token
    v
Orbis Vault PS4
    |-- catálogo/cache
    |-- fila de downloads
    |-- verificação
    |-- adaptador de instalação
    v
Sistema do PS4
```

## Componentes

### CatalogClient
Consulta `/api/catalog/version`. Só busca o catálogo quando a revision mudou.

### DownloadManager
Download HTTPS para armazenamento local com retomada HTTP quando suportada.

### Installer
Camada separada do catálogo e do download. Isto permite trocar a implementação entre AppInstUtil/BGFT conforme o ambiente real do console sem espalhar chamadas específicas pela aplicação.

### RemoteQueueClient
Poll da fila Cloudflare e atualização do progresso.

### UI
Planejada como grid de capas + tela de detalhes + downloads + configurações/pareamento.

## Instalação "como jogo normal"

A meta é que, após a instalação de um pacote autorizado pelo adaptador nativo, o título seja gerenciado pelo próprio sistema do console e apareça na interface do PS4. A aplicação não implementa bypass de licença/DRM.
