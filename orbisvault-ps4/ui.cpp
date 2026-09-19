#include "ui.hpp"
#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <stdio.h>

namespace ov {

namespace {

constexpr int W = 1920;
constexpr int H = 1080;
constexpr int GRID_COLS = 5;
constexpr int GRID_ROWS = 2;
constexpr int CARD_W = 292;
constexpr int CARD_H = 360;
constexpr int CARD_GAP = 38;
constexpr int GRID_X = 118;
constexpr int GRID_Y = 205;

enum PadButton {
    PAD_CROSS = 0,
    PAD_CIRCLE = 1,
    PAD_SQUARE = 2,
    PAD_TRIANGLE = 3,
    PAD_OPTIONS = 9,
    PAD_UP = 13,
    PAD_DOWN = 14,
    PAD_LEFT = 15,
    PAD_RIGHT = 16
};

struct Glyph {
    char c;
    uint8_t row[7];
};

#define G(ch,a,b,c,d,e,f,g) {ch,{a,b,c,d,e,f,g}}
static const Glyph FONT[] = {
G('A',14,17,17,31,17,17,17), G('B',30,17,17,30,17,17,30),
G('C',14,17,16,16,16,17,14), G('D',30,17,17,17,17,17,30),
G('E',31,16,16,30,16,16,31), G('F',31,16,16,30,16,16,16),
G('G',14,17,16,23,17,17,14), G('H',17,17,17,31,17,17,17),
G('I',31,4,4,4,4,4,31), G('J',7,2,2,2,18,18,12),
G('K',17,18,20,24,20,18,17), G('L',16,16,16,16,16,16,31),
G('M',17,27,21,21,17,17,17), G('N',17,25,21,19,17,17,17),
G('O',14,17,17,17,17,17,14), G('P',30,17,17,30,16,16,16),
G('Q',14,17,17,17,21,18,13), G('R',30,17,17,30,20,18,17),
G('S',15,16,16,14,1,1,30), G('T',31,4,4,4,4,4,4),
G('U',17,17,17,17,17,17,14), G('V',17,17,17,17,17,10,4),
G('W',17,17,17,21,21,21,10), G('X',17,17,10,4,10,17,17),
G('Y',17,17,10,4,4,4,4), G('Z',31,1,2,4,8,16,31),
G('0',14,17,19,21,25,17,14), G('1',4,12,4,4,4,4,14),
G('2',14,17,1,2,4,8,31), G('3',30,1,1,14,1,1,30),
G('4',2,6,10,18,31,2,2), G('5',31,16,16,30,1,1,30),
G('6',14,16,16,30,17,17,14), G('7',31,1,2,4,8,8,8),
G('8',14,17,17,14,17,17,14), G('9',14,17,17,15,1,1,14),
G('-',0,0,0,31,0,0,0), G('_',0,0,0,0,0,0,31),
G('.',0,0,0,0,0,6,6), G(':',0,6,6,0,6,6,0),
G('/',1,2,2,4,8,8,16), G('%',17,2,4,8,16,17,0),
G('(',2,4,8,8,8,4,2), G(')',8,4,2,2,2,4,8),
G('+',0,4,4,31,4,4,0), G('!',4,4,4,4,4,0,4),
G('?',14,17,1,2,4,0,4), G(' ',0,0,0,0,0,0,0)
};
#undef G

const uint8_t* glyph(char c) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (const auto& g : FONT) if (g.c == c) return g.row;
    for (const auto& g : FONT) if (g.c == '?') return g.row;
    return FONT[0].row;
}

std::string shortText(const std::string& s, int maxChars) {
    if (maxChars < 0 || static_cast<int>(s.size()) <= maxChars) return s;
    if (maxChars <= 3) return s.substr(0, maxChars);
    return s.substr(0, maxChars - 3) + "...";
}

std::string coverPath(const TitleItem& t) {
    return std::string(DATA_DIR) + "/covers/" + t.titleId + ".img";
}

} // namespace

AppUi::AppUi() = default;

AppUi::~AppUi() {
    freeTextures();
    if (controller_) SDL_JoystickClose(controller_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    IMG_Quit();
    SDL_Quit();
}

bool AppUi::initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) return false;
    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP);

    window_ = SDL_CreateWindow(
        "Orbis Vault", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        W, H, 0);
    if (!window_) return false;

    SDL_Surface* surface = SDL_GetWindowSurface(window_);
    renderer_ = SDL_CreateSoftwareRenderer(surface);
    if (!renderer_) return false;

    if (SDL_NumJoysticks() > 0) controller_ = SDL_JoystickOpen(0);
    return true;
}

void AppUi::setCatalog(const Catalog* catalog) {
    catalog_ = catalog;
    selected_ = 0;
    scrollRow_ = 0;
    freeTextures();
}

