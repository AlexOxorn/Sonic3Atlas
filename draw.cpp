//
// Created by alexoxorn on 8/4/26.
//

#include "draw.h"

#include <cmath>
#include <format>
#include <numeric>
#include <ostream>

#include <SDL3/SDL.h>
#include "consts.h"
#include "structs.h"

#define GET_LEFTMOST int leftmostChunk = static_cast<int>(std::floor(scrollX / Chunk::WIDTH))
#define GET_TOPMOST int topmostChunk = static_cast<int>(std::floor(scrollY / Chunk::WIDTH))
#define GET_RIGHTMOST int rightmostChunk = static_cast<int>(std::ceil((scrollX + RENDER_WIDTH) / Chunk::WIDTH)) + 2
#define GET_BOTTOMMOST int bottommostChunk = static_cast<int>(std::ceil((scrollY + RENDER_HEIGHT) / Chunk::WIDTH)) + 2

namespace DEBUG {
    bool chunkInfo = false;
    bool swapFGBG = false;
    bool forceFG = false;
    static u32 previousBGEvent = 0;
    static std::array<u16, 9> previousBGVars;
} // namespace DEBUG

struct FramePosition {
    int x_pos;
    int y_pos;
    SDL_FlipMode flip;
};

struct MaskPosition {
    float y_start;
    float y_end;
};

using SpriteFrameData = std::variant<FramePosition, MaskPosition, std::monostate>;

static void renderEntry(int index, SDL_FRect src, SDL_FRect dst) {
    dst.x += static_cast<float>(SPRITE_WIDTH * (index % SPRITE_PER_TMP_TEXTURE_ROW));
    dst.y += static_cast<float>(SPRITE_WIDTH * (index / SPRITE_PER_TMP_TEXTURE_ROW));
    SDL_RenderTexture(renderer, mappings_texture, &src, &dst);
}

