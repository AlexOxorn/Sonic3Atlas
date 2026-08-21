//
// Created by alexoxorn on 8/6/26.
//

#include "consts.h"
#include "SDL3/SDL.h"
#include "textureManager.h"

/*
====================
Palettes
====================
Source Textures used an 8 bit indexed color mode to represent
the pixel data.
Instead of having multiple palettes to represent each palette line,
All palettes are combined into 2 palettes,
one fore high priority tiles, and one fore low priority tiles.

The bit field per pixel is broken down as follows
PWCC'IIII
P: Priority bit
W: Water Palette bit
CC: Color Line (0-4)
IIII: Color Index (0-15)

By having a low priority palette which sets colors 0x80 - 0xFF as transparent
and a high priority palette which sets colors 0x00 - 0x7F as transparent,

We can switch wether we are rendering just the low prio pixels vs high prio pixels.

====================
Source Textures
====================
These textures act as the building block for all the graphics.
These all use SDL_TEXTUREACCESS_STREAMING access and Indexed color.
By calling SDL_LockTexture, we can stream the raw bytes into the texture,
while also adding appropriate offsets for Color line, water palette and
priority.

-------------
tiles_texture
-------------
A texture with dimensions 8 tiles by 2048 tiles.
Each row contains 8 copies of the same tile
with the first 4 columns using the 4 above ground palettes
and the last 4 columns using the 4 underwater palettes.

--------------
chunks_texture
--------------
A texture holding every chunk.
Each row is 16 chunks wide, with 8 chunks per row,
interleaving their above ground and underwater palettes

--------------------------------------
translucency_mask_texture (DEPRECATED)
--------------------------------------
My attempt to de-dither the textures

----------------
mappings_texture
----------------

Holds the mapping data used to render sprites.
Each row has 16 mapping entries, with a width of 32 pixels each (the max size of a mapping entry)
Alternating each entry's above and underwater palettes.

The first 8 entry pairs will always refer to the 8 entries used by rings.
The next entries are added on a first come first serve basis.

The struct SpriteMappingEntry has two static fields used to keep track of where each
entry is located.

mappingIDs map specific entries to the index into the mapping_texture.
mappingPixels maps those indexes to the pixel data that they would contain.

Mapping entries with identical data will only be added to the mapping texture once,
and will be reused, if one or more sprites use the same data.

===================================
Destination Textures (see draw.cpp)
===================================

Destination Textures are the intermediary textures
used to render the levels themselves.
They all use the SDL_PIXELFORMAT_RGBA32 format,
and all have SDL_TEXTUREACCESS_TARGET, so that they can be rendered to.

As an optimization, some of the layer textures have a size of
2 more than the number of chunks that can fit the width and height
of the internal resolution.

Then depending on the current scroll, only the chunks that fit are rendered

-------------
rings_texture
-------------
Used to hold the rendering of the static rings.
(These do not include rings dropped by the player, nor rings that are
attracted to the electric shield)

-----------------------------
level_textures
(level/bg)_texture_(low/high)
-----------------------------
Used to hold the level data in multiple layers.
High and Low priority layers are archived by changing the palette
of chunks_texture to the high/low priority palette.

Uses the water_line (and water flag)
To switch between using the above ground chunks or the underwater chunks

-------------------------
sprite_texture_(high/low)
-------------------------
Used to hold the sprite layers.

High and Low priority layers are archived by changing the palette
of mapping_texture to the high/low priority palette.

Uses the water_line (and water flag)
To switch between using the above ground mapping entries or the underwater mapping entries

------------------
sprite_tmp_texture
------------------
Used to temporarily store complete sprites, the combination of the entries that make it up.
Used to simplify the process of flipping the sprite as a whole as opposed to
manually placing flipped entries in their appropriate place.

---------------------
fullscreen_texture
---------------------
Used to hold the full combined image before being rendered to the main window.

---------------------
make_transparent_mask
---------------------
A fixed texture with half transparency.
As an anti-aliasing method for dealing with Sonic's dithering.

Using a custom blending mode, this can be applied to full_screen_texture
to force every non transparent pixel to half alpha.

Then by using a custom blending mode on the fullscreen image,
and by rendering the fullscreen_texture twice, with the second
shifted right by 1 pixel to blend neighbouring dithered pixels.
 */


static u8 to_grey_scale(Color8Bit color) {
    auto [red, green, blue] = color;
    return static_cast<u8>(0.299 * red + 0.587 * green + 0.114 * blue);
};

void cleanupDestTextures() {
    SDL_DestroyTexture(rings_texture);
    stdr::for_each(level_textures, SDL_DestroyTexture);
    SDL_DestroyTexture(sprite_texture_high);
    SDL_DestroyTexture(sprite_texture_low);
    SDL_DestroyTexture(sprite_tmp_texture);
    SDL_DestroyTexture(hud_texture);
}