void AppUi::setStatus(const UiRuntimeStatus& status) {
    status_ = status;
}

void AppUi::fillRect(int x, int y, int w, int h,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    SDL_SetRenderDrawColor(renderer_, r, g, b, a);
    SDL_Rect rc{x,y,w,h};
    SDL_RenderFillRect(renderer_, &rc);
}

void AppUi::outlineRect(int x, int y, int w, int h,
                        uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    SDL_SetRenderDrawColor(renderer_, r, g, b, a);
    SDL_Rect rc{x,y,w,h};
    SDL_RenderDrawRect(renderer_, &rc);
}

void AppUi::drawText(int x, int y, const std::string& input,
                     int scale, uint8_t r, uint8_t g, uint8_t b,
                     int maxChars) {
    const std::string text = shortText(input, maxChars);
    SDL_SetRenderDrawColor(renderer_, r, g, b, 255);
    int cx = x;
    for (char ch : text) {
        const uint8_t* rows = glyph(ch);
        for (int yy=0; yy<7; ++yy) {
            for (int xx=0; xx<5; ++xx) {
                if (rows[yy] & (1 << (4-xx))) {
                    SDL_Rect p{cx + xx*scale, y + yy*scale, scale, scale};
                    SDL_RenderFillRect(renderer_, &p);
                }
            }
        }
        cx += 6*scale;
    }
}

void AppUi::drawProgress(int x, int y, int w, int h, int percent) {
    percent = std::max(0, std::min(100, percent));
    fillRect(x,y,w,h,9,28,48);
    fillRect(x,y,w*percent/100,h,22,140,255);
    outlineRect(x,y,w,h,43,105,158);
}

SDL_Texture* AppUi::coverTexture(const TitleItem& title) {
    auto it = coverTextures_.find(title.titleId);
    if (it != coverTextures_.end()) return it->second;

    const std::string path = coverPath(title);
    SDL_Surface* img = IMG_Load(path.c_str());
    if (!img) {
        coverTextures_[title.titleId] = nullptr;
        return nullptr;
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, img);
    SDL_FreeSurface(img);
    coverTextures_[title.titleId] = tex;
    return tex;
}

void AppUi::freeTextures() {
    for (auto& kv : coverTextures_) if (kv.second) SDL_DestroyTexture(kv.second);
    coverTextures_.clear();
}

void AppUi::renderHeader() {
    fillRect(0,0,W,145,2,8,23);
    drawText(72,44,"ORBIS VAULT",5,244,248,255);
    drawText(72,92,"PS4",2,53,200,255);

    const std::string online = status_.online ? "ONLINE" : "OFFLINE";
    drawText(1550,48,online,3,
             status_.online ? 31 : 255,
             status_.online ? 211 : 82,
             status_.online ? 138 : 99);
    drawText(1550,88,"REV "+std::to_string(status_.revision),2,159,181,205);
    fillRect(0,144,W,1,21,54,86);
}

void AppUi::renderCatalog() {
    drawText(84,164,"CATALOGO",3,244,248,255);
    if (!catalog_ || catalog_->titles.empty()) {
        drawText(84,260,"CATALOGO VAZIO OU INDISPONIVEL",3,159,181,205);
        return;
    }

    const int selectedRow = selected_ / GRID_COLS;
    if (selectedRow < scrollRow_) scrollRow_ = selectedRow;
    if (selectedRow >= scrollRow_ + GRID_ROWS) scrollRow_ = selectedRow - GRID_ROWS + 1;

    const int first = scrollRow_ * GRID_COLS;
    const int last = std::min(
        static_cast<int>(catalog_->titles.size()),
        first + GRID_COLS * GRID_ROWS);

    for (int i=first; i<last; ++i) {
        const int local = i-first;
        const int row = local / GRID_COLS;
        const int col = local % GRID_COLS;
        const int x = GRID_X + col*(CARD_W + CARD_GAP);
        const int y = GRID_Y + row*(CARD_H + 35);
        const bool sel = i == selected_;

        fillRect(x,y,CARD_W,CARD_H, sel?12:7, sel?49:22, sel?87:42);
        outlineRect(x,y,CARD_W,CARD_H,
                    sel?53:21, sel?200:54, sel?255:86);

        const auto& title = catalog_->titles[i];
        SDL_Texture* tex = coverTexture(title);
        const int coverH = 250;
        if (tex) {
            SDL_Rect dst{x+18,y+16,CARD_W-36,coverH};
            SDL_RenderCopy(renderer_, tex, nullptr, &dst);
        } else {
            fillRect(x+18,y+16,CARD_W-36,coverH,10,36,65);
            drawText(x+84,y+112,"PS4",4,53,200,255);
        }

        drawText(x+18,y+282,title.name,2,244,248,255,20);
        drawText(x+18,y+322,
                 title.titleId + (title.installed ? " INSTALADO" : ""),
                 2,
                 title.installed ? 31 : 159,
                 title.installed ? 211 : 181,
                 title.installed ? 138 : 205,
                 24);
    }
}

