// minimal_test.cpp — Just test SDL window creation
#include <SDL2/SDL.h>
#include <cstdio>

int main() {
    printf("Starting minimal test...\n");
    
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("SDL initialized\n");
    
    SDL_Window* window = SDL_CreateWindow(
        "Minimal Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        800, 600,
        0
    );
    if (!window) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    printf("Window created successfully\n");
    
    SDL_Delay(1000);
    
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    printf("Clean exit\n");
    return 0;
}
