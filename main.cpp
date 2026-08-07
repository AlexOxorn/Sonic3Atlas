//
// Created by alexoxorn on 7/23/26.
//

#include "structs.h"
#include "consts.h"
#include "draw.h"
#include "textureManager.h"
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
namespace fs = std::filesystem;
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <cmath>
#include <expected>
#include <format>
#include <ranges>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>


enum updateResult {
    UPDATE_SUCCESS,
    UPDATE_FAILURE,
    UPDATE_LAG_FRAME,
    UPDATE_EOF,
};



static FILE* ffmpegProcess = nullptr;


static updateResult update_data();

static s32 getNextFrame(FILE* fd) {
    char msg[9] = {};
    char dump[128];
    size_t count;
    s32 flags = 0;
    while ((count = recvStrict(fd, msg, 8)) > 0) {
        if (strncmp(msg, "SCRN_POS", 8) == 0) {
            recvStrict(fd, &gameData.screen_position_A.first, 2);
            recvStrict(fd, &gameData.screen_position_A.second, 2);
            recvStrict(fd, &gameData.screen_position_B.first, 2);
            recvStrict(fd, &gameData.screen_position_B.second, 2);
            flags |= SCREEN_UPDATED;
        }
        else if (strncmp(msg, "COLORTST", 8) == 0) {
            gameData.palette = Palette::fromSocket(fd);
            gameData.water_palette = Palette::fromSocket(fd);
            flags |= COLOR_UPDATED;
        }
        else if (strncmp(msg, "TILE_TST", 8) == 0) {
            gameData.tileset = TileSet::fromSocket(fd);
            gameData.newly_updated_tiles.clear();
            flags |= FULL_TILE_UPDATED;
        }
        else if (strncmp(msg, "TILE_LST", 8) == 0) {
            s16 index;
            while (true) {
                recvStrict(fd, &index, 2);
                if (index == -1) break;
                gameData.tileset.tiles[index] = Tile::fromSocket(fd);
                gameData.newly_updated_tiles.insert(index);
            }
            flags |= PARTIAL_TILE_UPDATED;
        }
        else if (strncmp(msg, "VRAM_SET", 8) == 0) {
            u16 dest;
            u16 len;
            recvStrict(fd, &dest, 2);
            recvStrict(fd, &len, 2);
            for (int i = dest; i < dest+len; ++i) {
                gameData.tileset.tiles[i] = Tile::fromSocket(fd);
                gameData.newly_updated_tiles.insert(i);
            }
            flags |= PARTIAL_TILE_UPDATED;
        }
        else if (strncmp(msg, "VRAM_DMA", 8) == 0) {
            u16 dest;
            u16 len;
            recvStrict(fd, &dest, 2);
            recvStrict(fd, &len, 2);
            for (int i = dest; i < dest+len; ++i) {
                gameData.tileset.tiles[i] = Tile::fromSocket(fd);
                gameData.newly_updated_tiles.insert(i);
            }
            flags |= PARTIAL_TILE_UPDATED;
        }
        else if (strncmp(msg, "VRAMSET2", 8) == 0) {
            u16 dest;
            u16 len;
            recvStrict(fd, &dest, 2);
            recvStrict(fd, &len, 2);
            dest /= 0x20;
            len /= 0x20;
            for (int i = dest; i < dest+len; ++i) {
                gameData.tileset.tiles[i] = Tile::fromSocket(fd);
                gameData.newly_updated_tiles.insert(i);
            }
            flags |= PARTIAL_TILE_UPDATED;
        }
        else if (strncmp(msg, "BLOCKTST", 8) == 0) {
            gameData.blocks = BlockMap::fromSocket(fd);
            flags |= BLOCK_LIST_UPDATED;
        }
        else if (strncmp(msg, "CHUNKTST", 8) == 0) {
            gameData.chunks = ChunkMap::fromSocket(fd);
            flags |= CHUNK_LIST_UPDATED;
        }
        else if (strncmp(msg, "H_SCROLL", 8) == 0) {
            for (int i=0; i < 0x380/2; i++)
                recvStrict(fd, dump, 2);
        }
        else if (strncmp(msg, "LVLDAT_", 7) == 0) {
            s16 columns;
            s16 rows;
            recvStrict(fd, &columns, 2);
            recvStrict(fd, &rows, 2);

            auto& lvlVec = (msg[7] == 'A') ? gameData.level_chunks : gameData.background_chunks;

            lvlVec.resize(rows);
            for (auto& row : lvlVec) {
                row.resize(columns);
                for (auto& cell : row) {
                    recvStrict(fd, &cell, 1);
                }
            }
            flags |= LEVEL_DATA_UPDATED;
        }
        else if (strncmp(msg, "SPRITE_2", 8) == 0) {
            gameData.sprites.clear();
            char sub[4];
            while (true) {
                recvStrict(fd, sub, 4);
                if (strncmp(sub, "NEXT", 4) != 0)
                    break;
                gameData.sprites.push_back(Sprite::fromSocket(fd));
            }
            flags |= SPRITE_DATA_UPDATED;
        }
        else if (strncmp(msg, "LOOPDATA", 8) == 0) {
            recvStrict(fd, &gameData.vertical_loop, 2);
            recvStrict(fd, &gameData.screen_min_x, 2);
            recvStrict(fd, &gameData.screen_min_y, 2);
            recvStrict(fd, &gameData.screen_max_x, 2);
            recvStrict(fd, &gameData.screen_max_y, 2);
            flags |= LOOP_DATA_UPDATED;
        }
        else if (strncmp(msg, "WATER_LV", 8) == 0) {
            recvStrict(fd, &gameData.has_water, 1);
            recvStrict(fd, &gameData.water_line, 2);
            flags |= WATER_LV_UPDATED;
        }
        else if (strncmp(msg, "POSITION", 8) == 0) {
            recvStrict(fd, &gameData.scroll_x, 2);
            recvStrict(fd, &gameData.scroll_y, 2);
            flags |= POSITION_UPDATED;
        }
        else if (strncmp(msg, "RING_POS", 8) == 0) {
            gameData.ring_data.set_location_from_socket(fd);
            flags |= RING_POS_UPDATED;
        }
        else if (strncmp(msg, "RINGSTAT", 8) == 0) {
            gameData.ring_data.set_status_from_socket(fd);
            flags |= RING_STATS_UPDATED;
        }
        else if (strncmp(msg, "RING_MAP", 8) == 0) {
            gameData.ring_data.set_mappings_from_socket(fd);
            flags |= RING_MAP_UPDATED;
        }
        else if (strncmp(msg, "EVENTDAT", 8) == 0) {
            recvStrict(fd, &gameData.currentZoneAct, 2);
            recvStrict(fd, &gameData.bgEvent, 2);
            recvStrict(fd, &gameData.fgEvent, 2);
            recvStrict(fd, &gameData.bgEventVar, 2);
            recvStrict(fd, gameData.fgEventVars.begin(), 2 * 6);
            recvStrict(fd, &gameData.lbzDeathEggEvent, 2);
        }
        else if (strncmp(msg, "IS_PAUSE", 8) == 0) {
            recvStrict(fd, &gameData.gamePaused, 2);
            // flags |= GAME_PAUSED * static_cast<bool>(gameData.gamePaused);
        }
        else if (strncmp(msg, "DONE____", 8) == 0) {
            if (!gameData.gamePaused)
                return flags;
        }
        else if (strncmp(msg, "LAGFRAME", 8) == 0) {
            // return flags | LAG_FRAME;
        }
        else if (strncmp(msg, "LAGCOUNT", 8) == 0) {
            recvStrict(fd, &gameData.lagFrames, 2);
        }
        else {
            fprintf(stderr, "Unexpected msg '%8s'", msg);
            return (-1);
        }
    }
    return FRAME_EOF;
}

