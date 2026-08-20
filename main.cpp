//
// Created by alexoxorn on 7/23/26.
//

#include "structs.h"
#include "consts.h"
#include "draw.h"
#include "textureManager.h"
#include "main_renderer.h"
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <fstream>

#include "main_menu.h"
namespace fs = std::filesystem;
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <cmath>
#include <expected>
#include <format>
#include <ranges>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

enum class Mode {
    RENDERING,
    MENU,
};

static Mode currentMode = Mode::MENU;

static const char* HelpText = R"HELP(
usage: ./Sonic3Atlus
Keyboard Commands:
    ESC: Quit the Program

    (The following only works when NOT outputting to a video)

    P: Increase speed by 1.5x
    O: Decrease speed by 0.67x
    I: Reset speed back to default
    SPACE: Pause + Advance a single frame
    PAUSE: Pause / Unpause
    M: Cycle to next file from ../SocketBin
    N: Cycle to previous file from ../SocketBin
    Z: Skip to next level
    1-7: Toggle displaying different layers
)HELP";

[[noreturn]] static void printHelp() {
    fprintf(stderr, "%s", HelpText);
    exit(1);
}

static SDL_AppResult (*inits[])() = {Output::INIT, Menu::INIT};
static SDL_AppResult (*clears[])(void*) = {Output::CLEAR, Menu::CLEAR};

static void switchModes(void* appdata, const Mode mode) {
    clears[std::to_underlying(currentMode)](appdata);
    currentMode = mode;
    inits[std::to_underlying(currentMode)]();
}

extern "C" SDL_AppResult SDL_AppInit(void ** /*appstate*/, int  /*argc*/, char * argv[])
{
    baseConfig.setDefaults();
    if (stdfs::exists("config.txt")) {
        baseConfig.deserialize(slurp("config.txt"));
    }

    SDL_SetAppMetadata("Sonic 3 Atlas Encoding", "1.0", "");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return Menu::INIT();

    return SDL_APP_CONTINUE;
}


extern "C" SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    switch (currentMode) {
        case Mode::RENDERING: {
            const auto res = Output::EVENT(appstate, event);
            if (res == SDL_APP_SUCCESS) {
                switchModes(appstate, Mode::MENU);
                return SDL_APP_CONTINUE;
            }
            return res;
        }
        case Mode::MENU: {
            return Menu::EVENT(appstate, event);
        }
    }
    return SDL_APP_CONTINUE;
}

extern "C" SDL_AppResult SDL_AppIterate(void *appstate) {
    switch (currentMode) {
        case Mode::RENDERING: {
            const auto res = Output::ITER(appstate);
            if (res != SDL_APP_CONTINUE) {
                switchModes(appstate, Mode::MENU);
                return SDL_APP_CONTINUE;
            }
            return res;
        }
        case Mode::MENU: {
            const auto res = Menu::ITER(appstate);
            if (res == SDL_APP_SUCCESS) {
                switchModes(appstate, Mode::RENDERING);
                return SDL_APP_CONTINUE;
            }
            return res;
        }
    }
    return SDL_APP_CONTINUE;
}


/* This function runs once at shutdown. */
extern "C" void SDL_AppQuit(void * appstate, SDL_AppResult  /*result*/)
{
    clears[std::to_underlying(currentMode)](appstate);
    std::ofstream outFile("config.txt");
    if (!outFile) {
        std::cerr << "Error opening file!" << std::endl;
    }
    outFile << baseConfig.serialize();
    outFile.close();
}