void cleanupSrcTextures() {
    SDL_DestroyTexture(tiles_texture);
    SDL_DestroyTexture(chunks_texture);
    SDL_DestroyTexture(grey_chunks_texture);
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
    SDL_SetTextureScaleMode(rings_texture, SDL_SCALEMODE_PIXELART);

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
        SDL_SetTextureScaleMode(level_texture, SDL_SCALEMODE_PIXELART);
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
    SDL_SetTextureScaleMode(sprite_texture_high, SDL_SCALEMODE_PIXELART);
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
    SDL_SetTextureScaleMode(sprite_texture_low, SDL_SCALEMODE_PIXELART);
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
    SDL_SetTextureScaleMode(sprite_tmp_texture, SDL_SCALEMODE_PIXELART);

    make_transparent_mask = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        SCREEN_TEXTURE_WIDTH,
        SCREEN_TEXTURE_HEIGHT
        );
    if (!make_transparent_mask) {
        SDL_Log("Couldn't create make_transparent_mask texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(make_transparent_mask, SDL_SCALEMODE_PIXELART);
    SDL_SetTextureBlendMode(make_transparent_mask, makeTransparentBlend);

    SDL_SetRenderTarget(renderer, make_transparent_mask);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 127);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, nullptr);

    fullscreen_texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        RENDER_WIDTH,
        RENDER_HEIGHT
        );
    if (!fullscreen_texture) {
        SDL_Log("Couldn't create fullscreen_texture texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(fullscreen_texture, SDL_SCALEMODE_PIXELART);


    hud_texture = SDL_CreateTexture(renderer,
    SDL_PIXELFORMAT_RGBA32,
    SDL_TEXTUREACCESS_TARGET,
    WIDESCREEN_GEN.first,
    WIDESCREEN_GEN.second
    );

    if (!hud_texture) {
        SDL_Log("Couldn't create hud_texture texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(hud_texture, SDL_SCALEMODE_PIXELART);
    SDL_SetTextureBlendMode(hud_texture, SDL_BLENDMODE_BLEND);


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

    grey_chunks_texture = SDL_CreateTexture(renderer,
    pixelFormat,
    SDL_TEXTUREACCESS_STREAMING,
    Chunk::WIDTH*16,
    Chunk::WIDTH * ChunkMap::COUNT/8);
    if (!grey_chunks_texture) {
        SDL_Log("Couldn't create grey_chunks_texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(grey_chunks_texture, SCALE_MODE);
    SDL_SetTextureBlendMode(grey_chunks_texture, SDL_BLENDMODE_BLEND);

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

bool updatePalette() {
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
    if (low_grey_palette == nullptr) {
        low_grey_palette = SDL_CreatePalette(1 << (sizeof(indexedColor) * 8));
        if (!low_grey_palette) {
            SDL_Log("Couldn't create palette: %s", SDL_GetError());
            return false;
        }
    }
    if (high_grey_palette == nullptr) {
        high_grey_palette = SDL_CreatePalette(1 << (sizeof(indexedColor) * 8));
        if (!high_grey_palette) {
            SDL_Log("Couldn't create palette: %s", SDL_GetError());
            return false;
        }
    }
    if (full_palette == nullptr) {
        full_palette = SDL_CreatePalette(1 << (sizeof(indexedColor) * 8));
        if (!full_palette) {
            SDL_Log("Couldn't create palette: %s", SDL_GetError());
            return false;
        }
    }

    std::array<SDL_Color, PALETTE_SIZE*PALETTE_COUNT*2> low{};
    std::array<SDL_Color, PALETTE_SIZE*PALETTE_COUNT*2> high{};
    std::array<SDL_Color, PALETTE_SIZE*PALETTE_COUNT*2> low_grey{};
    std::array<SDL_Color, PALETTE_SIZE*PALETTE_COUNT*2> high_grey{};

    auto to_SDL_color = [](const Color8Bit color) {
        return SDL_Color{.r = color.red, .g = color.green, .b = color.blue, .a = 255};
    };
    auto to_grey = [] (const Color8Bit color) { return to_grey_scale(color); };
    auto to_SDL_grey = [](const u8 color) {
        return SDL_Color{.r = color, .g = color, .b = color, .a = 128};
    };
    const auto low_out = low.begin();
    const auto high_out = high.begin();
    const auto lowT_out = low_grey.begin();
    const auto highT_out = high_grey.begin();

    // LOW PRIO
    for (auto [i, line] : stdr::views::enumerate(gameData.palette.lines)) {
        stdr::transform(line.colors, low_out + (PALETTE_SIZE * i), to_SDL_color);
        stdr::copy(line.colors | stdv::transform(to_grey) | stdv::transform(to_SDL_grey), lowT_out + (PALETTE_SIZE * i));
        stdr::copy(line.colors | stdv::transform(to_grey) | stdv::transform(to_SDL_grey), lowT_out + (PALETTE_SIZE * (i+4)));
    }
    for (auto [i, line] : stdr::views::enumerate(gameData.water_palette.lines)) {
        stdr::transform(line.colors, low_out + (PALETTE_SIZE * (i+4)), to_SDL_color);
    }
    // HIGH PRIO
    for (auto [i, line] : stdr::views::enumerate(gameData.palette.lines)) {
        stdr::transform(line.colors, high_out + (PALETTE_SIZE * (i+8)), to_SDL_color);
        stdr::copy(line.colors | stdv::transform(to_grey_scale) | stdv::transform(to_SDL_grey), highT_out + (PALETTE_SIZE * (i+8)));
        stdr::copy(line.colors | stdv::transform(to_grey_scale) | stdv::transform(to_SDL_grey), highT_out + (PALETTE_SIZE * (i+12)));
    }
    for (auto [i, line] : stdr::views::enumerate(gameData.water_palette.lines)) {
        stdr::transform(line.colors, high_out + (PALETTE_SIZE * (i+12)), to_SDL_color);
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

    if (!SDL_SetPaletteColors(full_palette, low.data(), 0, PALETTE_SIZE * PALETTE_COUNT)) {
        SDL_Log("Couldn't set palette full palette low: %s", SDL_GetError());
        return false;
    }

    if (!SDL_SetPaletteColors(full_palette, low.data(), 0x80, PALETTE_SIZE * PALETTE_COUNT)) {
        SDL_Log("Couldn't set palette full palette high: %s", SDL_GetError());
        return false;
    }


    if (!SDL_SetPaletteColors(low_grey_palette, low_grey.data(), 0, PALETTE_SIZE * PALETTE_COUNT * 2)) {
        SDL_Log("Couldn't set palette low transparent: %s", SDL_GetError());
        return false;
    };

    if (!SDL_SetPaletteColors(high_grey_palette, high_grey.data(), 0, PALETTE_SIZE * PALETTE_COUNT * 2)) {
        SDL_Log("Couldn't set palette low transparent: %s", SDL_GetError());
        return false;
    };

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
    if (grey_chunks_texture) {
        if (!SDL_SetTexturePalette(grey_chunks_texture, high_grey_palette)) {
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

bool fullTileUpdate() {
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

bool fullChunkUpdate() {
    indexedColor* SDLpixels;
    int pitch;

    std::vector<bool> maskData(Chunk::WIDTH*16 *  Chunk::WIDTH * ChunkMap::COUNT/8);

    const auto copyChunks = [&]() {
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
                    for (const unsigned char k : subrow) {
                        *SDLpixels++ = k + (rowCount % 2 ? 0x40 : 0);
                    }
                }
            }
        }
    };

    if (!SDL_LockTexture(chunks_texture, nullptr, reinterpret_cast<void**>(&SDLpixels), &pitch)) {
        SDL_Log("Couldn't lock tile texture: %s", SDL_GetError());
        return false;
    };
    copyChunks();
    SDL_UnlockTexture(chunks_texture);

    if (!SDL_LockTexture(grey_chunks_texture, nullptr, reinterpret_cast<void**>(&SDLpixels), &pitch)) {
        SDL_Log("Couldn't lock tile texture: %s", SDL_GetError());
        return false;
    };
    copyChunks();
    SDL_UnlockTexture(grey_chunks_texture);

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

bool updateMappings() {
    // Building Map Bytes
    for (auto& s : gameData.sprites) {
        for (auto &entry : s.frame.entries) {
            static_cast<void>(entry.getBytes(s.object.art_tile, gameData.tileset));
        }
        for (auto& childEntry : s.children | stdv::transform(&SpriteMappingFrame::entries) | stdv::join) {
            static_cast<void>(childEntry.getBytes(s.object.art_tile, gameData.tileset));
        }
    }
    for (const auto& [size, entries] : hud_mappings) {
        for (auto& entry : entries) {
            static_cast<void>(entry.getBytes(
                HUD_ART_TILE_TMP,
                gameData.tileset));
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
    for (int e = 0; e < std::min(number_of_entries, MAX_MAPPING_ENTRIES); ++e) {
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

bool partialTileUpdate() {
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

    auto copy_textures = [](indexedColor* SDLpixels, int pitch) {
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
    };

    if (!SDL_LockTexture(chunks_texture, nullptr, reinterpret_cast<void**>(&SDLpixels), &pitch)) {
        SDL_Log("Couldn't lock tile texture: %s", SDL_GetError());
        return false;
    };
    copy_textures(SDLpixels, pitch);
    SDL_UnlockTexture(chunks_texture);
    if (!SDL_LockTexture(grey_chunks_texture, nullptr, reinterpret_cast<void**>(&SDLpixels), &pitch)) {
        SDL_Log("Couldn't lock tile texture: %s", SDL_GetError());
        return false;
    };
    copy_textures(SDLpixels, pitch);
    SDL_UnlockTexture(grey_chunks_texture);
    return true;
}