static std::vector<std::string> binFiles;
static int fileIndex = 0;

static bool init_renderer() {
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan");
    if (!device) {
        SDL_Log("Couldn't create GPU device: %s", SDL_GetError());
        return false;
    }

    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("Couldn't claim GPU device: %s", SDL_GetError());
        return false;
    };
    int max = SDL_GetNumRenderDrivers();
    for (int i = 0; i < max; i++) {
        printf("Renderer Options: %s\n", SDL_GetRenderDriver(i));
    }

    renderer = SDL_CreateRenderer(/*device, */window, nullptr);
    printf("Renderer Name %s\n", SDL_GetRendererName(renderer));
    if (!renderer) {
        SDL_Log( "Could not create renderer: %s\n", SDL_GetError() );
        return false;
    }

    SDL_SetRenderLogicalPresentation(renderer, RENDER_WIDTH, RENDER_HEIGHT, SDL_LOGICAL_PRESENTATION_OVERSCAN);

    //Enable VSync
    if( !SDL_SetRenderVSync( renderer, SDL_RENDERER_VSYNC_ADAPTIVE ) )
    {
        SDL_Log( "Could not enable VSync! SDL error: %s\n", SDL_GetError() );
        return false;
    }

    if constexpr (FFMPEG_DATA) {
        ffmpeg_texture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_TARGET,
            RENDER_WIDTH,
            RENDER_HEIGHT
            );
    }

    transparency_mask_palette = SDL_CreatePalette(2);
    SDL_Color trans_palette[] = {
        {.r = 255, .g = 255, .b = 255, .a = 255},
        {.r = 255, .g = 255, .b = 255, .a = 128},
    };
    SDL_SetPaletteColors(transparency_mask_palette, trans_palette, 0, 2);

    if (!initSourceTextures()) {
        SDL_Log("Failed to initialize source textures: %s\n", SDL_GetError());
    }

    if (!initDestTextures()) {
        SDL_Log("Failed to initialize source textures: %s\n", SDL_GetError());
    }

    return update_data() != UPDATE_FAILURE;
}

static bool operFile(int index) {
    if (inputStream != nullptr) {
        fclose(inputStream);
    }
    inputFD = open(binFiles[index].c_str(), O_RDONLY);
    if (inputFD < 0) {
        fprintf(stderr, "OPEN ERROR %d\n", errno);
        return false;
    }
    inputStream = fdopen(inputFD, "rb");
    return true;
}

static bool operFileDirect(const char* filename) {
    if (inputStream != nullptr) {
        fclose(inputStream);
    }
    inputFD = open(filename, O_RDONLY);
    if (inputFD < 0) {
        fprintf(stderr, "OPEN ERROR %d\n", errno);
        return false;
    }
    inputStream = fdopen(inputFD, "rb");
    return true;
}