void AppUi::renderDetails() {
    if (!catalog_ || selected_ < 0 ||
        selected_ >= static_cast<int>(catalog_->titles.size())) return;

    const auto& t = catalog_->titles[selected_];
    drawText(82,168,"DETALHES",3,244,248,255);

    const int x=82,y=225;
    fillRect(x,y,390,570,7,22,42);
    outlineRect(x,y,390,570,21,54,86);
    SDL_Texture* tex=coverTexture(t);
    if(tex){
        SDL_Rect dst{x+24,y+24,342,430};
        SDL_RenderCopy(renderer_,tex,nullptr,&dst);
    }else{
        fillRect(x+24,y+24,342,430,10,36,65);
        drawText(x+130,y+210,"PS4",5,53,200,255);
    }

    drawText(525,230,t.name,4,244,248,255,27);
    drawText(525,288,t.titleId+"  "+t.region,2,53,200,255,38);
    drawText(525,340,t.category.empty()?"PS4":t.category,2,159,181,205,42);
    drawText(525,382,
             t.installed ? "INSTALADO NO PS4" : "NAO INSTALADO",
             2,
             t.installed ? 31 : 255,
             t.installed ? 211 : 189,
             t.installed ? 138 : 74,
             42);

    int py=435;
    if(t.hasBase){
        drawText(525,py,"BASE  V"+t.base.version,3,31,211,138,46);
        py+=52;
    }else{
        drawText(525,py,"BASE  NAO CADASTRADA",3,255,189,74,46);
        py+=52;
    }
    drawText(525,py,"UPDATES  "+std::to_string(t.updates.size()),3,159,181,205); py+=52;
    drawText(525,py,"DLCS     "+std::to_string(t.dlcs.size()),3,159,181,205); py+=78;

    if(t.installed){
        const bool openSel=selectedDetailAction_==0;
        fillRect(525,py,600,76,openSel?13:8,openSel?99:30,openSel?185:52);
        outlineRect(525,py,600,76,22,140,255);
        drawText(570,py+25,"ABRIR JOGO",3,244,248,255);

        const int installY=py+94;
        const bool installSel=selectedDetailAction_==1;
        fillRect(525,installY,600,76,installSel?13:8,installSel?99:30,installSel?185:52);
        outlineRect(525,installY,600,76,22,140,255);
        drawText(570,installY+25,"BAIXAR / ATUALIZAR",3,244,248,255);

        drawText(525,installY+118,"BASE -> UPDATE -> DLCS",2,159,181,205);
    }else{
        const bool buttonSel=selectedDetailAction_==0;
        fillRect(525,py,600,76,buttonSel?13:8,buttonSel?99:30,buttonSel?185:52);
        outlineRect(525,py,600,76,22,140,255);
        drawText(570,py+25,"BAIXAR E INSTALAR",3,244,248,255);
        drawText(525,py+118,"BASE -> UPDATE -> DLCS",2,159,181,205);
    }
}

void AppUi::renderDownloads() {
    drawText(82,168,"DOWNLOADS",3,244,248,255);
    fillRect(82,230,1756,260,7,22,42);
    outlineRect(82,230,1756,260,21,54,86);

    if(status_.jobTitle.empty()){
        drawText(120,290,"NENHUM DOWNLOAD ATIVO",3,159,181,205);
        drawText(120,350,"SELECIONE UM TITULO E PRESSIONE X",2,103,135,168);
    }else{
        drawText(120,275,status_.jobTitle,3,244,248,255,42);
        drawText(120,335,status_.jobStage,2,53,200,255,60);
        drawProgress(120,395,1560,34,status_.jobProgress);
        drawText(1710,395,std::to_string(status_.jobProgress)+"%",2,244,248,255);
    }
}

void AppUi::renderSettings() {
    drawText(82,168,"CONFIGURACOES",3,244,248,255);
    int y=245;
    fillRect(82,y,1756,110,7,22,42);
    drawText(120,y+26,"SERVIDOR",2,159,181,205);
    drawText(120,y+64,API_BASE,2,53,200,255,74);

    y+=140;
    fillRect(82,y,1756,110,7,22,42);
    drawText(120,y+26,"INSTALACAO REMOTA",2,159,181,205);
    if(status_.remotePaired){
        drawText(120,y+64,"PS4 PAREADO COM O APP ANDROID",2,31,211,138,65);
        if(!status_.remoteDeviceName.empty())
            drawText(1160,y+64,status_.remoteDeviceName,2,159,181,205,28);
    }else if(!status_.pairingCode.empty()){
        drawText(120,y+64,"CODIGO: "+status_.pairingCode,3,53,200,255,40);
        drawText(720,y+68,"DIGITE ESTE CODIGO NO CELULAR",2,244,248,255,48);
    }else{
        drawText(120,y+64,"PRESSIONE X PARA PAREAR COM O APP ANDROID",2,244,248,255,65);
    }

    y+=140;
    fillRect(82,y,1756,110,7,22,42);
    drawText(120,y+26,"CACHE",2,159,181,205);
    drawText(120,y+64,"/DATA/ORBIS-VAULT",2,244,248,255);
}

