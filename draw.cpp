//
// Created by alexoxorn on 8/4/26.
//

#include "draw.h"

#include <cmath>
#include <format>

#include "structs.h"
#include "consts.h"
#include <SDL3/SDL.h>

static constexpr bool drawchunkBorder = false;

struct FramePosition {
    int x_pos;
    int y_pos;
    SDL_FlipMode flip;
};


static void renderEntry(int index, SDL_FRect src, SDL_FRect dst) {
    dst.x += static_cast<float>(SPRITE_WIDTH * (index % SPRITE_PER_TMP_TEXTURE_ROW));
    dst.y += static_cast<float>(SPRITE_WIDTH * (index / SPRITE_PER_TMP_TEXTURE_ROW));
    SDL_RenderTexture(renderer, mappings_texture, &src, &dst);
}

static bool drawFrame(
    int index,
    const SpriteMappingFrame& frame,
    const BatCell art_tile,
    const int x_pos,
    const int y_pos,
    const int relativeWaterLine,
    const SDL_FlipMode flip,
    std::vector<FramePosition>& positions
    )
{
    auto [size, entries] = frame;

    for (auto& entry : entries | stdv::reverse) {
        const s64 mapID = SpriteMappingEntry::mappingIDs[entry.withBase(art_tile)];

        SDL_FRect src{
            .x = static_cast<float>(2 * (mapID % MAPPING_ENTRY_PER_ROW * SpriteMappingEntry::WIDTH)),
            .y = static_cast<float>(mapID / MAPPING_ENTRY_PER_ROW * SpriteMappingEntry::WIDTH),
            .w = static_cast<float>((entry.dim.width+1) * Tile::WIDTH),
            .h = static_cast<float>((entry.dim.height+1) * Tile::WIDTH)
        };

        SDL_FRect dest{
            .x = static_cast<float>(entry.x_pos + 128),
            .y = static_cast<float>(entry.y_pos + 128),
            .w = static_cast<float>((entry.dim.width+1) * Tile::WIDTH),
            .h = static_cast<float>((entry.dim.height+1) * Tile::WIDTH)
        };

        // Above Water
        if (SPRITE_WIDTH <= (relativeWaterLine)) {
            renderEntry(index, src, dest);
        }
        // Under Water
        if (gameData.has_water && relativeWaterLine <= 0) {
            src.x += SpriteMappingEntry::WIDTH;
            renderEntry(index, src, dest);
        }
        if (gameData.has_water && 0 < relativeWaterLine && relativeWaterLine < SPRITE_WIDTH) {
            if (flip & SDL_FLIP_VERTICAL) {
                const auto init_h = dest.h;
                const int modifiedRelativeWater = SPRITE_WIDTH - relativeWaterLine;
                if (dest.y > static_cast<float>(modifiedRelativeWater)) {
                    renderEntry(index, src, dest);
                } else {
                    src.x += SpriteMappingEntry::WIDTH;

                    dest.h = std::min(static_cast<float>(modifiedRelativeWater) - dest.y, dest.h);
                    src.h = dest.h;
                        renderEntry(index, src, dest);

                    if (dest.h != init_h) {
                        src.x -= SpriteMappingEntry::WIDTH;
                        src.y += dest.h;
                        dest.y += dest.h;
                        dest.h = init_h - dest.h;
                        src.h = dest.h;

                        renderEntry(index, src, dest);
                    }
                }
            } else {
                const auto init_h = dest.h;
                if (dest.y > static_cast<float>(relativeWaterLine)) {
                    src.x += SpriteMappingEntry::WIDTH;
                    renderEntry(index, src, dest);
                } else {
                    dest.h = std::min(static_cast<float>(relativeWaterLine) - dest.y, dest.h);
                    src.h = dest.h;
                    renderEntry(index, src, dest);

                    if (dest.h != init_h) {
                        src.x += SpriteMappingEntry::WIDTH;
                        src.y += dest.h;
                        dest.y += dest.h;
                        dest.h = init_h - dest.h;
                        src.h = dest.h;
                        renderEntry(index, src, dest);
                    }
                }
            }
        }

    }

    positions.emplace_back(x_pos, y_pos, flip);
    return true;
}