extern "C" SDL_AppResult SDL_AppInit(void ** /*appstate*/, int  /*argc*/, char * argv[])
{
    argv++;
    while (*argv != nullptr) {
        printf("arg %s\n", *argv);
        if (strcmp(*argv++, "--startFile") == 0) {
            printf("file %s\n", *argv);
            char* e;
            fileIndex = static_cast<int>(std::strtol(*argv++, &e, 10));
        }
    }

    SDL_Surface *surface  = nullptr;

    if constexpr (FFMPEG_DATA) {
        auto [in, out] = *FFMPEG_DATA;
        if (!operFileDirect(in)) {
            fprintf(stderr, "Couldn't Open File\n");
            return SDL_APP_FAILURE;
        }

        const auto ffmpegCmd = std::format(
            "ffmpeg -y "
            "-f rawvideo "
            "-pix_fmt rgba "
            "-s {}x{} "
            "-r 60 "
            "-i - "
            "-c:v h264_nvenc "
            "-b:v 80M "
            // "-minrate:v 24M "
            // "-maxrate:v 50M "
            "-pix_fmt yuv420p "
            // "-vf scale=4096:-2 "
            // "-vf \"scale=trunc(iw/2)*2:trunc(ih/2)*2\""
            "{}",
            FFMPEG_WIDTH,
            FFMPEG_HEIGHT,
            out);
        ffmpegProcess = popen(ffmpegCmd.c_str(), "w");
        if (!ffmpegProcess) {
            fprintf(stderr, "Couldn't Open POPEN: %d\n", errno);
            return SDL_APP_FAILURE;
        }
    } else {
        for (const auto & entry : fs::directory_iterator("../SocketBins"))
            binFiles.push_back(entry.path().string());

        stdr::sort(binFiles);

        if (binFiles.empty()) {
            fprintf(stderr, "No bin files found\n");
            return SDL_APP_FAILURE;
        }
        if (!operFile(fileIndex)) {
            fprintf(stderr, "Couldn't Open File\n");
            return SDL_APP_FAILURE;
        }
    }



    SDL_SetAppMetadata("Example Blending", "1.0", "com.example.blending");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    window = SDL_CreateWindow("examples/renderer/blending", WINDOW_WIDTH, WINDOW_HEIGHT,
        0
        // | SDL_WINDOW_FULLSCREEN
        // | SDL_WINDOW_BORDERLESS
        );
    if (window == nullptr) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!init_renderer()) {
        fprintf(stderr, "Couldn't initialize renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}




#define RENDER_TOGGLE_EVENT(x) if (event->key.key == SDLK_##x) { renderFlags ^= (1 << (x-1)); renderToggleTimers[x-1] = 3.0f; }

extern "C" SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.key == SDLK_ESCAPE) {
            return SDL_APP_SUCCESS;
        }
        if constexpr (FFMPEG_DATA) {
            return SDL_APP_CONTINUE;
        }
        if (event->key.key == SDLK_DOWN) {
            scrollY += 32.0f;
            redrawLevel = true;
        }
        if (event->key.key == SDLK_UP) {
            scrollY -= 32.0f;
            redrawLevel = true;
        }
        if (event->key.key == SDLK_RIGHT) {
            scrollX += 32.0f;
            redrawLevel = true;
        }
        if (event->key.key == SDLK_LEFT) {
            scrollX -= 32.0f;
            redrawLevel = true;
        }
        if (event->key.key == SDLK_KP_PLUS) {
            scale *= 2.0f;
        }
        if (event->key.key == SDLK_KP_MINUS) {
            scale /= 2.0f;
        }
        if (event->key.key == SDLK_P) {
            speed += 1.0f;
            SpeedChangeMsgTimer = 3.0f;
        }
        if (event->key.key == SDLK_O) {
            speed -= 1.0f;
            SpeedChangeMsgTimer = 3.0f;
        }
        if (event->key.key == SDLK_I) {
            speed = 0.0f;
            SpeedChangeMsgTimer = 3.0f;
        }
        if (event->key.key == SDLK_SPACE) {
            update_data();
            pauseData = true;
        }
        if (event->key.key == SDLK_PAUSE) {
            pauseData = !pauseData;
        }
        if (event->key.key == SDLK_M) {
            fileIndex += 1;
            fileIndex %= static_cast<int>(binFiles.size());
            operFile(fileIndex);
            FileChangeMsgTimer = 3.0f;
        }
        if (event->key.key == SDLK_N) {
            fileIndex -= 1;
            fileIndex = fileIndex < 0 ? fileIndex + static_cast<int>(binFiles.size()) : fileIndex;
            operFile(fileIndex);
            FileChangeMsgTimer = 3.0f;
        }
        RENDER_TOGGLE_EVENT(1);
        RENDER_TOGGLE_EVENT(2);
        RENDER_TOGGLE_EVENT(3);
        RENDER_TOGGLE_EVENT(4);
        RENDER_TOGGLE_EVENT(5);
        RENDER_TOGGLE_EVENT(6);
        RENDER_TOGGLE_EVENT(7);
        if (event->key.key == SDLK_Z) {
            skipToNextLevel = true;
        }
    }
    return SDL_APP_CONTINUE;
}

