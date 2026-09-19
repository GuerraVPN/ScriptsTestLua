#include "catalog_client.hpp"
#include "config.hpp"
#include "cover_cache.hpp"
#include "http_client.hpp"
#include "install_coordinator.hpp"
#include "installer.hpp"
#include "native_http.hpp"
#include "pairing_client.hpp"
#include "remote_queue.hpp"
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
    PairingClient pairing(http);
    RemoteQueueClient remote(http);

    DeviceIdentity device;
    pairing.load(device);

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
    uiStatus.remotePaired = device.paired;
    uiStatus.remoteDeviceName = device.deviceName;

    if (!installerReady)
        uiStatus.message = "INSTALADOR NATIVO INDISPONIVEL NESTA BUILD";
    ui.setStatus(uiStatus);

    uint32_t lastPairCheck = 0;
    uint32_t lastRemotePoll = 0;
    uint32_t lastRemoteReport = 0;
    long activeRemoteCommand = 0;
    int lastReportedProgress = -1;
    std::string lastReportedStage;

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
            if (device.paired) {
                uiStatus.message = "ESTE PS4 JA ESTA PAREADO";
            } else {
                PairingResult result = pairing.start(device, "Orbis Vault PS4");
                if (result.ok) {
                    uiStatus.pairingCode = result.code;
                    uiStatus.remoteDeviceName = device.deviceName;
                    uiStatus.message = "DIGITE O CODIGO NO APP ANDROID";
                    lastPairCheck = SDL_GetTicks();
                } else {
                    uiStatus.message = "FALHA NO PAREAMENTO: " + result.error;
                }
            }
        }

        const uint32_t now = SDL_GetTicks();

        // While waiting for confirmation on Android, refresh pairing status.
        if (!device.paired &&
            !device.deviceId.empty() &&
            !uiStatus.pairingCode.empty() &&
            now - lastPairCheck >= 3000) {
            std::string pairError;
            if (pairing.refreshStatus(device, pairError) && device.paired) {
                uiStatus.remotePaired = true;
                uiStatus.pairingCode.clear();
                uiStatus.message = "PS4 PAREADO COM SUCESSO";
            }
            lastPairCheck = now;
        }

        // Poll remote install queue only when no local/remote job is running.
        InstallSnapshot beforeRemote = coordinator.snapshot();
        if (device.paired &&
            !beforeRemote.active &&
            activeRemoteCommand == 0 &&
            now - lastRemotePoll >= static_cast<uint32_t>(REMOTE_POLL_SECONDS * 1000)) {
            std::vector<RemoteCommand> commands;
            std::string remoteError;

            if (remote.poll(device, commands, remoteError)) {
                if (!commands.empty()) {
                    const RemoteCommand& cmd = commands.front();

                    const TitleItem* selected = nullptr;
                    for (const auto& t : catalog.titles) {
                        if ((cmd.titleDbId > 0 && t.id == cmd.titleDbId) ||
                            (!cmd.titleId.empty() && t.titleId == cmd.titleId)) {
                            selected = &t;
                            break;
                        }
                    }

                    if (cmd.action == "SYNC_CATALOG") {
                        syncCatalog(catalogClient, coverCache, catalog, uiStatus);
                        ui.setCatalog(&catalog);
                        remote.updateStatus(device, cmd.id, "COMPLETED", 100,
                                            "Catalogo sincronizado", remoteError);
                    } else if ((cmd.action == "INSTALL_ALL" ||
                                cmd.action == "INSTALL_SELECTED") &&
                               selected) {
                        if (coordinator.start(*selected)) {
                            activeRemoteCommand = cmd.id;
                            lastReportedProgress = -1;
                            lastReportedStage.clear();
                            remote.updateStatus(device, cmd.id, "ACCEPTED", 0,
                                                "Comando aceito pelo PS4", remoteError);
                            uiStatus.message = "INSTALACAO REMOTA RECEBIDA";
                        }
                    } else {
                        remote.updateStatus(device, cmd.id, "ERROR", 0,
                                            "Comando ou titulo invalido", remoteError);
                    }
                } else {
                    remote.heartbeat(device, remoteError);
                }
            }
            lastRemotePoll = now;
        }

        const InstallSnapshot job = coordinator.snapshot();
        uiStatus.jobTitle = job.title;
        uiStatus.jobStage = job.stage;
        uiStatus.jobProgress = job.progress;

        if (job.failed && !job.error.empty())
            uiStatus.message = job.error;
        else if (job.completed)
            uiStatus.message = "INSTALACAO CONCLUIDA";

        if (activeRemoteCommand != 0 &&
            (job.progress != lastReportedProgress ||
             job.stage != lastReportedStage ||
             now - lastRemoteReport >= 5000)) {
            std::string remoteState = "DOWNLOADING";
            if (job.stage.find("VERIFICANDO") == 0) remoteState = "VERIFYING";
            else if (job.stage.find("INSTALANDO") == 0) remoteState = "INSTALLING";
            else if (job.completed) remoteState = "COMPLETED";
            else if (job.failed) remoteState = "ERROR";

            std::string reportError;
            const std::string reportMessage =
                job.failed && !job.error.empty() ? job.error : job.stage;

            remote.updateStatus(device, activeRemoteCommand, remoteState,
                                job.progress, reportMessage, reportError);

            lastReportedProgress = job.progress;
            lastReportedStage = job.stage;
            lastRemoteReport = now;

            if (job.completed || job.failed) {
                activeRemoteCommand = 0;
                lastRemotePoll = 0;
            }
        }

        uiStatus.remotePaired = device.paired;
        uiStatus.remoteDeviceName = device.deviceName;
        ui.setStatus(uiStatus);
        ui.render();
        SDL_Delay(16);
    }

    coordinator.cancel();
    installer.shutdown();
    shutdownNativeHttp();
    return 0;
}
