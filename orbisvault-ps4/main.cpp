#include "catalog_client.hpp"
#include "config.hpp"
#include "cover_cache.hpp"
#include "http_client.hpp"
#include "install_coordinator.hpp"
#include "installer.hpp"
#include "native_http.hpp"
#include "ui.hpp"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace ov;

static void ensureDataDirs() {
    mkdir(DATA_DIR, 0777);
    const std::string covers = std::string(DATA_DIR) + "/covers";
    mkdir(covers.c_str(), 0777);
    mkdir(DOWNLOAD_DIR, 0777);
}

static bool syncCatalog(CatalogClient& client,
                        CoverCache& covers,
                        Catalog& catalog,
                        UiRuntimeStatus& status) {
    std::string error;
    int revision = 0;

    if (!client.fetchRevision(revision, error)) {
        status.online = false;
        status.message = "SEM SERVIDOR - USANDO CACHE";
        error.clear();
        if (client.loadCached(catalog, error)) {
            status.revision = catalog.revision;
            return true;
        }
        status.message = "CATALOGO INDISPONIVEL";
        return false;
    }

    status.online = true;

    // Avoid downloading the full catalog if the raw cached revision is current.
    Catalog cached;
    std::string cacheError;
    if (client.loadCached(cached, cacheError) &&
        cached.revision == revision &&
        !cached.titles.empty()) {
        catalog = cached;
        status.revision = revision;
        status.message = "CATALOGO ATUALIZADO";
        covers.sync(catalog);
        return true;
    }

    error.clear();
    if (!client.fetchCatalog(catalog, error)) {
        status.message = "ERRO AO SINCRONIZAR CATALOGO";
        return false;
    }

    status.revision = catalog.revision;
    status.message = "SINCRONIZADO COM CLOUDFLARE";
    covers.sync(catalog);
    return true;
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    ensureDataDirs();

    HttpClient http;
    CatalogClient catalogClient(http);
    CoverCache coverCache(http);

    Catalog catalog;
    UiRuntimeStatus uiStatus;
    syncCatalog(catalogClient, coverCache, catalog, uiStatus);

    Installer installer;
    const bool installerReady = installer.initialize();

    InstallCoordinator coordinator(installer);

    AppUi ui;
    if (!ui.initialize()) {
        printf("Orbis Vault: SDL initialization failed\n");
        return 1;
    }

    ui.setCatalog(&catalog);
    if (!installerReady)
        uiStatus.message = "INSTALADOR NATIVO INDISPONIVEL NESTA BUILD";
    ui.setStatus(uiStatus);

    while (ui.running()) {
        const UiAction action = ui.update();

        if (action.type == UiActionType::Exit) {
            break;
        }

        if (action.type == UiActionType::RefreshCatalog) {
            InstallSnapshot job = coordinator.snapshot();
            if (!job.active) {
                syncCatalog(catalogClient, coverCache, catalog, uiStatus);
                ui.setCatalog(&catalog);
            } else {
                uiStatus.message = "AGUARDE O DOWNLOAD ATIVO";
            }
        }

        if (action.type == UiActionType::InstallSelected &&
            action.titleIndex >= 0 &&
            action.titleIndex < static_cast<int>(catalog.titles.size())) {
            if (!installerReady) {
                uiStatus.message = "INSTALADOR NATIVO NAO INICIALIZADO";
            } else if (coordinator.start(catalog.titles[action.titleIndex])) {
                uiStatus.message = "INSTALACAO ADICIONADA";
            } else {
                uiStatus.message = "JA EXISTE UMA INSTALACAO ATIVA";
            }
        }

        if (action.type == UiActionType::StartPairing) {
            // Pairing transport is the next runtime milestone. The Worker/D1
            // protocol is already implemented in backend/remote_worker_patch.js.
            uiStatus.message = "PAREAMENTO: PUBLIQUE A API REMOTA NO CLOUDFLARE";
        }

        const InstallSnapshot job = coordinator.snapshot();
        uiStatus.jobTitle = job.title;
        uiStatus.jobStage = job.stage;
        uiStatus.jobProgress = job.progress;

        if (job.failed && !job.error.empty())
            uiStatus.message = job.error;
        else if (job.completed)
            uiStatus.message = "INSTALACAO CONCLUIDA";

        ui.setStatus(uiStatus);
        ui.render();
        SDL_Delay(16);
    }

    coordinator.cancel();
    installer.shutdown();
    shutdownNativeHttp();
    return 0;
}
