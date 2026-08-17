//
// Created by alexoxorn on 8/15/26.
//

#ifndef SONIC3ATLUS_MAIN_MENU_H
#define SONIC3ATLUS_MAIN_MENU_H
#include "SDL3/SDL_init.h"

namespace Menu {
    SDL_AppResult INIT();
    SDL_AppResult EVENT(void* appstate, SDL_Event* event);
    SDL_AppResult ITER(void* appstate);
    SDL_AppResult CLEAR(void* appstate);
}


#endif //SONIC3ATLUS_MAIN_MENU_H