static bool updatePalette() {
    if (low_prio_palette == nullptr) {
        low_prio_palette = SDL_CreatePalette(1 << (sizeof(indexedColor) * 8));
        if (!low_prio_palette) {
            SDL_Log("Couldn't create palette: %s", SDL_GetError());
            return false;
        }
    }
    if (high_prio_palette == nullptr) {
        high_prio_palette = SDL_CreatePalette(1 << (sizeof(indexedColor) * 8));
        if (!high_prio_palette) {
            SDL_Log("Couldn't create palette: %s", SDL_GetError());
            return false;
        }
    }

    std::array<SDL_Color, PALETTE_SIZE*PALETTE_COUNT*2> low{};
    std::array<SDL_Color, PALETTE_SIZE*PALETTE_COUNT*2> high{};
    std::array<SDL_Color, PALETTE_SIZE*PALETTE_COUNT*2> lowTransparent{};
    std::array<SDL_Color, PALETTE_SIZE*PALETTE_COUNT*2> highTransparent{};

    auto to_SDL_color = [](const Color8Bit color) {
        return SDL_Color{.r = color.red, .g = color.green, .b = color.blue, .a = 255};
    };
    auto to_SDL_halfColor = [](const Color8Bit color) {
        return SDL_Color{.r = color.red, .g = color.green, .b = color.blue, .a = 128};
    };
    const auto low_out = low.begin();
    const auto high_out = high.begin();
    const auto lowT_out = lowTransparent.begin();
    const auto highT_out = highTransparent.begin();

    // LOW PRIO
    for (auto [i, line] : stdr::views::enumerate(gameData.palette.lines)) {
        stdr::transform(line.colors, low_out + (PALETTE_SIZE * i), to_SDL_color);
        if constexpr (sizeof(indexedColor) > 1)
            stdr::transform(line.colors, lowT_out + (PALETTE_SIZE * i), to_SDL_halfColor);
    }
    for (auto [i, line] : stdr::views::enumerate(gameData.water_palette.lines)) {
        stdr::transform(line.colors, low_out + (PALETTE_SIZE * (i+4)), to_SDL_color);
        if constexpr (sizeof(indexedColor) > 1)
            stdr::transform(line.colors, lowT_out + (PALETTE_SIZE * (i+4)), to_SDL_halfColor);
    }
    // HIGH PRIO
    for (auto [i, line] : stdr::views::enumerate(gameData.palette.lines)) {
        stdr::transform(line.colors, high_out + (PALETTE_SIZE * (i+8)), to_SDL_color);
        if constexpr (sizeof(indexedColor) > 1)
            stdr::transform(line.colors, highT_out + (PALETTE_SIZE * (i+8)), to_SDL_halfColor);
    }
    for (auto [i, line] : stdr::views::enumerate(gameData.water_palette.lines)) {
        stdr::transform(line.colors, high_out + (PALETTE_SIZE * (i+12)), to_SDL_color);
        if constexpr (sizeof(indexedColor) > 1)
            stdr::transform(line.colors, highT_out + (PALETTE_SIZE * (i+12)), to_SDL_halfColor);
    }

    low_out[0] = {.r=0,.g=0,.b=0,.a=0};
    low_out[0x40] = {.r=0,.g=0,.b=0,.a=0};
    high_out[0x80] = {.r=0,.g=0,.b=0,.a=0};
    high_out[0xC0] = {.r=0,.g=0,.b=0,.a=0};

    if (!SDL_SetPaletteColors(low_prio_palette, low.data(), 0, PALETTE_SIZE * PALETTE_COUNT * 2)) {
        SDL_Log("Couldn't set palette low opaque: %s", SDL_GetError());
        return false;
    };

    if (!SDL_SetPaletteColors(high_prio_palette, high.data(), 0, PALETTE_SIZE * PALETTE_COUNT * 2)) {
        SDL_Log("Couldn't set palette high opaque: %s", SDL_GetError());
        return false;
    };


    if constexpr (sizeof(indexedColor) > 1) {
        if (!SDL_SetPaletteColors(low_prio_palette, lowTransparent.data(), 0x100, PALETTE_SIZE * PALETTE_COUNT * 2)) {
            SDL_Log("Couldn't set palette low transparent: %s", SDL_GetError());
            return false;
        };

        if (!SDL_SetPaletteColors(high_prio_palette, highTransparent.data(), 0x100, PALETTE_SIZE * PALETTE_COUNT * 2)) {
            SDL_Log("Couldn't set palette low transparent: %s", SDL_GetError());
            return false;
        };
    }

    if (tiles_texture) {
        if (!SDL_SetTexturePalette(tiles_texture, high_prio_palette)) {
            SDL_Log("Couldn't assign 1 palette: %s", SDL_GetError());
            return false;
        }
    }
    if (chunks_texture) {
        if (!SDL_SetTexturePalette(chunks_texture, high_prio_palette)) {
            SDL_Log("Couldn't assign 2 palette: %s", SDL_GetError());
            return false;
        }
    }
    if (mappings_texture) {
        if (!SDL_SetTexturePalette(mappings_texture, high_prio_palette)) {
            SDL_Log("Couldn't assign 3 palette: %s", SDL_GetError());
            return false;
        }
    }

    return true;
}