void AppUi::renderFooter() {
    fillRect(0,1010,W,70,3,15,30);
    fillRect(0,1009,W,1,21,54,86);

    if(screen_==UiScreen::Catalog)
        drawText(70,1035,"X ABRIR   TRIANGULO DOWNLOADS   QUADRADO CONFIG   OPTIONS SINCRONIZAR",2,159,181,205,84);
    else if(screen_==UiScreen::Details)
        drawText(70,1035,"X SELECIONAR   CIMA/BAIXO ACAO   O VOLTAR   TRIANGULO DOWNLOADS",2,159,181,205,82);
    else if(screen_==UiScreen::Settings)
        drawText(70,1035,"X PAREAR PS4   O VOLTAR",2,159,181,205,58);
    else
        drawText(70,1035,"QUADRADO CANCELAR   O VOLTAR   OPTIONS SINCRONIZAR",2,159,181,205,70);
}

void AppUi::moveSelection(int dx,int dy) {
    if(!catalog_ || catalog_->titles.empty()) return;
    int next=selected_ + dx + dy*GRID_COLS;
    next=std::max(0,std::min(next,static_cast<int>(catalog_->titles.size())-1));
    selected_=next;
}

UiAction AppUi::handleButton(uint8_t button) {
    UiAction a;

    if(button==PAD_OPTIONS){
        a.type=UiActionType::RefreshCatalog;
        return a;
    }
    if(button==PAD_TRIANGLE){
        screen_=UiScreen::Downloads;
        return a;
    }
    if(button==PAD_SQUARE && screen_==UiScreen::Catalog){
        screen_=UiScreen::Settings;
        return a;
    }
    if(button==PAD_CIRCLE){
        if(screen_!=UiScreen::Catalog) screen_=UiScreen::Catalog;
        return a;
    }

    if(screen_==UiScreen::Catalog){
        if(button==PAD_LEFT) moveSelection(-1,0);
        else if(button==PAD_RIGHT) moveSelection(1,0);
        else if(button==PAD_UP) moveSelection(0,-1);
        else if(button==PAD_DOWN) moveSelection(0,1);
        else if(button==PAD_CROSS) {
            selectedDetailAction_=0;
            screen_=UiScreen::Details;
        }
    }else if(screen_==UiScreen::Details){
        const bool installed =
            catalog_ && selected_ >= 0 &&
            selected_ < static_cast<int>(catalog_->titles.size()) &&
            catalog_->titles[selected_].installed;

        if(button==PAD_UP || button==PAD_DOWN){
            if(installed) selectedDetailAction_=selectedDetailAction_==0?1:0;
            else selectedDetailAction_=0;
        }else if(button==PAD_CROSS){
            if(installed && selectedDetailAction_==0)
                a.type=UiActionType::OpenSelected;
            else
                a.type=UiActionType::InstallSelected;
            a.titleIndex=selected_;
        }
    }else if(screen_==UiScreen::Downloads){
        if(button==PAD_SQUARE) a.type=UiActionType::CancelInstall;
    }else if(screen_==UiScreen::Settings){
        if(button==PAD_CROSS) a.type=UiActionType::StartPairing;
    }

    return a;
}

UiAction AppUi::update() {
    UiAction out;
    SDL_Event ev;
    while(SDL_PollEvent(&ev)){
        if(ev.type==SDL_QUIT){
            running_=false;
            out.type=UiActionType::Exit;
        }else if(ev.type==SDL_JOYBUTTONDOWN){
            UiAction a=handleButton(ev.jbutton.button);
            if(a.type!=UiActionType::None) out=a;
        }
    }
    return out;
}

void AppUi::render() {
    if(!renderer_ || !window_) return;
    SDL_SetRenderDrawColor(renderer_,2,8,23,255);
    SDL_RenderClear(renderer_);

    renderHeader();
    switch(screen_){
        case UiScreen::Catalog: renderCatalog(); break;
        case UiScreen::Details: renderDetails(); break;
        case UiScreen::Downloads: renderDownloads(); break;
        case UiScreen::Settings: renderSettings(); break;
    }

    if(!status_.message.empty())
        drawText(82,965,status_.message,2,103,135,168,100);

    renderFooter();
    SDL_UpdateWindowSurface(window_);
}

} // namespace ov
