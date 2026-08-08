//
// Created by alexoxorn on 7/23/26.
//

#ifndef SONIC3ATLUS_CONSTS_H
#define SONIC3ATLUS_CONSTS_H

//
// Created by alexoxorn on 7/23/26.
//


#include <utility>
#include "common.h"
#include "structs.h"
#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_render.h"



constexpr inline auto HOST = "127.0.0.1";
constexpr inline auto PORT = 5000;


#define R_HD {1920, 1080}
#define R_2K {2560, 1440}
#define R_4K {3840, 2160}
#define R_8K {3840*2, 2160*2}

#define R_HEIGHT_TO_WIDTH(x) (16 * (x) / 9)
#define R_MAX_HEIGHT 0x1000
#define R_FROM_HEIGHT(x) {R_HEIGHT_TO_WIDTH(x), x}
#define R_FULL R_FROM_HEIGHT(R_MAX_HEIGHT)

constexpr std::pair<int, int> scale16(const std::pair<int, int> &in) {
    return {in.first / 16 * 16 + 16, in.second / 16 * 16 + 16};
}

constexpr inline std::pair GENESIS_RESOLUTION(320, 224);
extern std::pair<int, int> INTERNAL_RESOLUTION;
constexpr inline std::pair OUTPUT_RESOLUTION = R_2K;
inline constexpr std::optional<std::pair<const char*, const char*>> FFMPEG_DATA = std::nullopt;
// inline constexpr std::optional FFMPEG_DATA = std::pair{"../SocketBins/02-Draft1.bin", "../output/Zone1to2.mp4"};

#define WINDOW_WIDTH OUTPUT_RESOLUTION.first
#define WINDOW_HEIGHT OUTPUT_RESOLUTION.second
#define RENDER_WIDTH INTERNAL_RESOLUTION.first
#define RENDER_HEIGHT INTERNAL_RESOLUTION.second

/*
 Color Format
 0000'000T'PWCC'XXXX
 T      : Transparent Bit       (Used to replace dithered pixels with half transparent pixels)
 P      : Priority Bit          (High priority)
 W      : Water Bit             (Underwater palette)
 CC     : Palette Line          (Which of the 4 Genesis palette lines)
 XXXX   : Color Index           (The index into the palette line)
 */
constexpr inline auto  INDEXED_COLOR_16 = static_cast<SDL_PixelFormat>(SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX8, 0, 0, 16, 2));

#define horizontalChunksPerScreen (RENDER_WIDTH / Chunk::WIDTH + 2)
#define verticalChunksPerScreen (RENDER_HEIGHT / Chunk::WIDTH + 2)

constexpr inline u32 RENDER_BACKGROUND_LOW = 1 << 0;
constexpr inline u32 RENDER_FOREGROUND_LOW = 1 << 1;
constexpr inline u32 RENDER_SPRITE_LOW = 1 << 2;
constexpr inline u32 RENDER_RINGS = 1 << 3;
constexpr inline u32 RENDER_BACKGROUND_HIGH = 1 << 4;
constexpr inline u32 RENDER_FOREGROUND_HIGH = 1 << 5;
constexpr inline u32 RENDER_SPRITE_HIGH = 1 << 6;
constexpr inline u32 RENDER_COUNT = 7;

constexpr inline s32 SCREEN_UPDATED = 1 << 0;
constexpr inline s32 COLOR_UPDATED = 1 << 1;
constexpr inline s32 FULL_TILE_UPDATED = 1 << 2;
constexpr inline s32 PARTIAL_TILE_UPDATED = 1 << 3;
constexpr inline s32 BLOCK_LIST_UPDATED = 1 << 4;
constexpr inline s32 CHUNK_LIST_UPDATED = 1 << 5;
constexpr inline s32 LEVEL_DATA_UPDATED = 1 << 6;
constexpr inline s32 SPRITE_DATA_UPDATED = 1 << 7;
constexpr inline s32 LOOP_DATA_UPDATED = 1 << 8;
constexpr inline s32 WATER_LV_UPDATED = 1 << 9;
constexpr inline s32 POSITION_UPDATED = 1 << 10;
constexpr inline s32 RING_POS_UPDATED = 1 << 11;
constexpr inline s32 RING_STATS_UPDATED = 1 << 12;
constexpr inline s32 RING_MAP_UPDATED = 1 << 13;
constexpr inline s32 LAG_FRAME = 1 << 14;
constexpr inline s32 GAME_PAUSED = 1 << 15;
constexpr inline s32 FRAME_EOF = 1 << 15;

constexpr inline u8 ANGLE_ISLAND_ZONE = 0;
constexpr inline u8 HYDRO_CITY_ZONE = 1;
constexpr inline u8 MARBLE_GARDEN_ZONE = 2;
constexpr inline u8 CARNIVAL_NIGHT_ZONE = 3;
constexpr inline u8 FLYING_BATTERY_ZONE = 4;
constexpr inline u8 ICE_CAP_ZONE = 5;
constexpr inline u8 LAUNCH_BASE_ZONE = 6;
constexpr inline u8 MUSHROOM_HILL_ZONE = 7;
constexpr inline u8 SANDOPOLIS_ZONE = 8;
constexpr inline u8 LAVA_REEF_ZONE = 9;
constexpr inline u8 SKY_SANCTUARY_ZONE = 0xA;
constexpr inline u8 DEATH_EGG_ZONE = 0xB;
constexpr inline u8 DOOMSDAY_ZONE = 0xC;
constexpr inline u8 HIDDEN_PALACE_ZONE = 0x16;
constexpr inline u8 FINAL_BOSS_ZONE = 0x17;

