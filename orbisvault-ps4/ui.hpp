#pragma once
#include "models.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <map>
#include <string>

namespace ov {

enum class UiScreen {
    Catalog,
    Details,
    Downloads,
    Settings
};

enum class UiActionType {
    None,
    RefreshCatalog,
    InstallSelected,
    OpenSelected,
    StartPairing,
    Exit
};

struct UiAction {
    UiActionType type = UiActionType::None;
    int titleIndex = -1;
};

struct UiRuntimeStatus {
    bool online = false;
    int revision = 0;
    std::string message;
    std::string jobTitle;
    std::string jobStage;
    int jobProgress = 0;

    bool remotePaired = false;
    std::string remoteDeviceName;
    std::string pairingCode;
};

class AppUi {
public:
    AppUi();
    ~AppUi();

    bool initialize();
    void setCatalog(const Catalog* catalog);
    void setStatus(const UiRuntimeStatus& status);

    bool running() const { return running_; }
    UiAction update();
    void render();

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Joystick* controller_ = nullptr;

    const Catalog* catalog_ = nullptr;
    UiRuntimeStatus status_;

    UiScreen screen_ = UiScreen::Catalog;
    bool running_ = true;
    int selected_ = 0;
    int selectedDetailAction_ = 0;
    int scrollRow_ = 0;

    std::map<std::string, SDL_Texture*> coverTextures_;

    void renderHeader();
    void renderCatalog();
    void renderDetails();
    void renderDownloads();
    void renderSettings();
    void renderFooter();

    void moveSelection(int dx, int dy);
    UiAction handleButton(uint8_t button);

    SDL_Texture* coverTexture(const TitleItem& title);
    void freeTextures();

    void fillRect(int x, int y, int w, int h,
                  uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void outlineRect(int x, int y, int w, int h,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void drawText(int x, int y, const std::string& text,
                  int scale, uint8_t r, uint8_t g, uint8_t b,
                  int maxChars = -1);
    void drawProgress(int x, int y, int w, int h, int percent);
};

} // namespace ov