static bool fullTileUpdate() {
    indexedColor* SDLpixels;
    int pitch;
    if (!SDL_LockTexture(tiles_texture, nullptr, reinterpret_cast<void**>(&SDLpixels), &pitch)) {
        SDL_Log("Couldn't lock tile texture: %s", SDL_GetError());
        return false;
    };

    for (auto& [pixels]: gameData.tileset.tiles) {
        for (auto row : pixels) {
            for (int offsets = 0; offsets < 0x80; offsets+=0x10) {
                for (const auto p : row) {
                    if (p)
                        *SDLpixels++ = p + offsets;
                    else
                        *SDLpixels++ = 0;
                }
            }
        }
    }

    SDL_UnlockTexture(tiles_texture);
    return true;
}

template <int CURR, typename Tup, typename F>
static void loopTuple(const Tup& data, const F& func) {
    if (CURR >= std::tuple_size_v<Tup>)
        return;
    func(std::get<CURR>(data));
    loopTuple<CURR + 1, Tup, F>(data, func);
}

static bool fullChunkUpdate() {
    indexedColor* SDLpixels;
    int pitch;
    if (!SDL_LockTexture(chunks_texture, nullptr, reinterpret_cast<void**>(&SDLpixels), &pitch)) {
        SDL_Log("Couldn't lock tile texture: %s", SDL_GetError());
        return false;
    };

    std::vector<bool> maskData(Chunk::WIDTH*16 *  Chunk::WIDTH * ChunkMap::COUNT/8);
    const indexedColor* start = SDLpixels;
    auto setMaskBit = [&] (const indexedColor* pixel) {
        const long index = pixel - start;
        maskData[index] = true;
    };

    auto getChunkBytes = [&] (const int i) {
        return gameData.chunks.getBytes(i,
            gameData.blocks,
            gameData.tileset); };

    for (int i = 0; i < ChunkMap::COUNT/8; ++i) {
        std::array<Chunk::pixelType, 16> chunks{};
        for (int j = 0; j < 8; ++j) {
            chunks[2*j] = getChunkBytes(j + i * 8);
            chunks[2*j+1] = getChunkBytes(j + i * 8);
        }

        for (auto row : zip_array(chunks)) {
            for (auto [rowCount, subrow] : row | stdv::enumerate) {
                for (int k = 0; k < subrow.size(); ++k) {
                    // LEFT MOST PIXEL IS TRANSPARENT BUT NEIGHBOUR ISN'T
                    if (k == 0 && subrow[k] == 0 && subrow[k+1] != 0) {
                        *SDLpixels = subrow[k+1] + (rowCount % 2 ? 0x40 : 0);
                        setMaskBit(SDLpixels);
                        setMaskBit(SDLpixels+1);
                        ++SDLpixels;
                    }
                    // TRANSPARENT PIXEL SURROUNDED BY IDENTICAL NON TRANSPARENT PIXEL
                    else if (k>0 && k < 127 && subrow[k] == 0 && subrow[k+1] != 0 && subrow[k-1] != 0) {
                        *SDLpixels = subrow[k-1] + (rowCount % 2 ? 0x40 : 0);
                        setMaskBit(SDLpixels-1);
                        setMaskBit(SDLpixels);
                        setMaskBit(SDLpixels+1);
                        ++SDLpixels;
                    }
                    // RIGHT MOST PIXEL IS TRANSPARENT BUT NEIGHBOUR ISN'T
                    else if (k==127 && subrow[k] == 0 && subrow[k-1] != 0) {
                        *SDLpixels = subrow[k-1] + (rowCount % 2 ? 0x40 : 0);
                        setMaskBit(SDLpixels);
                        setMaskBit(SDLpixels-1);
                        ++SDLpixels;
                    }
                    else {
                        *SDLpixels++ = subrow[k] + (rowCount % 2 ? 0x40 : 0);
                    }
                }
            }
        }
    }
    SDL_UnlockTexture(chunks_texture);

    if (!SDL_LockTexture(translucency_mask_texture, nullptr, reinterpret_cast<void**>(&SDLpixels), &pitch)) {
        SDL_Log("Couldn't lock transparency texture: %s", SDL_GetError());
        return false;
    };

    for (const bool t : maskData) {
        *SDLpixels++ = static_cast<int>(t);
    }
    SDL_UnlockTexture(translucency_mask_texture);

    return true;
}

static bool updateMappings() {
    // Building Map Bytes
    for (auto& s : gameData.sprites) {
        for (auto &entry : s.frame.entries) {
            static_cast<void>(entry.getBytes(s.object.art_tile, gameData.tileset));
        }
        for (auto& childEntry : s.children | stdv::transform(&SpriteMappingFrame::entries) | stdv::join) {
            static_cast<void>(childEntry.getBytes(s.object.art_tile, gameData.tileset));
        }
    }

    constexpr auto pixelsPerMapRow = pixelsPerRow * SpriteMappingEntry::WIDTH;

    auto chunkToId = [](const int c, const long x, const long y) {
        const long chunk_row = c / MAPPING_ENTRY_PER_ROW;
        const long chunk_col = (c % MAPPING_ENTRY_PER_ROW) * 2;
        long base = chunk_row * pixelsPerMapRow;
        base += y * pixelsPerRow;
        base += chunk_col * SpriteMappingEntry::WIDTH;
        base += x;
        return base;
    };

    indexedColor* SDLpixels;
    int pitch;
    if (!SDL_LockTexture(mappings_texture, nullptr, reinterpret_cast<void**>(&SDLpixels), &pitch)) {
        SDL_Log("Couldn't lock tile texture: %s", SDL_GetError());
        return false;
    };

    const int number_of_entries = static_cast<int>(SpriteMappingEntry::mappingPixels.size());
    for (int e = 0; e < number_of_entries; ++e) {
        auto pixels = SpriteMappingEntry::mappingPixels[e];

        for (int j = 0; j < SpriteMappingEntry::WIDTH; ++j) {
            const auto& row = pixels[j];
            for (int i = 0; i < SpriteMappingEntry::WIDTH; ++i) {
                const u8 p = row[i];
                const auto pixId = chunkToId(e, i, j);
                SDLpixels[pixId] = p;
                SDLpixels[pixId+SpriteMappingEntry::WIDTH] = p+0x40;
            }
        }
    }

    SDL_UnlockTexture(mappings_texture);
    return true;
}

