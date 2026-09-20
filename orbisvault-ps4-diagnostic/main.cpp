#include <SDL2/SDL.h>
#include <stdio.h>

static void block(SDL_Renderer* r,int x,int y,int w,int h,Uint8 rr,Uint8 gg,Uint8 bb){
    SDL_SetRenderDrawColor(r,rr,gg,bb,255);
    SDL_Rect rc{x,y,w,h};
    SDL_RenderFillRect(r,&rc);
}

int main() {
    setvbuf(stdout,nullptr,_IONBF,0);

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) {
        for(;;) {}
    }

    SDL_Window* window = SDL_CreateWindow(
        "Orbis Vault Diagnostic",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        1920,1080,0);

    if(!window) for(;;) {}

    SDL_Surface* surface = SDL_GetWindowSurface(window);
    if(!surface) for(;;) {}

    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
    if(!renderer) for(;;) {}

    // Fundo azul escuro.
    SDL_SetRenderDrawColor(renderer,2,8,23,255);
    SDL_RenderClear(renderer);

    // Barra azul e três blocos verdes = diagnóstico visual OK.
    block(renderer,120,150,1680,16,22,140,255);
    block(renderer,240,360,420,260,31,211,138);
    block(renderer,750,360,420,260,31,211,138);
    block(renderer,1260,360,420,260,31,211,138);

    // Faixa branca inferior.
    block(renderer,240,760,1440,90,244,248,255);

    SDL_UpdateWindowSurface(window);

    // Mantém o app vivo. Se esta tela aparecer, o empacotamento e o SDL básico
    // funcionam no console; o problema está em alguma dependência do app completo.
    for(;;) {
        SDL_Event ev;
        while(SDL_PollEvent(&ev)) {
            if(ev.type == SDL_QUIT) return 0;
        }
        SDL_Delay(16);
    }
}