constexpr inline u8 AIZ_FLYING_BOMB_SHIP = 0x2;

#define LEVEL_ACT(LVL, ACT) (\
    (LVL == LAVA_REEF_ZONE && ACT == 3) ? (HIDDEN_PALACE_ZONE << 8) : \
    (LVL == HIDDEN_PALACE_ZONE && ACT <= 1) ? ((HIDDEN_PALACE_ZONE << 8) + 1) : \
    (LVL == DEATH_EGG_ZONE && ACT == 3) ? (FINAL_BOSS_ZONE << 8) : \
    ((LVL << 8) + (ACT - 1))\
   )

#define LEVEL_ACT_EVENT(LVL, ACT, EVENT) (\
    (LEVEL_ACT(LVL, ACT) << 16) + EVENT\
)

constexpr inline auto LEVEL_HIGH = 0b10;
constexpr inline auto LEVEL_BG = 0b1;

constexpr inline auto SPRITE_DIMS = 256;

constexpr inline auto TILE_SIZE = 8;
constexpr inline auto PALETTE_COUNT = 8;
constexpr inline auto PALETTE_SIZE = 0x10;
constexpr inline auto SPRITE_WIDTH = 256;

constexpr inline auto SPRITE_PER_TMP_TEXTURE_ROW = 16;
constexpr inline auto MAX_SPRITE_TMP_TEXTURE = SPRITE_PER_TMP_TEXTURE_ROW * SPRITE_PER_TMP_TEXTURE_ROW;

constexpr inline auto MAPPING_ENTRY_PER_ROW = 0x10;
constexpr inline auto MAX_MAPPING_ENTRIES = 0x2000;
constexpr inline auto MAPPING_ENTRY_ROWS = MAX_MAPPING_ENTRIES/MAPPING_ENTRY_PER_ROW;


#define SCREEN_TEXTURE_WIDTH (horizontalChunksPerScreen * Chunk::WIDTH)
#define SCREEN_TEXTURE_HEIGHT (verticalChunksPerScreen * Chunk::WIDTH)
constexpr inline int MAX_LEVEL_HEIGHT = 0x1000;
#define LOOP_COUNT (std::max((RENDER_HEIGHT / 0x800), 1))

extern float renderToggleTimers[RENDER_COUNT];
extern const std::string_view renderToggleNames[RENDER_COUNT];


extern u32 renderFlags;



inline constexpr bool FFMPEG_OUTPUT_RESOLUTION = true;
inline constexpr auto FFMPEG_WIDTH = FFMPEG_OUTPUT_RESOLUTION ? WINDOW_WIDTH : RENDER_WIDTH;
inline constexpr auto FFMPEG_HEIGHT = FFMPEG_OUTPUT_RESOLUTION ? WINDOW_HEIGHT: RENDER_HEIGHT;
inline constexpr auto FFMPEG_BYTES_PER_FRAME = FFMPEG_WIDTH * FFMPEG_HEIGHT * sizeof(u32);
inline constexpr auto pixelsPerRow = 2 * MAPPING_ENTRY_PER_ROW * SpriteMappingEntry::WIDTH;

constexpr inline auto RENDER_TARGET_COUNT = std::size(renderToggleNames);
using indexedColor = u8;
inline constexpr auto pixelFormat = sizeof(indexedColor) == 1
    ? SDL_PIXELFORMAT_INDEX8
    : static_cast<SDL_PixelFormat>(SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_INDEX8, 0, 0, 16, 2));
inline constexpr auto SCALE_MODE = SDL_SCALEMODE_LINEAR;


extern SDL_Palette* low_prio_palette;
extern SDL_Palette* high_prio_palette;
extern SDL_Palette* transparency_mask_palette;
extern std::array<SDL_Texture*, 4> level_textures;
extern SDL_Texture* tiles_texture;
extern SDL_Texture* chunks_texture;
extern SDL_Texture* mappings_texture;
extern SDL_Texture* sprite_texture_high;
extern SDL_Texture* sprite_texture_low;
extern SDL_Texture* sprite_tmp_texture;
extern SDL_Texture* rings_texture;
extern SDL_Texture* rings_texture_water;
extern SDL_Texture* ffmpeg_texture;
extern SDL_Texture* translucency_mask_texture;
extern SDL_Texture* make_transparent_mask;
extern SDL_Texture* fullscreen_texture;
extern SDL_Window    *window;
extern SDL_Renderer  *renderer;
extern SDL_BlendMode transparencyBlend;
extern SDL_BlendMode mixTwoHalfBlend;
extern SDL_BlendMode makeTransparentBlend;

#define level_texture_low (level_textures[0])
#define level_texture_high (level_textures[LEVEL_HIGH])
#define bg_texture_low (level_textures[LEVEL_BG])
#define bg_texture_high (level_textures[LEVEL_HIGH | LEVEL_BG])

extern SDL_Texture** texturesToRender[RENDER_COUNT];

static_assert(RENDER_COUNT == std::size(renderToggleTimers));
static_assert(RENDER_COUNT == std::size(texturesToRender));
static_assert(RENDER_COUNT == std::size(texturesToRender));

extern bool redrawLevel;

extern SDL_GPUDevice* device;

extern float scrollX;
extern float scrollY;
extern float scale;
extern float speed;
extern bool pauseData;

extern float SpeedChangeMsgTimer;
extern float FileChangeMsgTimer;
extern bool skipToNextLevel;

extern int inputFD;
extern FILE* inputStream;
extern RenderingData gameData;
extern std::string frameError;


#endif //SONIC3ATLUS_CONSTS_H
