#include "catalog_client.hpp"
#include "config.hpp"
#include "download_manager.hpp"
#include "http_client.hpp"
#include "installer.hpp"
#include "remote_queue.hpp"
#include <stdio.h>
#include <unistd.h>

using namespace ov;

static void banner() {
    printf("\n=====================================\n");
    printf("          ORBIS VAULT PS4 v0.1        \n");
    printf("=====================================\n");
    printf("Servidor: %s\n\n", API_BASE);
}

int main() {
    banner();

    HttpClient http;
    CatalogClient catalogClient(http);
    Installer installer;
    RemoteQueueClient remote(http);

    Catalog catalog;
    std::string error;

    int remoteRevision = 0;
    if (catalogClient.fetchRevision(remoteRevision, error)) {
        printf("[SYNC] Revision remota: %d\n", remoteRevision);
    } else {
        printf("[SYNC] Falha ao consultar revision: %s\n", error.c_str());
    }

    error.clear();
    if (catalogClient.fetchCatalog(catalog, error)) {
        printf("[SYNC] Catalogo sincronizado. Revision %d\n", catalog.revision);
        printf("[SYNC] Titulos parseados: %zu\n", catalog.titles.size());
    } else {
        printf("[SYNC] Falha: %s\n", error.c_str());
        printf("[SYNC] Tentando cache local...\n");
        error.clear();
        if (catalogClient.loadCached(catalog, error)) {
            printf("[SYNC] Cache carregado. Revision %d\n", catalog.revision);
        } else {
            printf("[SYNC] Cache indisponivel: %s\n", error.c_str());
        }
    }

    if (!installer.initialize()) {
        printf("[INSTALL] Adaptador nativo indisponivel nesta build.\n");
    }

    printf("\n[MVP] Transporte Cloudflare e cache inicializados.\n");
    printf("[MVP] Proximo marco: parser completo + UI em grade + fila.\n");

    // Mantém o app vivo no scaffold; a UI/controller loop substituirá isto.
    for (;;) {
        sleep(1);
    }

    installer.shutdown();
    return 0;
}