static bool drawFrame(const int index, const SpriteMappingFrame& frame, const BatCell art_tile, const int x_pos,
                      const int y_pos, const int relativeWaterLine, const SDL_FlipMode flip,
                      std::vector<SpriteFrameData>& positions, bool prio) {
    auto [size, entries] = frame;

    float y_min = std::numeric_limits<float>::infinity();
    float y_max = -std::numeric_limits<float>::infinity();
    bool mask_obj = false;

    for (auto& entry : entries | stdv::reverse) {
        const s64 mapID = SpriteMappingEntry::mappingIDs[entry.withBase(art_tile)];

        // Sprite Masking
        if (entry.withBase(art_tile).art_tile.vram_index == 0x7C0) {
            mask_obj = true;
            y_min = std::min(static_cast<float>(entry.y_pos + 128), y_min);
            y_max = std::max(static_cast<float>(entry.y_pos + 128 + (entry.dim.height + 1) * Tile::WIDTH), y_max);
            continue;
        }

        SDL_FRect src{.x = static_cast<float>(2 * (mapID % MAPPING_ENTRY_PER_ROW * SpriteMappingEntry::WIDTH)),
                      .y = static_cast<float>(mapID / MAPPING_ENTRY_PER_ROW * SpriteMappingEntry::WIDTH),
                      .w = static_cast<float>((entry.dim.width + 1) * Tile::WIDTH),
                      .h = static_cast<float>((entry.dim.height + 1) * Tile::WIDTH)};

        SDL_FRect dest{.x = static_cast<float>(entry.x_pos + 128),
                       .y = static_cast<float>(entry.y_pos + 128),
                       .w = static_cast<float>((entry.dim.width + 1) * Tile::WIDTH),
                       .h = static_cast<float>((entry.dim.height + 1) * Tile::WIDTH)};

        // Above Water
        if (!gameData.has_water || SPRITE_WIDTH <= (relativeWaterLine)) {
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

    if (mask_obj) {
        // if (art_tile.priority != prio)
        //     positions.emplace_back(std::monostate{});
        // else
        positions.emplace_back(MaskPosition(y_pos + y_min, y_pos + y_max));
    } else {
        positions.emplace_back(FramePosition(x_pos, y_pos, flip));
    }
    return true;
}

bool drawSprites(const bool prio) {
    if ((renderFlags & (prio ? RENDER_SPRITE_HIGH : RENDER_SPRITE_LOW)) == 0)
        return true;
    SDL_Texture*& target = prio ? sprite_texture_high : sprite_texture_low;

    const auto palette = prio ? high_prio_palette : low_prio_palette;
    constexpr int max_components = 9;
    SDL_SetTexturePalette(mappings_texture, palette);


    const GET_LEFTMOST;
    const GET_TOPMOST;
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

    const auto water_line_coord = (gameData.water_line - (topmostChunk * Chunk::WIDTH));

    std::vector<SpriteFrameData> positions;

    if (!SDL_SetRenderTarget(renderer, sprite_tmp_texture)) {
        SDL_Log("Couldn't set render target texture: %s", SDL_GetError());
        return false;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); // Transparent black
    SDL_RenderClear(renderer);

    int index = 0;

    for (auto& [object, frame, children] : gameData.sprites | stdv::reverse) {
        auto [x_pos, y_pos] = getTruePos(object.x_pos, object.y_pos, object.render_flags.use_level_coordinates);

        if (x_pos == -1 && y_pos == -1)
            continue;

        auto [size, entries] = frame;

        const auto flip = static_cast<SDL_FlipMode>(SDL_FLIP_VERTICAL * object.render_flags.vertical_mirror +
                                                    SDL_FLIP_HORIZONTAL * object.render_flags.horizontal_mirror);

        if (!SDL_SetRenderTarget(renderer, sprite_tmp_texture)) {
            SDL_Log("Couldn't set render target texture: %s", SDL_GetError());
            return false;
        }

        if (!object.render_flags.compound_sprite || object.mapping_frame != 0) {
            drawFrame(index++, frame, object.art_tile, x_pos, y_pos, water_line_coord - y_pos, flip, positions, prio);
        }

        for (const auto& [childFrame, childData] : stdv::zip(children, object.children)) {
            auto [sub_x_pos, sub_y_pos] =
                    getTruePos(childData.x_pos, childData.y_pos, object.render_flags.use_level_coordinates);
            drawFrame(index++,
                      childFrame,
                      object.art_tile,
                      sub_x_pos,
                      sub_y_pos,
                      water_line_coord - sub_y_pos,
                      flip,
                      positions,
                      prio);
        }
    }

    std::vector<int> loopOffsets;
    const float vertLoop = gameData.vertical_loop;
    if (gameData.screen_min_y < 0) {
        loopOffsets.resize(2 * LOOP_COUNT + 1);
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
        auto data = positions[i];
        if (std::holds_alternative<FramePosition>(data)) {
            auto [x, base_y, flip] = std::get<FramePosition>(data);
            for (const int loopOff : loopOffsets) {
                const int y = base_y + loopOff * gameData.vertical_loop;
                if (y + 256 < 0)
                    continue;
                if (y > RENDER_HEIGHT)
                    continue;

                SDL_FRect src{
                        .x = static_cast<float>(SPRITE_WIDTH * (i % SPRITE_PER_TMP_TEXTURE_ROW)),
                        .y = static_cast<float>(SPRITE_WIDTH * (i / SPRITE_PER_TMP_TEXTURE_ROW)),
                        .w = SPRITE_WIDTH,
                        .h = SPRITE_WIDTH,
                };

                SDL_FRect dest{
                        .x = static_cast<float>(x),
                        .y = static_cast<float>(y),
                        .w = SPRITE_WIDTH,
                        .h = SPRITE_WIDTH,
                };

                SDL_RenderTextureRotated(renderer, sprite_tmp_texture, &src, &dest, 0, nullptr, flip);
            }
        } else if (std::holds_alternative<MaskPosition>(data)) {
            auto [y_low, y_high] = std::get<MaskPosition>(data);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            for (const int loopOff : loopOffsets) {
                const float y = y_low + loopOff * gameData.vertical_loop;
                SDL_FRect dst{.x = 0.f, .y = y, .w = static_cast<float>(RENDER_WIDTH), .h = y_high - y_low};
                SDL_RenderFillRect(renderer, &dst);
            }
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
        loopOffsets.resize(LOOP_COUNT + 2);
        stdr::iota(loopOffsets.begin(), loopOffsets.end(), -1);
    } else {
        loopOffsets = {0};
    }

    const GET_LEFTMOST;
    const GET_TOPMOST;
    const s16 xOffset = leftmostChunk * Chunk::WIDTH;
    const s16 yOffset = topmostChunk * Chunk::WIDTH;

    auto locations = gameData.ring_data.locationSubrange();

    const auto first_ring = stdr::lower_bound(locations, xOffset, std::less{}, &ringLocation::x_pos);
    const auto last_ring = stdr::upper_bound(
            first_ring, locations.end(), xOffset + SCREEN_TEXTURE_WIDTH, std::less{}, &ringLocation::x_pos);

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

            SDL_FRect src{.x = static_cast<float>(2 * mapID * SpriteMappingEntry::WIDTH),
                          .y = static_cast<float>(0),
                          .w = static_cast<float>((entry.dim.width + 1) * Tile::WIDTH),
                          .h = static_cast<float>((entry.dim.height + 1) * Tile::WIDTH)};

            SDL_FRect dest{.x = static_cast<float>(entry.x_pos + x_shift),
                           .y = static_cast<float>(entry.y_pos + y_shift),
                           .w = static_cast<float>((entry.dim.width + 1) * Tile::WIDTH),
                           .h = static_cast<float>((entry.dim.height + 1) * Tile::WIDTH)};

            SDL_RenderTexture(renderer, mappings_texture, &src, &dest);
        }
    }
    SDL_SetRenderTarget(renderer, nullptr);
    return true;
}

static bool drawPlane2(const std::vector<std::vector<u8>>& chunks, const float xOffset, const float yOffset,
                       const int leftmostChunk, const int topmostChunk, const int rightmostChunk,
                       const int bottommostChunk, const float water_line_coord) {

    SDL_FRect src{.x = 0, .y = 0, .w = Chunk::WIDTH, .h = Chunk::WIDTH};
    SDL_FRect dest{.x = 0, .y = 0, .w = Chunk::WIDTH, .h = Chunk::WIDTH};
    auto getChunkIndex = [&chunks](u16 rowIndex, u16 columnIndex) -> std::pair<bool, int> {
        if (gameData.screen_min_y < 0) {
            while (rowIndex < 0)
                rowIndex += static_cast<int>(chunks.size());
            while (rowIndex >= chunks.size())
                rowIndex -= static_cast<int>(chunks.size());
        }
        bool loop = false;
        while (true) {
            try {
                return std::make_pair(loop, chunks.at(rowIndex).at(columnIndex));
            } catch (const std::out_of_range& e) {
                if (config.horizontal_oob != Options::OOB::LOOP) {
                    return {false, -1};
                }
                if (chunks.empty()) {
                    return {false, -1};
                }
                if (rowIndex >= chunks.size())
                    return {false, -1};
                rowIndex += columnIndex / chunks[0].size();
                columnIndex %= chunks[0].size();
                loop = true;
            }
        }
    };

    SDL_Palette* activePalette = SDL_GetTexturePalette(chunks_texture);
    SDL_Palette* greyPal = activePalette == low_prio_palette ? low_grey_palette : high_grey_palette;

#define CHUNK_COLOR grey ? grey_chunks_texture : chunks_texture

    for (int rowIndex_base = topmostChunk; rowIndex_base <= bottommostChunk; rowIndex_base++) {
        const int rowIndex = rowIndex_base;
        dest.y = static_cast<float>((rowIndex_base - topmostChunk) * Chunk::WIDTH) + yOffset;

        const bool land = dest.y < water_line_coord || !gameData.has_water;
        const bool water = dest.y + dest.h >= water_line_coord && gameData.has_water;

        for (int columnIndex_base = leftmostChunk; columnIndex_base <= rightmostChunk; columnIndex_base++) {
            int columnIndex = columnIndex_base;
            const auto [grey, chunkIndex] = getChunkIndex(rowIndex, columnIndex);
            if (chunkIndex < 0)
                continue;
            if (water && land) {
                // LAND PART
                dest.h = water_line_coord - dest.y;
                dest.x = static_cast<float>((columnIndex - leftmostChunk) * Chunk::WIDTH) + xOffset;

                src.x = static_cast<float>(chunkIndex % 8 * Chunk::WIDTH * 2);
                src.y = static_cast<float>(chunkIndex / 8 * Chunk::WIDTH);
                src.h = dest.h;

                SDL_RenderTexture(renderer, CHUNK_COLOR, &src, &dest);
                SDL_RenderTexture(renderer, translucency_mask_texture, &src, &dest);

                // WATER PART
                dest.h = (dest.y + Chunk::WIDTH - water_line_coord);
                dest.y = water_line_coord;

                dest.x = static_cast<float>((columnIndex_base - leftmostChunk) * Chunk::WIDTH) + xOffset;

                src.x = static_cast<float>(chunkIndex % 8 * Chunk::WIDTH * 2 + Chunk::WIDTH);
                src.y = static_cast<float>(chunkIndex / 8 * Chunk::WIDTH + (Chunk::WIDTH - dest.h));
                src.h = dest.h;


                SDL_RenderTexture(renderer, CHUNK_COLOR, &src, &dest);
                SDL_RenderTexture(renderer, translucency_mask_texture, &src, &dest);

                dest.h = Chunk::WIDTH;
                dest.y = static_cast<float>((rowIndex_base - topmostChunk) * Chunk::WIDTH) + yOffset;
                src.h = Chunk::WIDTH;
            } else if (land) {
                dest.x = static_cast<float>((columnIndex_base - leftmostChunk) * Chunk::WIDTH) + xOffset;
                src.x = static_cast<float>(chunkIndex % 8 * Chunk::WIDTH * 2);
                src.y = static_cast<float>(chunkIndex / 8 * Chunk::WIDTH);
                SDL_RenderTexture(renderer, CHUNK_COLOR, &src, &dest);
                SDL_RenderTexture(renderer, translucency_mask_texture, &src, &dest);
            } else if (water) {
                dest.x = static_cast<float>((columnIndex_base - leftmostChunk) * Chunk::WIDTH) + xOffset;
                src.x = static_cast<float>(chunkIndex % 8 * Chunk::WIDTH * 2 + Chunk::WIDTH);
                src.y = static_cast<float>(chunkIndex / 8 * Chunk::WIDTH);

                SDL_RenderTexture(renderer, CHUNK_COLOR, &src, &dest);
                SDL_RenderTexture(renderer, translucency_mask_texture, &src, &dest);
            } else {
                fprintf(stderr, "Error, neither water nor land\n");
                fprintf(stderr, "Above: dest.y           < water_line_coord === %f < %f\n", dest.y, water_line_coord);
                fprintf(stderr,
                        "Under: dest.y + dest.h >= water_line_coord === %f < %f\n",
                        dest.y + dest.h,
                        water_line_coord);
            }
        }

        if (DEBUG::chunkInfo) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            for (int i = 1; i < RENDER_WIDTH / Chunk::WIDTH; i++) {
                SDL_RenderLine(renderer, i * Chunk::WIDTH, 0, i * Chunk::WIDTH, RENDER_HEIGHT);
                SDL_RenderLine(renderer, i * Chunk::WIDTH + 1, 0, i * Chunk::WIDTH + 1, RENDER_HEIGHT);
                SDL_RenderLine(renderer, i * Chunk::WIDTH + 2, 0, i * Chunk::WIDTH + 2, RENDER_HEIGHT);
            }
            for (int i = 1; i < RENDER_HEIGHT / Chunk::WIDTH; i++) {
                SDL_RenderLine(renderer, 0, i * Chunk::WIDTH, RENDER_WIDTH, i * Chunk::WIDTH);
                SDL_RenderLine(renderer, 0, i * Chunk::WIDTH + 1, RENDER_WIDTH, i * Chunk::WIDTH + 1);
                SDL_RenderLine(renderer, 0, i * Chunk::WIDTH + 2, RENDER_WIDTH, i * Chunk::WIDTH + 2);
            }

            for (int rowIndex = topmostChunk; rowIndex <= bottommostChunk; rowIndex++) {
                for (int columnIndex = leftmostChunk; columnIndex <= rightmostChunk; columnIndex++) {
                    const auto [_, chunkIndex] = getChunkIndex(rowIndex, columnIndex);
                    auto chunkPOS = std::format("{},{}", columnIndex, rowIndex);
                    auto chunkIndexS = std::format("{}", chunkIndex);
                    const float X = Chunk::WIDTH * (columnIndex - leftmostChunk);
                    const float Y = Chunk::WIDTH * (rowIndex - topmostChunk);
                    SDL_FRect dest{.x = X, .y = Y, .w = 48, .h = 24};
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_RenderFillRect(renderer, &dest);
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                    SDL_RenderDebugText(renderer, X + 3, Y + 3, chunkPOS.c_str());
                    SDL_RenderDebugText(renderer, X + 3, Y + 14, chunkIndexS.c_str());
                }
            }
        }
    }

    return true;
}