bool drawSprites(const bool prio) {
    if ((renderFlags & (prio ? RENDER_SPRITE_HIGH : RENDER_SPRITE_LOW)) == 0)
        return true;
    SDL_Texture*& target = prio ? sprite_texture_high : sprite_texture_low;

    const auto palette = prio ? high_prio_palette : low_prio_palette;
    constexpr int max_components = 9;
    SDL_SetTexturePalette(mappings_texture, palette);


    const int leftmostChunk = static_cast<int>(scrollX / Chunk::WIDTH);
    const int topmostChunk = static_cast<int>(scrollY / Chunk::WIDTH);
    const int xOffset = leftmostChunk * Chunk::WIDTH;
    const int yOffset = topmostChunk * Chunk::WIDTH;

    auto getTruePos = [=](const int in_x, const int in_y, const bool lvl_coord) -> std::pair<int, int> {
        int x_pos = in_x - xOffset - 128;
        int y_pos = in_y - yOffset - 128;

        if (!lvl_coord) {
            x_pos += gameData.screen_position_A.first - 128;
            y_pos += gameData.screen_position_A.second - 128;
        }

        if (x_pos + 256 < 0)
            return {-1, -1};
        if (x_pos > RENDER_WIDTH) {
            return {-1, -1};
        }

        return {x_pos, y_pos};
    };

    const auto water_line_coord = (gameData.water_line - (topmostChunk*Chunk::WIDTH));

    std::vector<FramePosition> positions;

    if (!SDL_SetRenderTarget(renderer, sprite_tmp_texture)) {
        SDL_Log("Couldn't set render target texture: %s", SDL_GetError());
        return false;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); // Transparent black
    SDL_RenderClear(renderer);

    int index = 0;

    for (const auto& [object, frame, children] : gameData.sprites | stdv::reverse) {
        auto [x_pos, y_pos] = getTruePos(
            object.x_pos, object.y_pos, object.render_flags.use_level_coordinates);

        if (x_pos == -1 && y_pos == -1)
            continue;

        auto [size, entries] = frame;

        const auto flip =
            static_cast<SDL_FlipMode>(
                SDL_FLIP_VERTICAL * object.render_flags.vertical_mirror +
                SDL_FLIP_HORIZONTAL * object.render_flags.horizontal_mirror);

        if (!SDL_SetRenderTarget(renderer, sprite_tmp_texture)) {
            SDL_Log("Couldn't set render target texture: %s", SDL_GetError());
            return false;
        }

        if (!object.render_flags.compound_sprite || object.mapping_frame != 0 ) {
            drawFrame(index++, frame, object.art_tile, x_pos, y_pos, water_line_coord - y_pos, flip, positions);
        }

        for (const auto& [childFrame, childData] :
            stdv::zip(children, object.children))
        {
            auto [sub_x_pos, sub_y_pos] = getTruePos(childData.x_pos, childData.y_pos, object.render_flags.use_level_coordinates);
            drawFrame(index++, childFrame, object.art_tile, sub_x_pos, sub_y_pos, water_line_coord - sub_y_pos, flip, positions);
        }
    }

    std::vector<int> loopOffsets;
    const float vertLoop = gameData.vertical_loop;
    if (gameData.screen_min_y < 0) {
        loopOffsets.resize(2*LOOP_COUNT+1);
        stdr::iota(loopOffsets.begin(), loopOffsets.end(), -LOOP_COUNT);
    } else {
        loopOffsets = {0};
    }

    if (!SDL_SetRenderTarget(renderer, target)) {
        SDL_Log("Couldn't set render target texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); // Transparent black
    SDL_RenderClear(renderer);

    for (int i = 0; i < index; ++i) {
        auto [x, base_y, flip] = positions[i];
        for (const int loopOff : loopOffsets) {
            const int y = base_y + loopOff * gameData.vertical_loop;;
            if (y + 256 < 0)
                continue;
            if (y > RENDER_HEIGHT)
                continue;

            SDL_FRect src {
                .x = static_cast<float>(SPRITE_WIDTH * (i%SPRITE_PER_TMP_TEXTURE_ROW)),
                .y = static_cast<float>(SPRITE_WIDTH * (i/SPRITE_PER_TMP_TEXTURE_ROW)),
                .w = SPRITE_WIDTH,
                .h = SPRITE_WIDTH,
            };

            SDL_FRect dest {
                .x = static_cast<float>(x),
                .y = static_cast<float>(y),
                .w = SPRITE_WIDTH,
                .h = SPRITE_WIDTH,
            };

            SDL_RenderTextureRotated(renderer, sprite_tmp_texture, &src, &dest, 0, nullptr, flip);
        }
    }


    SDL_SetRenderTarget(renderer, nullptr);
    return true;
}


bool drawRings() {
    if ((renderFlags & RENDER_RINGS) == 0)
        return true;

    SDL_SetTexturePalette(mappings_texture, low_prio_palette);

    if (!SDL_SetRenderTarget(renderer, rings_texture)) {
        SDL_Log("Couldn't set render target texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); // Transparent black
    SDL_RenderClear(renderer);

    std::vector<int> loopOffsets;
    const float vertLoop = gameData.vertical_loop;
    if (gameData.screen_min_y < 0) {
        loopOffsets.resize(LOOP_COUNT+2);
        stdr::iota(loopOffsets.begin(), loopOffsets.end(), -1);
    } else {
        loopOffsets = {0};
    }

    const s16 leftmostChunk = static_cast<s16>(scrollX / Chunk::WIDTH);
    const s16 topmostChunk = static_cast<s16>(scrollY / Chunk::WIDTH);
    const s16 xOffset = leftmostChunk * Chunk::WIDTH;
    const s16 yOffset = topmostChunk * Chunk::WIDTH;

    auto locations = gameData.ring_data.locationSubrange();

    const auto first_ring = stdr::lower_bound(
        locations,
        xOffset, std::less{}, &ringLocation::x_pos);
    const auto last_ring = stdr::upper_bound(first_ring, locations.end(),
        xOffset+SCREEN_TEXTURE_WIDTH, std::less{}, &ringLocation::x_pos);

    const auto start_index = first_ring - locations.begin();
    const auto last_index = last_ring - locations.begin();

    for (long ringIndex = start_index; ringIndex < last_index; ringIndex++) {
        auto [timer, frame] = gameData.ring_data.status[ringIndex];
        auto [x_pos, y_pos] = locations[ringIndex];
        if (timer == -1)
            continue;

        int x_shift = x_pos - xOffset;
        int y_shift_pre = y_pos - yOffset;

        if (x_shift + 16 < 0)
            continue;
        if (x_shift - 16 > RENDER_WIDTH) {
            continue;
        }

        for (int loopOff : loopOffsets) {
            int y_shift = y_shift_pre + loopOff * (gameData.vertical_loop);

            if (y_shift + 256 < 0) {
                continue;
            }
            if (y_shift - 256 > RENDER_HEIGHT) {
                continue;
            }

            const auto mapID = frame ? frame : gameData.ring_data.ringAnimFrame;
            const auto entry = gameData.ring_data.ring_mappings[mapID];

            SDL_FRect src{
                .x = static_cast<float>(2 * mapID * SpriteMappingEntry::WIDTH ),
                .y = static_cast<float>(0),
                .w = static_cast<float>((entry.dim.width+1) * Tile::WIDTH),
                .h = static_cast<float>((entry.dim.height+1) * Tile::WIDTH)
            };

            SDL_FRect dest{
                .x = static_cast<float>(entry.x_pos + x_shift),
                .y = static_cast<float>(entry.y_pos + y_shift),
                .w = static_cast<float>((entry.dim.width+1) * Tile::WIDTH),
                .h = static_cast<float>((entry.dim.height+1) * Tile::WIDTH)
            };

            SDL_RenderTexture(renderer, mappings_texture, &src, &dest);
        }


    }
    SDL_SetRenderTarget(renderer, nullptr);
    return true;
}

static bool drawPlane(const std::vector<std::vector<u8>>& chunks, const int chunkXOffset = 0, const int chunkYOffset = 0) {
    SDL_FRect src{
        .x = 0,
        .y = 0,
        .w = Chunk::WIDTH,
        .h = Chunk::WIDTH
    };
    SDL_FRect dest{
        .x = 0,
        .y = 0,
        .w = Chunk::WIDTH,
        .h = Chunk::WIDTH
    };

    const int leftmostChunk = static_cast<int>(scrollX / Chunk::WIDTH);
    const int topmostChunk = static_cast<int>(scrollY / Chunk::WIDTH);
    const int rightmostChunk = std::ceil((scrollX + RENDER_WIDTH)/Chunk::WIDTH);
    const int bottommostChunk = std::ceil((scrollY + RENDER_HEIGHT)/Chunk::WIDTH);

    const auto water_line_coord = static_cast<float>(gameData.water_line - (topmostChunk*Chunk::WIDTH));

    auto getChunkIndex = [&chunks](int rowIndex, int columnIndex) -> int {
        if (gameData.screen_min_y < 0) {
            while (rowIndex < 0)
                rowIndex += static_cast<int>(chunks.size());
            while (rowIndex >= chunks.size())
                rowIndex -= static_cast<int>(chunks.size());
        }
        try {
            return chunks.at(rowIndex).at(columnIndex);
        } catch (const std::out_of_range& e) {
            return -1;
        }
    };

    for (int rowIndex_base = topmostChunk; rowIndex_base <= bottommostChunk; rowIndex_base++) {
        const int rowIndex = rowIndex_base + chunkYOffset;
        dest.y = static_cast<float>((rowIndex_base - topmostChunk) * Chunk::WIDTH);

        const bool land = dest.y < water_line_coord || !gameData.has_water;
        const bool water = dest.y + dest.h >= water_line_coord && gameData.has_water;

        if (water && land) {
            dest.h = water_line_coord - dest.y;
            for (int columnIndex_base = leftmostChunk; columnIndex_base <= rightmostChunk; columnIndex_base++) {
                const int columnIndex = columnIndex_base + chunkXOffset;
                const int chunkIndex = getChunkIndex(rowIndex, columnIndex);
                if (chunkIndex < 0)
                    continue;
                dest.x = static_cast<float>((columnIndex - leftmostChunk) * Chunk::WIDTH);

                src.x = static_cast<float>(chunkIndex%8 * Chunk::WIDTH * 2);
                src.y = static_cast<float>(chunkIndex/8 * Chunk::WIDTH);
                src.h = dest.h;

                SDL_RenderTexture(renderer, chunks_texture, &src, &dest);
                SDL_RenderTexture(renderer, translucency_mask_texture, &src, &dest);
            }
            dest.h = (dest.y + Chunk::WIDTH - water_line_coord);
            dest.y = water_line_coord;
            for (int columnIndex_base = leftmostChunk; columnIndex_base <= rightmostChunk; columnIndex_base++) {
                const int columnIndex = columnIndex_base + chunkXOffset;
                const int chunkIndex = getChunkIndex(rowIndex, columnIndex);
                if (chunkIndex < 0)
                    continue;
                dest.x = static_cast<float>((columnIndex_base - leftmostChunk) * Chunk::WIDTH);

                src.x = static_cast<float>(chunkIndex%8 * Chunk::WIDTH * 2 + Chunk::WIDTH);
                src.y = static_cast<float>(chunkIndex/8 * Chunk::WIDTH + (Chunk::WIDTH - dest.h));
                src.h = dest.h;

                SDL_RenderTexture(renderer, chunks_texture, &src, &dest);
                SDL_RenderTexture(renderer, translucency_mask_texture, &src, &dest);
            }
            dest.h = Chunk::WIDTH;
            dest.y = 0;
            src.h = Chunk::WIDTH;
        }
        else if (land) {
            for (int columnIndex_base = leftmostChunk; columnIndex_base <= rightmostChunk; columnIndex_base++) {
                const int columnIndex = columnIndex_base + chunkXOffset;
                const int chunkIndex = getChunkIndex(rowIndex, columnIndex);
                if (chunkIndex < 0)
                    continue;
                dest.x = static_cast<float>((columnIndex_base - leftmostChunk) * Chunk::WIDTH);

                src.x = static_cast<float>(chunkIndex%8 * Chunk::WIDTH * 2);
                src.y = static_cast<float>(chunkIndex/8 * Chunk::WIDTH);

                SDL_RenderTexture(renderer, chunks_texture, &src, &dest);
                SDL_RenderTexture(renderer, translucency_mask_texture, &src, &dest);
            }
        }
        else if (water) {
            for (int columnIndex_base = leftmostChunk; columnIndex_base <= rightmostChunk; columnIndex_base++) {
                const int columnIndex = columnIndex_base + chunkXOffset;
                int chunkIndex;
                try {
                    chunkIndex = chunks.at(rowIndex).at(columnIndex);
                } catch (const std::out_of_range& e) {
                    continue;;
                }
                dest.x = static_cast<float>((columnIndex_base - leftmostChunk) * Chunk::WIDTH);

                src.x = static_cast<float>(chunkIndex%8 * Chunk::WIDTH * 2+ Chunk::WIDTH);
                src.y = static_cast<float>(chunkIndex/8 * Chunk::WIDTH);

                SDL_RenderTexture(renderer, chunks_texture, &src, &dest);
                SDL_RenderTexture(renderer, translucency_mask_texture, &src, &dest);
            }
        }


    }

    if constexpr (drawchunkBorder) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        for (int i = 1; i < RENDER_WIDTH/Chunk::WIDTH; i++) {
            SDL_RenderLine(renderer, i * Chunk::WIDTH, 0, i * Chunk::WIDTH, RENDER_HEIGHT);
            SDL_RenderLine(renderer, i * Chunk::WIDTH+1, 0, i * Chunk::WIDTH+1, RENDER_HEIGHT);
            SDL_RenderLine(renderer, i * Chunk::WIDTH+2, 0, i * Chunk::WIDTH+2, RENDER_HEIGHT);
        }
        for (int i = 1; i < RENDER_HEIGHT/Chunk::WIDTH; i++) {
            SDL_RenderLine(renderer, 0, i * Chunk::WIDTH, RENDER_WIDTH, i * Chunk::WIDTH);
            SDL_RenderLine(renderer, 0, i * Chunk::WIDTH+1, RENDER_WIDTH, i * Chunk::WIDTH+1);
            SDL_RenderLine(renderer, 0, i * Chunk::WIDTH+2, RENDER_WIDTH, i * Chunk::WIDTH+2);
        }

        for (int rowIndex = topmostChunk; rowIndex <= bottommostChunk; rowIndex++) {
            for (int columnIndex = leftmostChunk; columnIndex <= rightmostChunk; columnIndex++) {
                const int chunkIndex = getChunkIndex(rowIndex, columnIndex);
                auto chunkPOS = std::format("{},{}", columnIndex, rowIndex);
                auto chunkIndexS = std::format("{}", chunkIndex);
                const float X = Chunk::WIDTH * (columnIndex - leftmostChunk);
                const float Y = Chunk::WIDTH * (rowIndex - topmostChunk);
                SDL_FRect dest{
                    .x = X,
                    .y = Y,
                    .w = 48,
                    .h = 24
                };
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderFillRect(renderer, &dest);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDebugText(renderer,
                    X+3,
                    Y+3,
                    chunkPOS.c_str());
                SDL_RenderDebugText(renderer,
                    X+3,
                    Y+14,
                    chunkIndexS.c_str());
            }
        }
    }

    return true;
}


static bool drawToBackgroundDefault(const bool prio) {
    if ((renderFlags & (prio ? RENDER_BACKGROUND_HIGH : RENDER_BACKGROUND_LOW)) == 0)
        return true;

    SDL_Texture*& level_texture = level_textures[LEVEL_BG | (prio ? LEVEL_HIGH : 0)];

    const auto palette = prio ? high_prio_palette : low_prio_palette;
    SDL_SetTexturePalette(chunks_texture, palette);
    if (!SDL_SetRenderTarget(renderer, level_texture)) {
        SDL_Log("Couldn't set render target texture: %s", SDL_GetError());
        return false;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);



    auto [BGScrollX, BGScrollY] = gameData.screen_position_B;
    auto [FGScrollX, FGScrollY] = gameData.screen_position_A;

    const int FG_leftmostChunk = FGScrollX / Chunk::WIDTH;
    const int BG_leftmostChunk = BGScrollX / Chunk::WIDTH;
    const int FG_topmostChunk = FGScrollY / Chunk::WIDTH;
    const int BG_topmostChunk = BGScrollY / Chunk::WIDTH;

    const int fgBgChunkXOffset = (BG_leftmostChunk - FG_leftmostChunk);
    const int fgBgChunkYOffset = (BG_topmostChunk - FG_topmostChunk);


    return drawPlane(gameData.background_chunks, fgBgChunkXOffset, fgBgChunkYOffset);
}


static bool drawToLevelDefault(const bool prio) {
    auto res = drawPlane(gameData.level_chunks);
    return res;
}

namespace AIZ_SHIP {
    static constexpr s32 propellerMapAddr = 0x23C182;
    static constexpr s32 SHIP_CHUNK_X = 123;
    static constexpr s32 SHIP_CHUNK_Y = 19;
    static constexpr s32 SHIP_CHUNK_WIDTH = 6;
    static constexpr s32 SHIP_CHUNK_HEIGHT = 2;
    static constexpr s32 SHIP_PIXEL_WIDTH = SHIP_CHUNK_WIDTH * Chunk::WIDTH;
    static constexpr s32 SHIP_PIXEL_HEIGHT = SHIP_CHUNK_HEIGHT * Chunk::WIDTH;

    static constexpr s32 PROPELLER_OFFSET_X = - SHIP_PIXEL_WIDTH + 3 * Chunk::WIDTH / 2 + Tile::WIDTH/2;
    static constexpr s32 PROPELLER_OFFSET_Y = - SHIP_PIXEL_HEIGHT + 2 * Tile::WIDTH;

    static std::optional<ObjectTableEntry> findFrontPropellerSprite() {
        auto propellers = gameData.sprites
            | stdv::transform(&Sprite::object)
            | stdv::filter([](const ObjectTableEntry& obj) { return obj.mappings == propellerMapAddr; });
        if (stdr::distance(propellers) == 0) {
            return std::nullopt;
        }
        return stdr::max(propellers, std::less{}, &ObjectTableEntry::x_pos);
    }

    static bool drawShip() {
        auto propellerOpt = findFrontPropellerSprite();
        if (!propellerOpt) {
            return true;
        }

        auto propeller = *propellerOpt;

        SDL_FRect src{
            .x = 0,
            .y = 0,
            .w = Chunk::WIDTH,
            .h = Chunk::WIDTH
        };
        SDL_FRect dest{
            .x = 0,
            .y = 0,
            .w = Chunk::WIDTH,
            .h = Chunk::WIDTH
        };

        const int leftmostChunk = static_cast<int>(scrollX / Chunk::WIDTH);
        const int topmostChunk = static_cast<int>(scrollY / Chunk::WIDTH);

        for (int j = 0; j < SHIP_CHUNK_HEIGHT; j++) {
            auto ship_y = static_cast<float>(propeller.y_pos + PROPELLER_OFFSET_Y);
            ship_y += static_cast<float>(j) * Chunk::WIDTH;
            dest.y = ship_y - topmostChunk * Chunk::WIDTH;;

            for (int i = 0; i < SHIP_CHUNK_WIDTH; ++i) {
                auto ship_x = static_cast<float>(propeller.x_pos + PROPELLER_OFFSET_X);
                ship_x += static_cast<float>(i) * Chunk::WIDTH;
                dest.x = ship_x - leftmostChunk * Chunk::WIDTH;

                auto chunkIndex = gameData.level_chunks[j + SHIP_CHUNK_Y][i + SHIP_CHUNK_X];

                src.x = static_cast<float>(chunkIndex%8 * Chunk::WIDTH * 2);
                src.y = static_cast<float>(chunkIndex/8 * Chunk::WIDTH);

                SDL_RenderTexture(renderer, chunks_texture, &src, &dest);
            }
        }
        return true;
    }
}

static bool drawSelect(bool prio) {
    switch (gameData.getCurrentActFGEvent()) {
        case LEVEL_ACT_EVENT(ANGLE_ISLAND_ZONE, 2, AIZ_FLYING_BOMB_SHIP): {
            const bool x = drawToLevelDefault(prio);
            const bool y = prio ? AIZ_SHIP::drawShip() : true;
            return x && y;
        }
        default: {
            return drawToLevelDefault(prio);
        }
    }

}

bool drawToLevel(const bool prio) {
    if ((renderFlags & (prio ? RENDER_FOREGROUND_HIGH : RENDER_FOREGROUND_LOW)) == 0)
        return true;

    // CREATE TEXTURE IF FIRST
    SDL_Texture*& level_texture = level_textures[LEVEL_HIGH * prio];

    // SET PALETTE AND SET AS RENDER TARGET
    const auto palette = prio ? high_prio_palette : low_prio_palette;
    SDL_SetTexturePalette(chunks_texture, palette);
    if (!SDL_SetRenderTarget(renderer, level_texture)) {
        SDL_Log("Couldn't set render target texture: %s", SDL_GetError());
        return false;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); // Transparent black
    SDL_RenderClear(renderer);

    bool ret = drawSelect(prio);

    SDL_SetRenderTarget(renderer, nullptr);
    return ret;
}

bool drawToBackground(const bool prio) {
    switch (gameData.getCurrentActBGEvent()) {
        default: {
            return true;
        }
    }
}