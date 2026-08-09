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

/*
Reads Data Packets from a FILE* (be it a binary file or a input socket)
and updates the global `gameData` variable with the relevant data for the current frame.
Reading will continue until either end of file, or it reads a "DONE____" packet.
The function will then return a bit flag representing which data has been updated.

Data Packets Start With an 8 Character ASCII Identifier Identifying the Type of Packet
Binary Data is then read according to the packet data, which each different type
defining how many bytes to read, and what they represent.

First, a common set of data that is reused is what I will call a
"VRAM" entry.

The "VRAM entry" is 2 bytes with the bitfield format
PCCY'XAAA AAAA'AAAA
P: Priority Bit
CC: Palette Line
Y: Vertical Flip
X: Horizotal Flip
AAA'AAAA'AAAA: Tile index (Vram Address/32)


---------------------
SCRN_POS
---------------------
Screen Position
Read 4 Two Byte unsigned integers in the order
Plane-A X position
Plane-A Y position
Plane-B X position
Plane-B Y position

---------------------
COLORTST
---------------------
Color Palette
Reads 256 bytes, broken up into

2 Palettes (above ground and underwater) with
4 Palette Lines of
16 Colors, each
2 bytes in the format 0000'BBB0 GGG0'RRR0

---------------------
TILE_TST
---------------------
Reads 64KB as in the entirety of VRAM
Uses these to build the current tileset.
The tileset consists of

2048 (0x800) Tiles each with
8 Rows of
4 bytes,
    with the 8 nibbles representing the palette
    index for the 8 pixels in that row

---------------------
TILE_LST (deprecated)
---------------------
Reads an arbitrary list of updated tiles.
The tile index to update is read (2 bytes LE),
followed by the 32 bytes for the tile data.

This continue until a tile index of -1 (0xFFFF) is read
At which point the reading stops.

This will also update the `newly_updated_tiles`
To better optimize chunk rerendering.

---------------------
VRAM_SET
---------------------
Representing Tile updates due to writes
to the VDP data register

Reads a destination tile index (2 bytes LE)
and a number of tiles to update (2 bytes LE)

Then will read that number of tiles (32 bytes each)
and update the tiles from the tileset
starting from the destination index read.

This will also update the `newly_updated_tiles`
To better optimize chunk rerendering.

---------------------
VRAM_DMA
---------------------
Representing Tile updates setting up
DMAs to VRAM

At present, functionally identical to VRAM_SET

---------------------
VRAMSET2 (deprecated)
---------------------
Same as VRAM_SET except that instead of reading
the tile destination index and number of tiles,

the byte destination index and number of bytes are read.
As a result, both need to be divided by 32 (0x20)
to get the tile index and length

---------------------
BLOCKTST
---------------------
Reads 6144 (0x1800) bytes for the block map.

The block map consists of

768 (0x300) Blocks each with
4 Cells (2 x 2 grid) of
2 byte VRAM entries.


---------------------
CHUNKTST
---------------------
Reads 32768 (0x8000) bytes for the chunk map.

The chunk map consists of

256 (0x100) Chunks each with
64 Cells (8 x 8 grid) of
2 bytes each.

The bit field breakdown of Chunk Cell Data
SSTT'YXII IIII'IIII
SS: Alt Layer Solidity (Unused)
TT: Main Layer Solidity (Unused)
Y: Vertical Flip
X: Horizotal Flip
II'IIII'IIII: Block Index

---------------------
H_SCROLL (UNUSED)
---------------------
Meant to read the line index of Horizontal Interrupts.

---------------------
LVLDAT_(A/B)
---------------------
Reads the level layout (in the form of chunk id grid)
for either the foreground (A) or background (B)

First read the dimensions of the level
- Columns (2 bytes - signed - LE)
- Rows (2 bytes - signed - LE)

Then reads then next (Columns * Rows) bytes,
in row-major order
with each byte representing a chunk ID.


---------------------
SPRITE_2
---------------------
First reads a 4 byte ascii instruction.

If 'NEXT'
    The next 74 (0x4A) bytes are the raw data
    for a sprite in the Object_Status_Table
    (See https://info.sonicretro.org/SCHG:Sonic_the_Hedgehog_3_%26_Knuckles/RAM_Editing#Object_Status_Table_Format)

    The read the current mapping data for the sprite the current frame.
    (Instructions on how to build the sprite graphics)

    First the number of entries is read (2 bytes, signed, LE)
    Then read the 6 bytes per entry.

    the nibble field breakdown of each entry is
    YYSS VVVV XXXX
    YY: Y Position (signed BE)
    SS: Shape [0000 wwhh: (ww + 1) x (hh + 1)]
    VVVV: Vram Entry
    XXXX: X Position (signed BE)

    If the sprite is a compound sprite
    (bit 6 of byte 4 of the sprite status entry is set)

    Then the next 2 bytes (LE) are the number of subsprites for the object.
    For each subsprite, the data is in the same format as above for reading
    the current mapping data

If 'DONE'
There are no more sprites to process


---------------------
LOOPDATA
---------------------
The next 5 groups of 2 bytes represent (all signed LE)
- The vertical loop value (used to represent the length of the loop)
- The minimum horizontal screen position
- The minimum vertical screen position (if < 0, vertically looping stage)
- the maximum horiztonal screen position
- the maximum vertical screen position


---------------------
WATER_LV
---------------------
1 byte flag representing if the current level has water
2 bytes (LE) representing the current vertical coordinte of the water's height

---------------------
POSITION
---------------------
2 bytes (LE) for Player 1's horizontal position
2 bytes (LE) for Player 1's vertical position

---------------------
RING_POS
---------------------
Read the ring location data (save for the initial 0000'0000)

The following data is grouped into 4 bytes,
with the first 2 bytes (signed LE) representing the x position of the ring
and the next 2 bytes (signed LE) representing the y position of the ring

if the x position is read as -1 (0xFFFF) then the end of the list has been reached
and the y position is NOT read.

---------------------
RINGSTAT
---------------------
The first byte represent the current animation frame for all uncollected
static rings.

The next 1022 (0x3fe) bytes are the status of each
of the 511 rings grouped into 2 bytes.

If the ring is uncollected, its status is 0x0000
If the ring is fully collected, its status if 0xFFFF
If the ring is being collected, the upper bytes acts as a timer,
with the lower byte representing which animation frame to use

---------------------
RING_MAP
---------------------
Mapping Data for rings (How ring graphics are built)

Read 64 bytes
Representing 8 frame,
which 8 bytes of data per frame.

Those 8 bytes are broken into
- YPosition: 2 bytes (signed BE)
- Empty Byte
- Size 0000wwhh [(ww + 1) x (hh + 1)]
- VRAM Entry: 2 bytes
- XPosition: 2 bytes (signed BE)

---------------------
EVENTDAT
---------------------
1 byte for the Current Zone
1 byte for the Current Act

2 bytes (LE) for the current background event
2 bytes (LE) for the current foreground event

2 bytes (LE) for a generic background event variable
6 * 2 bytes (LE) for 6 different generic foreground event variables
2 bytes (LE) special variable used during LBZ Death Egg Event

---------------------
IS_PAUSE
---------------------
2 bytes for if the game is currently paused

---------------------
LAGFRAME (Unused)
---------------------

---------------------
LAGCOUNT
---------------------
2 bytes (LE) for how many lag frames have occured since the last update
Used for when outputing to a video, and you want to duplicate frames

---------------------
DONE____
---------------------

Marks that all game updates are done for the frame, and is ready
to render the data to the screen.

If however the game is currently paused, this acts as a NOP.

*/