static bool drawPlane(const std::vector<std::vector<u8>>& chunks, const float xOffset = 0, const float yOffset = 0) {

    const GET_LEFTMOST;
    const GET_TOPMOST;
    const GET_RIGHTMOST;
    const GET_BOTTOMMOST;
    const auto water_line_coord = static_cast<float>(gameData.water_line - (topmostChunk * Chunk::WIDTH));
    return drawPlane2(
            chunks, xOffset, yOffset, leftmostChunk, topmostChunk, rightmostChunk, bottommostChunk, water_line_coord);
}


static bool drawToBackgroundDefault(const bool prio) {
    const int offsetX = gameData.screen_position_A.first - gameData.screen_position_B.first;
    const int offsetY = gameData.screen_position_A.second - gameData.screen_position_B.second;
    const GET_LEFTMOST;
    const GET_TOPMOST;
    const auto water_line_coord = static_cast<float>(gameData.water_line - (topmostChunk * Chunk::WIDTH));
    auto res = drawPlane2(DEBUG::swapFGBG ? gameData.level_chunks : gameData.background_chunks,
                          offsetX - leftmostChunk * Chunk::WIDTH,
                          offsetY - topmostChunk * Chunk::WIDTH,
                          0,
                          0,
                          gameData.background_chunks.empty() ? 0 : gameData.background_chunks[0].size(),
                          gameData.background_chunks.size(),
                          water_line_coord);

    return res;
}