static bool partialTileUpdate() {
    indexedColor* SDLpixels;
    int pitch;

    if (!SDL_LockTexture(tiles_texture, nullptr, reinterpret_cast<void**>(&SDLpixels), &pitch)) {
        SDL_Log("Couldn't lock tile texture: %s", SDL_GetError());
        return false;
    };

    pitch /= sizeof(indexedColor);

    constexpr auto t_pixels_per_row = Tile::WIDTH * 8;
    constexpr auto t_pixels_per_tile_row = t_pixels_per_row * Tile::WIDTH;
    assert(pitch == t_pixels_per_row);

    auto tileToId = [](const int c, const long x, const long y) {
        long base = c * t_pixels_per_tile_row;
        base += y * t_pixels_per_row;
        base += x;
        return base;
    };

    for (const int tile_to_update : gameData.newly_updated_tiles) {
        SDL_Rect tile_area {
            .x = 0,
            .y = tile_to_update*Tile::WIDTH,
            .w = 8 * Tile::WIDTH,
            .h = Tile::WIDTH
        };

        const auto& pixels = gameData.tileset.tiles[tile_to_update].pixels;
        for (int j = 0; j < Tile::WIDTH; ++j) {
            const auto& row = pixels[j];
            for (long offset = 0; offset < 8; offset++) {
                for (int i = 0; i < Tile::WIDTH; ++i) {
                    const u8 p = row[i];

                    const long id = tileToId(tile_to_update, i + offset*Tile::WIDTH, j);
                    SDLpixels[id] = p + offset*0x10;
                }
            }
        }
    }


    SDL_UnlockTexture(tiles_texture);



    if (!SDL_LockTexture(chunks_texture, nullptr, reinterpret_cast<void**>(&SDLpixels), &pitch)) {
        SDL_Log("Couldn't lock tile texture: %s", SDL_GetError());
        return false;
    };

    pitch /= sizeof(indexedColor);

    constexpr auto pixels_per_row = Chunk::WIDTH * 16;
    constexpr auto pixels_per_chunk_row = pixels_per_row * Chunk::WIDTH;
    assert(pitch == pixels_per_row);

    auto chunkToId = [](const int c, const long x, const long y) {
        const long chunk_row = c / 8;
        const long chunk_col = (c % 8) * 2;
        long base = chunk_row * pixels_per_chunk_row;
        base += y * pixels_per_row;
        base += chunk_col * Chunk::WIDTH;
        base += x;
        return base;
    };

    for (const int chunk_to_update : gameData.chunksToUpdate()) {
        // SDL_Rect chunk_area {
        //     .x = (chunk_to_update%8)*Chunk::WIDTH*2,
        //     .y = (chunk_to_update/8)*Chunk::WIDTH,
        //     .w = 2 * Chunk::WIDTH,
        //     .h = Chunk::WIDTH
        // };

        const auto pixels = gameData.chunks.getBytes(chunk_to_update, gameData.blocks, gameData.tileset);

        for (int j = 0; j < Chunk::WIDTH; ++j) {
            const auto& row = pixels[j];
            for (int i = 0; i < Chunk::WIDTH; ++i) {
                const u8 p = row[i];
                const auto pixId = chunkToId(chunk_to_update, i, j);
                SDLpixels[pixId] = p;
                SDLpixels[pixId+Chunk::WIDTH] = p+0x40;
            }
        }
    }

    SDL_UnlockTexture(chunks_texture);
    return true;
}

/*
def getAddr(x):
    bytes = [''.join(x) for x in itertools.batched(f'{x:032b}', 8)]
    a13 = bytes[0][2:]
    a7 = bytes[1]
    a15 = bytes[3][-2:]
    print(bytes)
    print(a15)
    print(a13)
    print(a7)
    print('0b' + a15 + a13 + a7)
    return eval('0b' + a15 + a13 + a7)
 */

/*
x = [
0.000123,
0.000038,
0.000014,
0.000222,
0.000087,
0.000026,
0.000218,
0.000016,
0.001421,
0.007397,
0.000935,
0.000020,
0.000002,
0.002207,
0.198611,
]

x[-1] - sum(x[:-1])
 */


static float progress = 0.0f;

s32 previousFrameLag = 0;

