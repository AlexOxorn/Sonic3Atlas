//
// Created by alexoxorn on 8/6/26.
//

#include "consts.h"
#include "SDL3/SDL.h"

void cleanupDestTextures() {
    SDL_DestroyTexture(rings_texture);
    stdr::for_each(level_textures, SDL_DestroyTexture);
    SDL_DestroyTexture(sprite_texture_high);
    SDL_DestroyTexture(sprite_texture_low);
    SDL_DestroyTexture(sprite_tmp_texture);
}

void cleanupSrcTextures() {
    SDL_DestroyTexture(tiles_texture);
    SDL_DestroyTexture(chunks_texture);
    SDL_DestroyTexture(translucency_mask_texture);
    SDL_DestroyTexture(mappings_texture);

}

bool initDestTextures() {
    cleanupDestTextures();
    rings_texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        SCREEN_TEXTURE_WIDTH,
        SCREEN_TEXTURE_HEIGHT
        );
    if (!rings_texture) {
        SDL_Log("Couldn't create rings_texture texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(rings_texture, SDL_SCALEMODE_LINEAR);

    for (auto& level_texture : level_textures) {
        level_texture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_TARGET,
            SCREEN_TEXTURE_WIDTH,
            SCREEN_TEXTURE_HEIGHT
            );
        if (!level_texture) {
            SDL_Log("Couldn't create level_texture texture: %s", SDL_GetError());
            return false;
        }
        SDL_SetTextureScaleMode(level_texture, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(level_texture, SDL_BLENDMODE_BLEND);
    }

    // for (auto& sprite_target : std::array{sprite_texture_high, sprite_texture_low}) {
    sprite_texture_high = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        SCREEN_TEXTURE_WIDTH,
        SCREEN_TEXTURE_HEIGHT
    );
    if (!sprite_texture_high) {
        SDL_Log("Couldn't create sprite_texture texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(sprite_texture_high, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(sprite_texture_high, SDL_BLENDMODE_BLEND);

    sprite_texture_low = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        SCREEN_TEXTURE_WIDTH,
        SCREEN_TEXTURE_HEIGHT
    );
    if (!sprite_texture_low) {
        SDL_Log("Couldn't create sprite_texture texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(sprite_texture_low, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(sprite_texture_low, SDL_BLENDMODE_BLEND);

    sprite_tmp_texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        SPRITE_WIDTH * SPRITE_PER_TMP_TEXTURE_ROW,
        SPRITE_WIDTH * SPRITE_PER_TMP_TEXTURE_ROW
        );
    if (!sprite_tmp_texture) {
        SDL_Log("Couldn't create sprite_tmp_texture texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(sprite_tmp_texture, SDL_SCALEMODE_LINEAR);

    return true;

}

bool initSourceTextures() {
    cleanupSrcTextures();
    tiles_texture = SDL_CreateTexture(renderer,
            pixelFormat,
            SDL_TEXTUREACCESS_STREAMING,
            Tile::WIDTH*PALETTE_COUNT,
            Tile::WIDTH * TileSet::COUNT);
    if (!tiles_texture) {
        SDL_Log("Couldn't create tile texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(tiles_texture, SCALE_MODE);

    chunks_texture = SDL_CreateTexture(renderer,
    pixelFormat,
    SDL_TEXTUREACCESS_STREAMING,
    Chunk::WIDTH*16,
    Chunk::WIDTH * ChunkMap::COUNT/8);
    if (!chunks_texture) {
        SDL_Log("Couldn't create chunk texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(chunks_texture, SCALE_MODE);
    SDL_SetTextureBlendMode(chunks_texture, SDL_BLENDMODE_BLEND);

    translucency_mask_texture = SDL_CreateTexture(renderer,
    SDL_PIXELFORMAT_INDEX8,
    SDL_TEXTUREACCESS_STREAMING,
    Chunk::WIDTH*16,
    Chunk::WIDTH * ChunkMap::COUNT/8);
    if (!translucency_mask_texture) {
        SDL_Log("Couldn't create translucent texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(translucency_mask_texture, SDL_SCALEMODE_PIXELART);
    SDL_SetTextureBlendMode(translucency_mask_texture, transparencyBlend);
    SDL_SetTexturePalette(translucency_mask_texture, transparency_mask_palette);

    mappings_texture = SDL_CreateTexture(renderer,
    pixelFormat,
    SDL_TEXTUREACCESS_STREAMING,
    pixelsPerRow,
    MAPPING_ENTRY_ROWS * SpriteMappingEntry::WIDTH);
    if (!mappings_texture) {
        SDL_Log("Couldn't create tile texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(mappings_texture, SCALE_MODE);
    SDL_SetTextureBlendMode(mappings_texture, SDL_BLENDMODE_BLEND);

    return true;
}