static bool drawToLevelDefault(const bool prio) {
    auto res = drawPlane(DEBUG::swapFGBG ? gameData.background_chunks : gameData.level_chunks);
    return res;
}

static bool drawBGSubset(int lowX, int lowY, int highX, int highY) {
    const int offsetX = gameData.screen_position_A.first - gameData.screen_position_B.first;
    const int offsetY = gameData.screen_position_A.second - gameData.screen_position_B.second;
    const GET_LEFTMOST;
    const GET_TOPMOST;
    const auto water_line_coord = static_cast<float>(gameData.water_line - (topmostChunk * Chunk::WIDTH));
    auto res = drawPlane2(gameData.background_chunks,
                          offsetX + (lowX - leftmostChunk) * Chunk::WIDTH,
                          offsetY + (lowY - topmostChunk) * Chunk::WIDTH,
                          lowX,
                          lowY,
                          highX,
                          highY,
                          water_line_coord);

    return res;
}

namespace AIZ2 {
    static constexpr u8 E_FLYING_BOMB_SHIP = 0x2;

    static constexpr s32 propellerMapAddr = 0x23C182;
    static constexpr s32 SHIP_CHUNK_X = 123;
    static constexpr s32 SHIP_CHUNK_Y = 19;
    static constexpr s32 SHIP_CHUNK_WIDTH = 6;
    static constexpr s32 SHIP_CHUNK_HEIGHT = 2;
    static constexpr s32 SHIP_PIXEL_WIDTH = SHIP_CHUNK_WIDTH * Chunk::WIDTH;
    static constexpr s32 SHIP_PIXEL_HEIGHT = SHIP_CHUNK_HEIGHT * Chunk::WIDTH;