static updateResult update_data() {
    s32 flags = 0;
    progress += std::pow(1.5f, speed);
    if (!skipToNextLevel) {
        if (progress < 1.0f)
            return UPDATE_SUCCESS;
        while (progress >= 1.0f) {
            progress -= 1.0f;
            flags |= getNextFrame(inputStream);
        }
    } else {
        const u16 currentZoneAct = gameData.currentZoneAct;
        while (currentZoneAct == gameData.currentZoneAct) {
            flags |= getNextFrame(inputStream);
            if (flags & FRAME_EOF)
                break;
        }
        skipToNextLevel = false;
        flags &= ~LAG_FRAME;
    }

    if (flags & FRAME_EOF)
        return UPDATE_EOF;

    if (flags & LAG_FRAME) {
        redrawLevel = false;
        previousFrameLag |= (flags &= LAG_FRAME);
        return UPDATE_LAG_FRAME;
    }
    flags |= previousFrameLag;
    previousFrameLag = 0;

    if (flags < 0) {
        redrawLevel = false;
        return UPDATE_SUCCESS;
    }
    RenderingData::clearCaches();

    if (flags & (FULL_TILE_UPDATED | CHUNK_LIST_UPDATED | BLOCK_LIST_UPDATED)) {
        if (!fullTileUpdate()) {
            fprintf(stderr, "Failed to build tile full update\n");
            return UPDATE_FAILURE;
        }
        if (!fullChunkUpdate()) {
            fprintf(stderr, "Failed to build chunk full update\n");
            return UPDATE_FAILURE;
        }
        gameData.setChunkDependecies();
    }
    else if (flags & PARTIAL_TILE_UPDATED) {
        if (!partialTileUpdate()) {
            fprintf(stderr, "Failed to build tile partial update\n");
            return UPDATE_FAILURE;
        }
    }

    if (flags & (RING_MAP_UPDATED | FULL_TILE_UPDATED | PARTIAL_TILE_UPDATED)) {
        gameData.ring_data.setBytes(gameData.tileset);
    }

    if (flags & SPRITE_DATA_UPDATED) {
        if (!updateMappings()) {
            fprintf(stderr, "Failed to build mapping update\n");
            return UPDATE_FAILURE;
        }
    }
    if (flags & COLOR_UPDATED) {
        if (!updatePalette()) {
            fprintf(stderr, "Failed to update palette\n");
            return UPDATE_FAILURE;
        }
    }

    gameData.newly_updated_tiles.clear();
    redrawLevel = true;
    return UPDATE_SUCCESS;
}


static bool redrawAll() {
    if (!redrawLevel) {
        return true;
    }
    if (!drawToLevel(false)) {
        return false;
    };
    if (!drawToLevel(true)) {
        return false;
    };
    if (!drawToBackground(false)) {
        return false;
    };
    if (!drawToBackground(true)) {
        return false;
    };
    if (!drawSprites(false)) {
        return false;
    };
    if (!drawSprites(true)) {
        return false;
    };
    if (!drawRings()) {
        return false;
    }
    redrawLevel = false;
    return true;
}

static bool render_master_texture() {
    SDL_FRect dst = {
        .x = 0.0f,
        .y = 0.0f,
        .w = RENDER_WIDTH,
        .h = RENDER_HEIGHT
    };
    SDL_FRect src = {
        .x = fmodf(scrollX, Chunk::WIDTH),
        .y = fmodf(scrollY, Chunk::WIDTH),
        .w = RENDER_WIDTH / scale,
        .h = RENDER_HEIGHT / scale
    };

    if (scrollX < 0) {
        dst.x = -src.x;
        src.x = 0.0f;
    }
    if (scrollY < 0) {
        dst.y = -src.y;
        src.y = 0.0f;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 64, 128);
    const SDL_FRect water_area{
        .x = 0,
        .y = (gameData.water_line - scrollY),
        .w = RENDER_WIDTH,
        .h = RENDER_HEIGHT
    };

    SDL_RenderFillRect(renderer, &water_area);

    for (int i = 0; i < RENDER_TARGET_COUNT; ++i) {
        if (renderFlags & (1 << i)) {
            SDL_RenderTexture(renderer, *texturesToRender[i], &src, &dst);
        }
    }
    return true;
}

static u64 last_ticks = SDL_GetTicks();
static float delta_time = 0.0f;

#define MSG_GEN_PAIR(x) {&renderToggleTimers[x], [] { return std::format("Rendering {}: {}", renderToggleNames[x], (renderFlags & (1 << x)) ? "On" : "Off"); } }

using debugTimerMsgPair = std::pair<float*, std::function<std::string()>>;
std::vector<debugTimerMsgPair> messageData = {
    {&SpeedChangeMsgTimer, [] { return std::format("Speed Changed to {:.2f}", std::pow(1.5f, speed)); }},
    {&FileChangeMsgTimer, [] { return std::format("File Change {}", binFiles[fileIndex]); }},
    MSG_GEN_PAIR(0),
    MSG_GEN_PAIR(1),
    MSG_GEN_PAIR(2),
    MSG_GEN_PAIR(3),
    MSG_GEN_PAIR(4),
};