static s32 getNextFrame(FILE* fd) {
    char msg[9] = {};
    char dump[128];
    size_t count;
    s32 flags = 0;
    while ((count = recvStrict(fd, msg, 8)) > 0) {
        // printf("msg: %8s\n", msg);
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
            if (!gameData.gamePaused && !gameData.level_chunks.empty()) {
                return flags;
            }
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
            gameData = RenderingData{};
            FileChangeMsgTimer = 3.0f;
        }
        if (event->key.key == SDLK_N) {
            fileIndex -= 1;
            fileIndex = fileIndex < 0 ? fileIndex + static_cast<int>(binFiles.size()) : fileIndex;
            operFile(fileIndex);
            gameData = RenderingData{};
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



static float progress = 0.0f;

s32 previousFrameLag = 0;

static std::pair<int, int> level_size_to_resolution() {
    int chunk_height = gameData.level_chunks.size();
    int pixel_height = chunk_height * Chunk::WIDTH;
    std::pair resolution = scale16(R_FROM_HEIGHT(pixel_height));
    return resolution;
}

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

    if (auto new_resolution = level_size_to_resolution(); new_resolution != INTERNAL_RESOLUTION) {
        INTERNAL_RESOLUTION = new_resolution;
        SDL_SetRenderLogicalPresentation(renderer, RENDER_WIDTH, RENDER_HEIGHT, SDL_LOGICAL_PRESENTATION_OVERSCAN);
        initDestTextures();
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
        .w = static_cast<float>(RENDER_WIDTH),
        .h = static_cast<float>(RENDER_HEIGHT)
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

    SDL_SetRenderTarget(renderer, fullscreen_texture);

    SDL_SetRenderDrawColor(renderer, 32, 0, 32, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 0, 0, 64, 128);
    const SDL_FRect water_area{
        .x = 0,
        .y = (gameData.water_line - scrollY),
        .w = static_cast<float>(RENDER_WIDTH),
        .h = static_cast<float>(RENDER_HEIGHT)
    };

    if (gameData.has_water)
        SDL_RenderFillRect(renderer, &water_area);

    for (int i = 0; i < RENDER_TARGET_COUNT; ++i) {
        if (renderFlags & (1 << i)) {
            SDL_RenderTexture(renderer, *texturesToRender[i], &src, &dst);
        }
    }


    SDL_FRect full_src = {
        .x = 0,
        .y = 0,
        .w = static_cast<float>(RENDER_WIDTH),
        .h = static_cast<float>(RENDER_HEIGHT)
    };

    SDL_RenderTexture(renderer, make_transparent_mask, &full_src, &dst);

    SDL_SetRenderTarget(renderer, nullptr);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_SetTextureBlendMode(fullscreen_texture, SDL_BLENDMODE_NONE);
    SDL_RenderTexture(renderer, fullscreen_texture, &full_src, &dst);
    dst.x += 1;
    SDL_SetTextureBlendMode(fullscreen_texture, mixTwoHalfBlend);
    SDL_RenderTexture(renderer, fullscreen_texture, &full_src, &dst);

    SDL_FRect screenDim = {
        .x = static_cast<float>(gameData.screen_position_A.first - scrollX),
        .y = static_cast<float>(gameData.screen_position_A.second - scrollY),
        .w = static_cast<float>(GENESIS_RESOLUTION.first),
        .h = static_cast<float>(GENESIS_RESOLUTION.second)
    };
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderRect(renderer, &screenDim);
    screenDim.x -= 1;
    screenDim.y -= 1;
    screenDim.w += 2;
    screenDim.h += 2;
    SDL_RenderRect(renderer, &screenDim);


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

    if (gameData.scroll_x > 0 && gameData.scroll_y > 0) {
        scrollX = static_cast<float>(gameData.scroll_x - RENDER_WIDTH/2);
        if (scrollX < 0)
            scrollX = 0;
        if (!gameData.level_chunks.empty()) {
            if (scrollX >= gameData.level_chunks[0].size() * Chunk::WIDTH - RENDER_WIDTH)
                scrollX = gameData.level_chunks[0].size() * Chunk::WIDTH - RENDER_WIDTH;

            if (gameData.level_chunks.size() * 128 <= RENDER_HEIGHT) {
                scrollY = 0;
            } else {
                scrollY = static_cast<float>(gameData.scroll_y - RENDER_HEIGHT/2);
            }
        } else {
            scrollX = 0;
            scrollY = 0;
        }
        if (gameData.screen_min_y < 0 && scrollY < 0) {
            scrollY += gameData.vertical_loop;
        }
    }



    const auto ticks = SDL_GetTicks();
    const float seconds = static_cast<float>(ticks)/1000;

    if constexpr (FFMPEG_DATA) {
        if (previousFrame) {
            if (gameData.lagFrames && gameData.level_chunks.size() > 0) {
                printf("\nLAG x %d\n", gameData.lagFrames);
            }
            for (int i = 0; i < gameData.lagFrames; ++i) {
                fwrite(previousFrame->pixels, 1, FFMPEG_BYTES_PER_FRAME, ffmpegProcess);
            }
        }

    }

    if (!redrawAll()) {
        fprintf(stderr, "Redraw Failure\n");
        return SDL_APP_FAILURE;
    }


    success = render_master_texture();
    SDL_SetRenderLogicalPresentation(renderer, RENDER_WIDTH, RENDER_HEIGHT, SDL_LOGICAL_PRESENTATION_DISABLED);
    if (!renderMessages(delta_time)) {
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, RENDER_WIDTH, RENDER_HEIGHT, SDL_LOGICAL_PRESENTATION_OVERSCAN);

    if constexpr (FFMPEG_DATA) {
        if (previousFrame)
            SDL_DestroySurface(previousFrame);
        previousFrame = SDL_RenderReadPixels(renderer, nullptr);
        fwrite(previousFrame->pixels, 1, FFMPEG_BYTES_PER_FRAME, ffmpegProcess);
    }

    SDL_RenderPresent(renderer);
    return success ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
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