    static constexpr s32 PROPELLER_OFFSET_X = -SHIP_PIXEL_WIDTH + 3 * Chunk::WIDTH / 2 + Tile::WIDTH / 2;
    static constexpr s32 PROPELLER_OFFSET_Y = -SHIP_PIXEL_HEIGHT + 2 * Tile::WIDTH;

    static std::optional<ObjectTableEntry> findFrontPropellerSprite() {
        auto propellers = gameData.sprites | stdv::transform(&Sprite::object) |
                stdv::filter([](const ObjectTableEntry& obj) { return obj.mappings == propellerMapAddr; });
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

        SDL_FRect src{.x = 0, .y = 0, .w = Chunk::WIDTH, .h = Chunk::WIDTH};
        SDL_FRect dest{.x = 0, .y = 0, .w = Chunk::WIDTH, .h = Chunk::WIDTH};

        const GET_LEFTMOST;
        const GET_TOPMOST;

        for (int j = 0; j < SHIP_CHUNK_HEIGHT; j++) {
            auto ship_y = static_cast<float>(propeller.y_pos + PROPELLER_OFFSET_Y);
            ship_y += static_cast<float>(j) * Chunk::WIDTH;
            dest.y = ship_y - topmostChunk * Chunk::WIDTH;
            ;

            for (int i = 0; i < SHIP_CHUNK_WIDTH; ++i) {
                auto ship_x = static_cast<float>(propeller.x_pos + PROPELLER_OFFSET_X);
                ship_x += static_cast<float>(i) * Chunk::WIDTH;
                dest.x = ship_x - leftmostChunk * Chunk::WIDTH;

                auto chunkIndex = gameData.level_chunks[j + SHIP_CHUNK_Y][i + SHIP_CHUNK_X];

                src.x = static_cast<float>(chunkIndex % 8 * Chunk::WIDTH * 2);
                src.y = static_cast<float>(chunkIndex / 8 * Chunk::WIDTH);

                SDL_RenderTexture(renderer, chunks_texture, &src, &dest);
            }
        }
        return true;
    }
} // namespace AIZ2