static bool renderMessages(float delta_time) {
    float y = 32.0f;
    constexpr float x = 32.0f;

    auto hasMsg = [](const float* timer) { return *timer > 0.0f; };

    auto msgCount = stdr::count_if(
        messageData,
        hasMsg,
        &debugTimerMsgPair::first );

    if (msgCount == 0)
        return true;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0 , 255);
    const SDL_FRect dst = {
        .x = 30.f,
        .y = 30.f,
        .w = 400.f,
        .h = msgCount * 16.f
    };
    SDL_RenderFillRect(renderer, &dst);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    for (const auto& [timer, msgGen] : messageData) {
        if (*timer <= 0.0f)
            continue;
        *timer -= delta_time;
        auto msg = msgGen();
        if (!SDL_RenderDebugText(renderer, x, y, msg.c_str())) {
            return false;
        }
        y += 16.f;
    }

    return true;
}

static SDL_Surface* previousFrame = nullptr;

/* This function runs once per frame, and is the heart of the program. */
extern "C" SDL_AppResult SDL_AppIterate(void *appstate) {
    const u64 current_ticks = SDL_GetTicks();
    // Delta time in seconds
    delta_time = static_cast<float>(current_ticks - last_ticks) / 1000.0f;
    last_ticks = current_ticks;
    // printf("DT: %f\n", 1/delta_time);

    bool success = true;
    if (!pauseData) {
        const auto res = update_data();
        if (res == UPDATE_LAG_FRAME) {
            fprintf(stderr, "LAG FRAME\n");
            // return SDL_APP_CONTINUE;
        }
        if (res == UPDATE_FAILURE) {
            fprintf(stderr, "Update Failure\n");
            return SDL_APP_FAILURE;
        }
        if (FFMPEG_DATA && (res == UPDATE_EOF)) {
            return SDL_APP_SUCCESS;
        }
    }

    scrollX = static_cast<float>(gameData.scroll_x - RENDER_WIDTH/2);
    if (scrollX < 0)
        scrollX = 0;
    if (scrollX >= gameData.level_chunks[0].size() * Chunk::WIDTH - RENDER_WIDTH)
        scrollX = gameData.level_chunks[0].size() * Chunk::WIDTH - RENDER_WIDTH;

    if (gameData.level_chunks.size() * 128 <= RENDER_HEIGHT) {
        scrollY = 0;
    } else {
        scrollY = static_cast<float>(gameData.scroll_y - RENDER_HEIGHT/2);
    }
    if (gameData.screen_min_y < 0 && scrollY < 0) {
        scrollY += gameData.vertical_loop;
    }



    const auto ticks = SDL_GetTicks();
    const float seconds = static_cast<float>(ticks)/1000;

    if constexpr (FFMPEG_DATA) {
        if (previousFrame) {
            for (int i = 0; i < gameData.lagFrames; ++i) {
                fwrite(previousFrame->pixels, 1, FFMPEG_BYTES_PER_FRAME, ffmpegProcess);
            }
        }

    }

    if (!redrawAll()) {
        fprintf(stderr, "Redraw Failure\n");
        return SDL_APP_FAILURE;
    }

    if constexpr (FFMPEG_DATA) {
        if constexpr (!FFMPEG_OUTPUT_RESOLUTION) {
            SDL_SetRenderTarget(renderer, ffmpeg_texture);
        }
        SDL_SetRenderDrawColor(renderer, 32, 0, 32, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
        success = render_master_texture();
        if (previousFrame)
            SDL_DestroySurface(previousFrame);
        previousFrame = SDL_RenderReadPixels(renderer, nullptr);

        SDL_SetRenderTarget(renderer, nullptr);
        SDL_RenderPresent(renderer);

        fwrite(previousFrame->pixels, 1, FFMPEG_BYTES_PER_FRAME, ffmpegProcess);
        return SDL_APP_CONTINUE;
    } else {
        SDL_SetRenderDrawColor(renderer, 32, 0, 32, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
        success = render_master_texture();
        SDL_SetRenderLogicalPresentation(renderer, RENDER_WIDTH, RENDER_HEIGHT, SDL_LOGICAL_PRESENTATION_DISABLED);
        if (!renderMessages(delta_time)) {
            return SDL_APP_FAILURE;
        }
        SDL_SetRenderLogicalPresentation(renderer, RENDER_WIDTH, RENDER_HEIGHT, SDL_LOGICAL_PRESENTATION_STRETCH);
        SDL_RenderPresent(renderer);



        return success ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
    }
}


/* This function runs once at shutdown. */
extern "C" void SDL_AppQuit(void * /*appstate*/, SDL_AppResult  /*result*/)
{
    SDL_DestroyPalette(low_prio_palette);
    SDL_DestroyPalette(high_prio_palette);

    SDL_DestroyTexture(tiles_texture);
    SDL_DestroyTexture(chunks_texture);
    SDL_DestroyTexture(mappings_texture);
    SDL_DestroyTexture(level_texture_high);
    SDL_DestroyTexture(level_texture_low);
    SDL_DestroyTexture(bg_texture_low);
    SDL_DestroyTexture(bg_texture_high);
    SDL_DestroyTexture(sprite_texture_high);
    SDL_DestroyTexture(sprite_texture_low);
    SDL_DestroyTexture(sprite_tmp_texture);
    SDL_DestroyTexture(rings_texture);
    SDL_DestroyTexture(rings_texture_water);
    SDL_DestroyTexture(ffmpeg_texture);
    SDL_DestroyTexture(translucency_mask_texture);
    if (ffmpegProcess)
        pclose(ffmpegProcess);
    // SDL_ReleaseGPUGraphicsPipeline(device, LinePipeline);
}