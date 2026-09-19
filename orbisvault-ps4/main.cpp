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

static void refreshInstalledState(Installer& installer, Catalog& catalog) {
    for (auto& title : catalog.titles) {
        title.installed = installer.isInstalled(title.titleId);
    }
}

static TitleItem filterTitlePackages(const TitleItem& source,
                                     const std::vector<int>& packageIds) {
    if (packageIds.empty()) return source;

    TitleItem out = source;
    out.hasBase = false;
    out.updates.clear();
    out.dlcs.clear();

    auto selected = [&packageIds](int id) {
        for (int wanted : packageIds)
            if (wanted == id) return true;
        return false;
    };

    if (source.hasBase && selected(source.base.id)) {
        out.base = source.base;
        out.hasBase = true;
        // Explicit Base selection means reinstall Base even when the title
        // already exists on the console.
        out.installed = false;
    }

    for (const auto& p : source.updates)
        if (selected(p.id)) out.updates.push_back(p);

    for (const auto& p : source.dlcs)
        if (selected(p.id)) out.dlcs.push_back(p);

    return out;
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
    if (installerReady) refreshInstalledState(installer, catalog);

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
    std::string installedRefreshFor;

    while (ui.running()) {
        const UiAction action = ui.update();

        if (action.type == UiActionType::Exit) {
            break;
        }

        if (action.type == UiActionType::RefreshCatalog) {
            InstallSnapshot job = coordinator.snapshot();
            if (!job.active) {
                syncCatalog(catalogClient, coverCache, catalog, uiStatus);
                if (installerReady) refreshInstalledState(installer, catalog);
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
                installedRefreshFor.clear();
                uiStatus.message = "INSTALACAO ADICIONADA";
            } else {
                uiStatus.message = "JA EXISTE UMA INSTALACAO ATIVA";
            }
        }

        if (action.type == UiActionType::CancelInstall) {
            InstallSnapshot activeJob = coordinator.snapshot();
            if (activeJob.active) {
                coordinator.cancel();
                uiStatus.message = "CANCELANDO DOWNLOAD";
            } else {
                uiStatus.message = "NENHUM DOWNLOAD ATIVO";
            }
        }

        if (action.type == UiActionType::OpenSelected &&
            action.titleIndex >= 0 &&
            action.titleIndex < static_cast<int>(catalog.titles.size())) {
            InstallResult opened =
                installer.launchTitle(catalog.titles[action.titleIndex].titleId);
            uiStatus.message = opened.message;
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

        // Poll every 3 seconds during a remote job so cancellation from the
        // Android app is noticed quickly; otherwise use the normal interval.
        InstallSnapshot beforeRemote = coordinator.snapshot();
        const uint32_t remoteInterval =
            activeRemoteCommand != 0
                ? 3000U
                : static_cast<uint32_t>(REMOTE_POLL_SECONDS * 1000);

        if (device.paired && now - lastRemotePoll >= remoteInterval) {
            std::vector<RemoteCommand> commands;
            std::string remoteError;

            if (remote.poll(device, commands, remoteError)) {
                bool handled = false;

                if (activeRemoteCommand != 0) {
                    for (const auto& cmd : commands) {
                        if (cmd.id == activeRemoteCommand &&
                            cmd.status == "CANCEL_REQUESTED") {
                            coordinator.cancel();
                            uiStatus.message = "CANCELANDO INSTALACAO REMOTA";
                            handled = true;
                            break;
                        }
                    }
                }

                if (!beforeRemote.active && activeRemoteCommand == 0) {
                    for (const auto& cmd : commands) {
                        if (cmd.status == "CANCEL_REQUESTED") {
                            remote.updateStatus(device, cmd.id, "CANCELLED", 0,
                                                "Nada em execucao para cancelar",
                                                remoteError);
                            handled = true;
                            continue;
                        }

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
                            if (installerReady) refreshInstalledState(installer, catalog);
                            ui.setCatalog(&catalog);
                            remote.updateStatus(device, cmd.id, "COMPLETED", 100,
                                                "Catalogo sincronizado", remoteError);
                            handled = true;
                            break;
                        }

                        if ((cmd.action == "INSTALL_ALL" ||
                             cmd.action == "INSTALL_SELECTED") &&
                            selected) {
                            TitleItem installTitle =
                                cmd.action == "INSTALL_SELECTED"
                                    ? filterTitlePackages(*selected, cmd.packageIds)
                                    : *selected;

                            const bool hasSelection =
                                installTitle.hasBase ||
                                !installTitle.updates.empty() ||
                                !installTitle.dlcs.empty();

                            if (!hasSelection && cmd.action == "INSTALL_SELECTED") {
                                remote.updateStatus(device, cmd.id, "ERROR", 0,
                                                    "Nenhum pacote valido selecionado",
                                                    remoteError);
                                handled = true;
                                break;
                            }

                            if (coordinator.start(installTitle)) {
                                installedRefreshFor.clear();
                                activeRemoteCommand = cmd.id;
                                lastReportedProgress = -1;
                                lastReportedStage.clear();
                                remote.updateStatus(device, cmd.id, "ACCEPTED", 0,
                                                    "Comando aceito pelo PS4",
                                                    remoteError);
                                uiStatus.message = "INSTALACAO REMOTA RECEBIDA";
                                handled = true;
                                break;
                            }
                        } else if (cmd.status == "QUEUED") {
                            remote.updateStatus(device, cmd.id, "ERROR", 0,
                                                "Comando ou titulo invalido",
                                                remoteError);
                            handled = true;
                            break;
                        }
                    }
                }

                if (!handled && commands.empty()) {
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
        else if (job.completed) {
            uiStatus.message = "INSTALACAO CONCLUIDA";
            if (!job.titleId.empty() && installedRefreshFor != job.titleId) {
                refreshInstalledState(installer, catalog);
                installedRefreshFor = job.titleId;
            }
        }

        if (activeRemoteCommand != 0 &&
            (job.progress != lastReportedProgress ||
             job.stage != lastReportedStage ||
             now - lastRemoteReport >= 5000)) {
            std::string remoteState = "DOWNLOADING";
            if (job.stage.find("VERIFICANDO") == 0) remoteState = "VERIFYING";
            else if (job.stage.find("INSTALANDO") == 0) remoteState = "INSTALLING";
            else if (job.completed) remoteState = "COMPLETED";
            else if (job.stage == "CANCELADO") remoteState = "CANCELLED";
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