namespace HCZ2 {
    constexpr u8 WALL_EVENT = 0x1;
    constexpr int WALL_CHUNK_START_X = 4;
    constexpr int WALL_CHUNK_START_Y = 2;
    constexpr int WALL_CHUNK_END_X = 8;
    constexpr int WALL_CHUNK_END_Y = 6;

    static bool drawWall() {
        return drawBGSubset(WALL_CHUNK_START_X, WALL_CHUNK_START_Y, WALL_CHUNK_END_X, WALL_CHUNK_END_Y);
    }
} // namespace HCZ2

namespace CNZ1 {
    constexpr inline u8 PRE_BOSS_EVENT = 0x1;
    constexpr inline u8 BOSS_EVENT = 0x2;
    constexpr inline u8 POST_BOSS_EVENT = 0x3;

    template<bool loop> static bool drawBossBackground(const bool prio) {
        int offsetX = gameData.screen_position_A.first - gameData.screen_position_B.first;
        int offsetY = gameData.screen_position_A.second - gameData.screen_position_B.second;
        const GET_LEFTMOST;
        const GET_TOPMOST;
        const auto water_line_coord = static_cast<float>(gameData.water_line - (topmostChunk * Chunk::WIDTH));

        if constexpr (loop) {
            while (offsetY < 0)
                offsetY += 2 * Chunk::WIDTH;
            while (offsetY >= 2 * Chunk::WIDTH)
                offsetY -= 2 * Chunk::WIDTH;
        }

        auto res = drawPlane2(gameData.background_chunks,
                              offsetX - leftmostChunk * Chunk::WIDTH,
                              offsetY - topmostChunk * Chunk::WIDTH,
                              0,
                              0,
                              gameData.background_chunks.empty() ? 0 : gameData.background_chunks[0].size(),
                              gameData.background_chunks.size(),
                              water_line_coord);

        return res;
    }

} // namespace CNZ1

namespace SOZ1 {
    constexpr inline u8 BOSS_INIT = 0x1;
    constexpr inline u8 BOSS_LOOP = 0x2;
    constexpr inline u8 LEVEL_TRANS1 = 0x3;
    constexpr inline u8 LEVEL_TRANS2 = 0x4;
    constexpr inline u8 LEVEL_TRANS3 = 0x5;

    constexpr int PYRAMID_CHUNK_START_X = 11;
    constexpr int PYRAMID_CHUNK_START_Y = 2;
    constexpr int PYRAMID_CHUNK_END_X = 14;
    constexpr int PYRAMID_CHUNK_END_Y = 6;
    static bool drawPyramid() {
        return drawBGSubset(PYRAMID_CHUNK_START_X, PYRAMID_CHUNK_START_Y, PYRAMID_CHUNK_END_X, PYRAMID_CHUNK_END_Y);
    }
} // namespace SOZ1

namespace SSZ1 {
    static std::optional<s16> start_colapse = std::nullopt;

    static bool drawSpiral(bool prio) {
        constexpr int BG_CHUNK_START_X = 0;
        constexpr int BG_CHUNK_START_Y = 0;
        constexpr int BG_CHUNK_END_X = 55;
        constexpr int BG_CHUNK_END_Y = 21;
        auto pieces =
                gameData.sprites | stdv::transform(&Sprite::object) | stdv::filter([](const ObjectTableEntry& entry) {
                    return entry.routine_address == 0x584DE || entry.routine_address == 0x58468;
                });

        auto top_piece = stdr::fold_left_first(pieces | stdv::transform(&ObjectTableEntry::y_pos),
                                               [](s16 l, s16 r) { return std::min(l, r); });

        int res = drawBGSubset(BG_CHUNK_START_X, BG_CHUNK_START_Y, BG_CHUNK_END_X, BG_CHUNK_END_Y);

        if (start_colapse || top_piece) {
            short cutoff;
            if (top_piece && start_colapse) {
                cutoff = std::min(*top_piece, *start_colapse);
            } else {
                cutoff = top_piece.value_or(*start_colapse);
            }
            auto bottom = gameData.screen_max_y + GENESIS_RESOLUTION.second;
            auto old_start = start_colapse.value_or(*top_piece);

            start_colapse = cutoff;
            cutoff -= 8;
            if (!top_piece || old_start < *top_piece) {
                cutoff -= 8;
            }

            auto leftPiece = gameData.screen_min_x;
            auto rightPiece = gameData.screen_max_x + GENESIS_RESOLUTION.first;


            const GET_LEFTMOST;
            const GET_TOPMOST;

            constexpr int BlueSkyChunk = 2;
            auto BlueSkyColorIndex = gameData.chunks.getBytes(BlueSkyChunk, gameData.blocks, gameData.tileset)[0][0];
            auto BlueSky = gameData.palette.lines[(BlueSkyColorIndex & 0x30) >> 4].colors[BlueSkyColorIndex & 0xF];


            SDL_SetRenderDrawColor(renderer, BlueSky.red, BlueSky.green, BlueSky.blue, prio ? 0 : 255);

            SDL_FRect dst{
                    .x = static_cast<float>(leftPiece - leftmostChunk * Chunk::WIDTH),
                    .y = static_cast<float>(cutoff - topmostChunk * Chunk::WIDTH),
                    .w = static_cast<float>(rightPiece - leftPiece),
                    .h = static_cast<float>((bottom - cutoff)),
            };

            // printf("dst: %.1f | %.1f | %.1f | %.1f\n", dst.x, dst.y, dst.w, dst.h);

            SDL_RenderFillRect(renderer, &dst);
        }

        return res;
    }
} // namespace SSZ1

