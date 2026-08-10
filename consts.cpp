//
// Created by alexoxorn on 8/4/26.
//

#include "consts.h"

SDL_Palette* low_prio_palette = nullptr;
SDL_Palette* high_prio_palette = nullptr;
SDL_Palette* transparency_mask_palette = nullptr;
SDL_Palette* high_priority_mask_palette = nullptr;
SDL_Palette* low_priority_mask_palette = nullptr;
std::array<SDL_Texture*, 4> level_textures{};
SDL_Texture* tiles_texture = nullptr;
SDL_Texture* chunks_texture = nullptr;
SDL_Texture* mappings_texture = nullptr;
SDL_Texture* sprite_texture_high = nullptr;
SDL_Texture* sprite_texture_low = nullptr;
SDL_Texture* sprite_tmp_texture = nullptr;
SDL_Texture* rings_texture = nullptr;
SDL_Texture* rings_texture_water = nullptr;
SDL_Texture* ffmpeg_texture = nullptr;
SDL_Texture* translucency_mask_texture = nullptr;
SDL_GPUDevice* device = nullptr;
SDL_Texture* make_transparent_mask = nullptr;
SDL_Texture* fullscreen_texture = nullptr;

SDL_Window    *window             = nullptr;
SDL_Renderer  *renderer           = nullptr;

/* dstRGB = dstRGB  ;  dstA = dstA * srcA  */
SDL_BlendMode transparencyBlend = SDL_ComposeCustomBlendMode(
    SDL_BLENDFACTOR_ZERO,
    SDL_BLENDFACTOR_ONE,
    SDL_BLENDOPERATION_ADD,
    SDL_BLENDFACTOR_DST_ALPHA,
    SDL_BLENDFACTOR_ZERO,
    SDL_BLENDOPERATION_ADD
    );

/* dstRGB = dstRGB  ;  dstA = srcA  */
SDL_BlendMode makeTransparentBlend = SDL_ComposeCustomBlendMode(
    SDL_BLENDFACTOR_ZERO,
    SDL_BLENDFACTOR_ONE,
    SDL_BLENDOPERATION_ADD,
    SDL_BLENDFACTOR_ONE,
    SDL_BLENDFACTOR_ZERO,
    SDL_BLENDOPERATION_ADD
    );

/* dstRGB = .5 dstRGB + .5 srcRGB ;  dstA = 1 (.5 + .5)  */
SDL_BlendMode mixTwoHalfBlend = SDL_ComposeCustomBlendMode(
    SDL_BLENDFACTOR_DST_ALPHA,
    SDL_BLENDFACTOR_DST_ALPHA,
    SDL_BLENDOPERATION_ADD,
    SDL_BLENDFACTOR_ZERO,
    SDL_BLENDFACTOR_ONE,
    SDL_BLENDOPERATION_ADD
    );

float renderToggleTimers[RENDER_COUNT] = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
};

const std::string_view renderToggleNames[]  = {
    "Background Low Priority",
    "Foreground Low Priority",
    "Sprites Low Priority",
    "Rings",
    "Background High Priority",
    "Foreground High Priority",
    "Sprites High Priority",
};

u32 renderFlags  = 0
    // RENDER_BACKGROUND_LOW
    | RENDER_FOREGROUND_LOW
    | RENDER_SPRITE_LOW
    | RENDER_RINGS
    // | RENDER_BACKGROUND_HIGH
    | RENDER_FOREGROUND_HIGH
    | RENDER_SPRITE_HIGH;

SDL_Texture** texturesToRender[RENDER_COUNT] = {
    &bg_texture_low,
    &level_texture_low,
    &sprite_texture_low,
    &rings_texture,
    &bg_texture_high,
    &level_texture_high,
    &sprite_texture_high,
};

bool redrawLevel = false;

float scrollX = 0.0f;
float scrollY = 0.0f;
float scale=1.0f;
float speed=0.0f;
bool pauseData = false;

float SpeedChangeMsgTimer = 0.0f;
float FileChangeMsgTimer = 0.0f;
bool skipToNextLevel = false;

int inputFD = -1;
FILE* inputStream = nullptr;
RenderingData gameData;
std::string frameError;

std::pair<int, int> INTERNAL_RESOLUTION;
std::pair<int, int> OUTPUT_RESOLUTION;
const char* FFMPEG_OUT = nullptr;
const char* FFMPEG_CODEC = "h264_nvenc";
bool dynamicResolution = false;

OOB x_loop = OOB::CLAMP;
OOB y_loop = OOB::CLAMP;