namespace FBZ2 {
    constexpr u8 BOSS_CLIMB = 0x4;
    constexpr int BG_CHUNK_START_X = 8;
    constexpr int BG_CHUNK_START_Y = 1;
    constexpr int BG_CHUNK_END_X = 36;
    constexpr int BG_CHUNK_END_Y = 11;
    static bool drawChaseBG() {
        return drawBGSubset(BG_CHUNK_START_X, BG_CHUNK_START_Y, BG_CHUNK_END_X, BG_CHUNK_END_Y);
    }
} // namespace FBZ2

static bool drawSelect(bool prio) {
    switch (gameData.getCurrentActFGEvent()) {
        case LEVEL_ACT_EVENT(ANGLE_ISLAND_ZONE, 2, AIZ2::E_FLYING_BOMB_SHIP): {
            const bool x = drawToLevelDefault(prio);
            const bool y = prio ? AIZ2::drawShip() : true;
            return x && y;
        }
        case LEVEL_ACT_EVENT(HYDRO_CITY_ZONE, 1, 0): {
            // The start of Hydro City doesn't use the water palette for below the water line
            // Instead using a unique above ground palette to simulate water being contained
            if (gameData.water_line == 0x500)
                gameData.water_line = 0x680;
            return drawToLevelDefault(prio);
        }
        default: {
            return drawToLevelDefault(prio);
        }
    }
}

static bool drawSelectBG(const bool prio) {
    auto event = gameData.getCurrentActBGEvent();
    if (event != DEBUG::previousBGEvent) {
        printf("New BG Event %08x\n", event);
    }

    DEBUG::previousBGEvent = event;
    switch (event) {
        case LEVEL_ACT_EVENT(HYDRO_CITY_ZONE, 2, HCZ2::WALL_EVENT): {
            return prio ? HCZ2::drawWall() : true;
        }
        case LEVEL_ACT_EVENT(MARBLE_GARDEN_ZONE, 2, 1): {
            // ------------------------------------------------------------
            // For Sonic
            // If between 0x800 and 0x900 Y and > 0x34C0 X, use second BG move
            // If bgEventVar0 is no longer 8, delete collapse manager
            // ------------------------------------------------------------

            // ------------------------------------------------------------
            // For Knuckles
            // If between 0x80 and 0x180 Y and > 0x3800 X, use first BG move
            // If bgEventVar0 is no longer 4, delete collapse manager
            // ------------------------------------------------------------

            if (gameData.bgEventVars[0] == 4 || gameData.bgEventVars[0] == 8) {
                return drawToBackgroundDefault(prio);
            }
            return true;
        }
        case LEVEL_ACT_EVENT(CARNIVAL_NIGHT_ZONE, 1, CNZ1::PRE_BOSS_EVENT):
        case LEVEL_ACT_EVENT(CARNIVAL_NIGHT_ZONE, 1, CNZ1::POST_BOSS_EVENT):
        case LEVEL_ACT_EVENT(CARNIVAL_NIGHT_ZONE, 1, CNZ1::POST_BOSS_EVENT + 1): {
            return CNZ1::drawBossBackground<false>(prio);
        }
        case LEVEL_ACT_EVENT(CARNIVAL_NIGHT_ZONE, 1, CNZ1::BOSS_EVENT): {
            return CNZ1::drawBossBackground<true>(prio);
        }

        case LEVEL_ACT_EVENT(FLYING_BATTERY_ZONE, 2, FBZ2::BOSS_CLIMB): {
            return FBZ2::drawChaseBG();
        }

        case LEVEL_ACT_EVENT(SANDOPOLIS_ZONE, 1, SOZ1::BOSS_INIT):
        case LEVEL_ACT_EVENT(SANDOPOLIS_ZONE, 1, SOZ1::BOSS_LOOP):
        case LEVEL_ACT_EVENT(SANDOPOLIS_ZONE, 1, SOZ1::LEVEL_TRANS1):
        case LEVEL_ACT_EVENT(SANDOPOLIS_ZONE, 1, SOZ1::LEVEL_TRANS2):
        case LEVEL_ACT_EVENT(SANDOPOLIS_ZONE, 1, SOZ1::LEVEL_TRANS3): {
            return prio ? SOZ1::drawPyramid() : true;
        }

        case LEVEL_ACT_EVENT(SKY_SANCTUARY_ZONE, 1, 0): {
            return SSZ1::drawSpiral(prio);
        }
        case LEVEL_ACT_EVENT(SKY_SANCTUARY_ZONE, 1, 1):
        case LEVEL_ACT_EVENT(SKY_SANCTUARY_ZONE, 1, 2):
        case LEVEL_ACT_EVENT(SKY_SANCTUARY_ZONE, 1, 3): {
            SSZ1::start_colapse = std::nullopt;
        }

        default: {
            if (DEBUG::forceFG) {
                return drawToBackgroundDefault(prio);
            }
            return true;
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
    const auto grey = prio ? high_grey_palette : low_grey_palette;
    SDL_SetTexturePalette(chunks_texture, palette);
    SDL_SetTexturePalette(grey_chunks_texture, grey);
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
    if ((renderFlags & (prio ? RENDER_BACKGROUND_HIGH : RENDER_BACKGROUND_LOW)) == 0)
        return true;

    // CREATE TEXTURE IF FIRST
    SDL_Texture*& level_texture = level_textures[LEVEL_HIGH * prio + LEVEL_BG];

    // SET PALETTE AND SET AS RENDER TARGET
    const auto palette = prio ? high_prio_palette : low_prio_palette;
    const auto grey = prio ? high_grey_palette : low_grey_palette;
    SDL_SetTexturePalette(chunks_texture, palette);
    SDL_SetTexturePalette(grey_chunks_texture, grey);
    if (!SDL_SetRenderTarget(renderer, level_texture)) {
        SDL_Log("Couldn't set render target texture: %s", SDL_GetError());
        return false;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); // Transparent black
    SDL_RenderClear(renderer);

    bool ret = drawSelectBG(prio);

    SDL_SetRenderTarget(renderer, nullptr);
    return ret;
}

bool drawHudText() {
    using Frame = SpriteMappingFrame;
    std::vector<FramePosition> out;

    if (!SDL_SetTexturePalette(mappings_texture, full_palette)) {
        SDL_Log("Couldn't set palette texture: %s", SDL_GetError());
        return false;
    }
    if (!SDL_SetRenderTarget(renderer, hud_texture)) {
        SDL_Log("Couldn't set render target texture: %s", SDL_GetError());
        return false;
    };
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    /*
     * RING MAPPING:
     *      0 = Full = 0
     *      1 = No Ring (Ring = 0 flash)
     *      2 = No Time (Time > 9min flash)
     *      3 = 1 + 2
     *      4 = Just Rings (Bonus Stages?)
     *      5 = 4 + 1
     */
    constexpr int testMapping = 0;
    for (auto& entry : hud_mappings[testMapping].entries | stdv::reverse) {
        const s64 mapID = SpriteMappingEntry::mappingIDs[entry.withBase(HUD_ART_TILE_TMP)];


        SDL_FRect src{.x = static_cast<float>(2 * (mapID % MAPPING_ENTRY_PER_ROW * SpriteMappingEntry::WIDTH)),
                      .y = static_cast<float>(mapID / MAPPING_ENTRY_PER_ROW * SpriteMappingEntry::WIDTH),
                      .w = static_cast<float>((entry.dim.width + 1) * Tile::WIDTH),
                      .h = static_cast<float>((entry.dim.height + 1) * Tile::WIDTH)};

        SDL_FRect dst{.x = static_cast<float>(entry.x_pos + 8),
                      .y = static_cast<float>(entry.y_pos + 128 + 8),
                      .w = static_cast<float>((entry.dim.width + 1) * Tile::WIDTH),
                      .h = static_cast<float>((entry.dim.height + 1) * Tile::WIDTH)};

        if (!SDL_RenderTexture(renderer, mappings_texture, &src, &dst)) {
            SDL_Log("Couldn't render texture onto hud texture: %s", SDL_GetError());
        };
    }
    return true;
}
