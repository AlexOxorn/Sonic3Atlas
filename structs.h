//
// Created by alexoxorn on 7/23/26.
//

#ifndef SONIC3ATLUS_STRUCTS_H
#define SONIC3ATLUS_STRUCTS_H

//
// Created by alexoxorn on 7/23/26.
//


#include "common.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <bitset>
#include <bit>
#include <cstring>
#include <ranges>
#include <unistd.h>
#include <type_traits>
#include <sys/socket.h>
#include <utility>
#include <map>
#include <ranges>
#include <set>
#include <vector>
#include <unistd.h>
#include <unordered_set>


/*

==============
COMMON METHODS
==============

-------------------------------------
static T fromSocket(FILE* dataStream)
-------------------------------------
Constructs a T using the data from a file pointer.
Used to deserialize data coming from the Game Data
Log file


--------------------------
pixelType build_bytes(...)
--------------------------
Use to construct a raw 2D array of color indexes
representing the pixels for a given Cell type


-----------------------
pixelType getBytes(...)
-----------------------
If the cell's pixel data already exists in the
respective static cache, return the cached pixels.

Otherwise, call the appropriate build_bytes(...),
the cache and return the result.

========================
Caching
========================
Caches are used when building raw bytes
so that repeated blocks/tiles/etc need not be
recomputed each time.

Cache's are cleared at the start of
each frame so that the any bytes requested
will always have up to date tile data

========================
Chunk Dependencies
========================
When creating new chunks, create a map between
between every tile and which chunks use them.

This allows us only update the Master Chunk Texture
for chunks that actually need updating

`gameData.newly_updated_tiles()` should be added to
whenever a tile is updated, and only cleared, after
rendering is complete.
*/

struct BlockMap;
struct BatCell;
struct ChunkMap;
struct RingMappingEntry;


#if defined(__LITTLE_ENDIAN_BITFIELD)
static_assert(false, "Little Endian Bitfield is not supported");
#endif

template <typename... Args>
auto tuple_to_array(const std::tuple<Args...>& t) {
    return std::apply([](auto... elems) {
        using CommonType = std::common_type_t<Args...>;

        return std::array<CommonType, sizeof...(Args)>{ static_cast<CommonType>(elems)... };
    }, t);
}

template <typename T>
auto zip_array(T& arr) {
    return std::apply([](auto&... args) {return stdr::zip_view(args...);}, arr) | stdv::transform([](const auto& x) { return tuple_to_array(x); });
}

inline size_t recvStrict(FILE* dataStream, void *buf, const int len) {
    size_t received = 0;
    const auto  _buf = static_cast<char*>(buf);
    while (received < len) {
        const auto count = fread(_buf + received, 1, len - received, dataStream);
        if (count == 0) {
            break;
        }
        received += count;
    }
    return received;
}

constexpr std::array<u8, 8> colormap {0, 52, 87, 116, 144, 172, 206, 255};

constexpr u8 color_3bit_to_8bit(const u8 color3) {
    // return (color3 << 5) | (color3 << 2) | (color3 >> 1);
    return colormap[color3];
}

struct Color3Bit {
    u8 red: 3;
    u8 green: 3;
    u8 blue: 3;

    static Color3Bit fromSocket(FILE* dataStream) {
        Color3Bit result{};

        u8 data[2];
        recvStrict(dataStream, reinterpret_cast<char*>(&data), sizeof(result));

        result.red = (data[1]>>1) & 0xFFF;
        result.green = (data[1]>>5) & 0xFFF;
        result.blue = (data[0]>>1) & 0xFFF;

        return result;
    }
};

struct Color8Bit {
    u8 red;
    u8 green;
    u8 blue;

    static Color8Bit fromSocket(FILE* dataStream) {
        const auto [red, green, blue] = Color3Bit::fromSocket(dataStream);
        return Color8Bit(
            color_3bit_to_8bit(red),
            color_3bit_to_8bit(green),
            color_3bit_to_8bit(blue)
        );
    }
};

struct PaletteLine {
    std::array<Color8Bit, 0x10> colors;

    static PaletteLine fromSocket(FILE* dataStream) {
        PaletteLine result{};
        stdr::generate(result.colors, [=] { return Color8Bit::fromSocket(dataStream); });
        return result;
    }
};

struct Palette {
    std::array<PaletteLine, 0x4> lines;
    static Palette fromSocket(FILE* dataStream) {
        Palette result{};
        stdr::generate(result.lines, [=] { return PaletteLine::fromSocket(dataStream); });
        return result;
    }
};

struct Tile {
    static constexpr auto WIDTH = 8;

    struct tilerow {
        u8 data[4];
        operator std::array<u8, 8>() const {
            return {
                static_cast<u8>(data[0] >> 4),
                static_cast<u8>(data[0] & 0xF),
                static_cast<u8>(data[1] >> 4),
                static_cast<u8>(data[1] & 0xF),
                static_cast<u8>(data[2] >> 4),
                static_cast<u8>(data[2] & 0xF),
                static_cast<u8>(data[3] >> 4),
                static_cast<u8>(data[3] & 0xF),
            };
        }
    };

    std::array<std::array<u8, 0x8>, 0x8> pixels;

    static Tile fromSocket(FILE* dataStream) {
        std::array<tilerow, 0x8> result{};
        Tile tile{};
        recvStrict(dataStream, reinterpret_cast<char*>(&result), sizeof(result));
        stdr::copy(result, tile.pixels.begin());
        return tile;
    }

    static Tile fromBytes(u8*& dataStream) {
        std::array<tilerow, 0x8> result{};
        Tile tile{};
        memcpy(&result, dataStream, sizeof(result));
        stdr::copy(result, tile.pixels.begin());
        dataStream += sizeof(result);
        return tile;
    }
};

struct TileSet {
    static constexpr auto COUNT = 0x800;
    std::array<Tile, COUNT> tiles;

    static TileSet fromSocket(FILE* dataStream) {
        TileSet result{};
        stdr::generate(result.tiles, [=] { return Tile::fromSocket(dataStream); });
        return result;
    }
    static TileSet fromBytes(u8* dataStream) {
        TileSet result{};
        stdr::generate(result.tiles, [&] { return Tile::fromBytes(dataStream); });
        return result;
    }
};


struct BatCell {
    u16 vram_index:11;
    bool flip_x : 1;
    bool flip_y : 1;
    u8 palette : 2;
    bool priority : 1;

    auto operator<=>(const BatCell& rhs) const = default;

    using pixelType = std::array<std::array<u8, 8>, 8>;
    using Map = std::map<BatCell, pixelType>;

    static inline Map batCache;

    static BatCell fromBytes(const u8* data) {
        return {
            .vram_index = static_cast<u16> (data[1] + (data[0]&0b111) * (1 << 8)),
            .flip_x = static_cast<bool> (data[0]&0b1000),
            .flip_y = static_cast<bool> (data[0]&0b10000),
            .palette = static_cast<u8> ((data[0] >> 5)&0b11),
            .priority = static_cast<bool> (data[0] & 0b10000000),
        };
    }

    BatCell operator+(const BatCell& other) const {
        const u16 l_tile = vram_index;
        const u16 r_tile = other.vram_index;
        const u8 l_x = flip_x;
        const u8 r_x = other.flip_x;
        const u8 l_y = flip_y;
        const u8 r_y = other.flip_y;
        const u8 l_pal = palette;
        const u8 r_pal = other.palette;
        const u8 l_prio = priority;
        const u8 r_prio = other.priority;

        u16 new_tile = l_tile + r_tile;
        u16 carry = new_tile >> 11;
        new_tile &= 0b111'1111'1111;

        u16 new_x = l_x + r_x + carry;
        carry = new_x >> 1;
        new_x &= 1;

        u16 new_y = l_y + r_y + carry;
        carry = new_y >> 1;
        new_y &= 1;

        u16 new_palette = l_pal + r_pal + carry;
        carry = new_palette >> 2;
        new_palette &= 0b11;

        u16 new_prio = l_prio + r_prio + carry;
        new_prio &= 1;

        return BatCell{
            .vram_index = new_tile,
            .flip_x = !!new_x,
            .flip_y = !!new_y,
            .palette = static_cast<u8>(new_palette),
            .priority = !!new_prio
        };
    }

    static BatCell fromSocket(FILE* dataStream) {
        u8 data[sizeof(BatCell)];
        recvStrict(dataStream, reinterpret_cast<char*>(data), sizeof(data));
        return BatCell::fromBytes(data);
    }

private:
    void for_rows(const stdr::range auto& rows, pixelType& output, const auto& map) const {
        for (auto [row, outRow]: stdr::zip_view(rows, output)) {
            if (flip_x) {
                stdr::transform(stdr::reverse_view(row), outRow.begin(), map);
            } else {
                stdr::transform(row, outRow.begin(), map);
            }
        }
    };
    [[nodiscard]] pixelType build_bytes(const TileSet& tile_set) const {
        auto map_palette = [this] (const u8 pixel) -> u8 {
            if (pixel == 0)
                return 0;
            int color = pixel + 0x10 * palette;
            if (priority)
                color |= 1<<7;
            return color;
        };
        auto [pixels] = tile_set.tiles[vram_index];
        std::array<std::array<u8, 8>, 8>  result{};


        if (flip_y) {
            for_rows(stdr::reverse_view(pixels), result, map_palette);
        } else {
            for_rows(pixels, result, map_palette);
        }
        return result;
    }
public:
    [[nodiscard]] pixelType getBytes(const TileSet& tile_set) const {
        if (const auto bb = batCache.find(*this); bb != std::end(batCache)) {
            return bb->second;
        }
        return (batCache[*this] = build_bytes(tile_set));
    }
};


struct Block {
    std::array<std::array<BatCell, 0x2>, 0x2> cells;
    static Block fromSocket(FILE* dataStream) {
        Block result{};
        for (auto &row: result.cells) {
            for (auto &cell: row) {
                cell = BatCell::fromSocket(dataStream);
            }
        }
        return result;
    }

    using pixelType = std::array<std::array<u8, 16>, 16>;
    using rowType = std::array<BatCell::pixelType, 0x2>;

private:
    [[nodiscard]] std::array<std::array<u8, 16>, 16> build_bytes(const TileSet& tile_set) const {
        std::array<std::array<u8, 16>, 16>  result{};

        std::array tiles {rowType{
                cells[0][0].getBytes(tile_set),
                cells[0][1].getBytes(tile_set),
            }, rowType{
                cells[1][0].getBytes(tile_set),
                cells[1][1].getBytes(tile_set),
            }
        };

        auto row_view = std::apply([](auto... args) {return stdr::zip_view(args...);}, tiles);
        auto out = result[0].data();

        for (auto [lcell, rcell] : tiles) {
            for (auto [lrow, rrow] :
                stdr::zip_view(lcell, rcell)) {
                stdr::copy(lrow, out);
                stdr::copy(rrow, out+8);
                out += 16;
            }
        }

        return result;
    }
    friend BlockMap;
};

struct BlockMap {
    static constexpr auto SIZE =0xA800 - 0x9000;
    static constexpr auto COUNT = SIZE/sizeof(Block);
    std::array<Block, COUNT> blocks;
    static inline std::array<Block::pixelType, COUNT> block_pixels;
    static inline std::bitset<COUNT> computed_pixels;

    static BlockMap fromSocket(FILE* dataStream) {
        BlockMap result{};
        stdr::generate(result.blocks, [=] { return Block::fromSocket(dataStream); });
        return result;
    }

    [[nodiscard]] Block::pixelType getBytes(const long index, const TileSet& tile_set) const {
        if (index >= computed_pixels.size()) {
            return Block::pixelType{};
        }
        if (computed_pixels[index]) {
            return block_pixels[index];
        }
        block_pixels[index] = blocks[index].build_bytes(tile_set);
        computed_pixels.set(index);
        return block_pixels[index];
    }
};

struct ChunkTile {
    u16 vram_index:10;
    bool flip_x : 1;
    bool flip_y : 1;
    u8 std_solidity : 2;
    u8 alt_solidity : 2;

    auto operator<=>(const ChunkTile& rhs) const = default;

    using pixelType = Block::pixelType;
    using Map = std::map<ChunkTile, pixelType>;

    static inline Map catCache;

    static ChunkTile fromSocket(FILE* dataStream) {
        u8 data[sizeof(ChunkTile)];
        recvStrict(dataStream, reinterpret_cast<char*>(&data), sizeof(ChunkTile));

        return {
            .vram_index = static_cast<u16>(data[1] + (data[0] & 0b11) * (1 << 8)),
            .flip_x = static_cast<bool>(data[0] & 0b100),
            .flip_y = static_cast<bool>(data[0] & 0b1000),
            .std_solidity = static_cast<bool>((data[0] >> 4) & 0b11),
            .alt_solidity = static_cast<bool>(data[0] >> 6),
        };
    }

private:
    void for_rows(const stdr::range auto& rows, pixelType& output) const {
        for (auto [row, outRow]: stdr::zip_view(rows, output)) {
            if (flip_x) {
                stdr::copy(stdr::reverse_view(row), outRow.begin());
            } else {
                stdr::copy(row, outRow.begin());
            }
        }
    };
    [[nodiscard]] pixelType build_bytes(const BlockMap& blocks, const TileSet& tile_set) const {
        auto block_pixels = blocks.getBytes(vram_index, tile_set);
        pixelType result{};

        if (flip_y) {
            for_rows(stdr::reverse_view(block_pixels), result);
        } else {
            for_rows(block_pixels, result);
        }
        return result;
    }
public:
    [[nodiscard]] pixelType getBytes(const BlockMap& blocks, const TileSet& tile_set) const {
        if (const auto bb = catCache.find(*this); bb != std::end(catCache)) {
            return bb->second;
        }
        return (catCache[*this] = build_bytes(blocks, tile_set));
    }
};

struct Chunk {
    std::array<std::array<ChunkTile, 0x8>, 0x8> parts;

    static constexpr auto WIDTH = 128;
    using pixelType = std::array<std::array<u8, 128>, 128>;
    using rowType = std::array<ChunkTile::pixelType, 0x8>;

    static Chunk fromSocket(FILE* dataStream) {
        Chunk result{};
        for (auto &row: result.parts) {
            for (auto &cell: row) {
                cell = ChunkTile::fromSocket(dataStream);
            }
        }
        return result;
    }

private:
    [[nodiscard]] pixelType build_bytes(const BlockMap& blocks, const TileSet& tile_set) const {
        pixelType  result{};

        std::array<rowType, 0x8> blockBytes{};

        for (auto [byteRow, blockRow]:
            stdr::zip_view(blockBytes, parts)) {
            stdr::transform(blockRow, byteRow.begin(),
                [&](const ChunkTile& cat) { return cat.getBytes(blocks, tile_set); });
        }

        auto out = result[0].data();

        for (const auto chunkRow: blockBytes) {
            for (auto rows : zip_array(chunkRow)) {
                for (auto [i, row] : rows | stdv::enumerate) {
                    stdr::copy(row, out + 16 * i);
                }
                out += 128;
            }
        }

        return result;
    }
    friend ChunkMap;

};

struct ChunkMap {
    constexpr static auto SIZE = 0x8000;
    constexpr static auto COUNT = SIZE/sizeof(Chunk);
    std::array<Chunk, COUNT> chunks;
    static inline std::array<Chunk::pixelType, COUNT> chunk_pixels;
    static inline std::bitset<COUNT> computed_pixels;
    static ChunkMap fromSocket(FILE* dataStream) {
        ChunkMap result{};
        stdr::generate(result.chunks, [=] { return Chunk::fromSocket(dataStream); });
        return result;
    }

    [[nodiscard]] Chunk::pixelType getBytes(const long index, const BlockMap& blocks, const TileSet& tile_set) const {
        if (computed_pixels[index]) {
            return chunk_pixels[index];
        }
        chunk_pixels[index] = chunks[index].build_bytes(blocks, tile_set);
        computed_pixels.set(index);
        return chunk_pixels[index];
    }
};

struct SpriteMappingEntry {
    s8 y_pos;
    struct dimentions {
        u8 height : 2;
        u8 width : 2;
        auto operator<=>(const dimentions& ) const noexcept = default;
    } dim;
    BatCell art_tile;
    s16 x_pos;

    auto operator<=>(const SpriteMappingEntry&) const = default;

    using pixelType = std::array<std::array<u8, Tile::WIDTH*4>, Tile::WIDTH*4>;
    using Map = std::map<SpriteMappingEntry, s64>;
    static constexpr auto WIDTH = Tile::WIDTH * 4;

    static inline Map mappingIDs;
    static inline std::vector<pixelType> mappingPixels = std::vector<pixelType>(8);

    static SpriteMappingEntry fromBytes(const u8* data) {
        SpriteMappingEntry result {
            .y_pos = static_cast<s8>(data[0]),
            .dim = {
                .height = static_cast<u8>(data[1] & 0b11),
                .width = static_cast<u8>((data[1] & 0b1100) >> 2),
            },
            .art_tile = BatCell::fromBytes(&data[2]),
            .x_pos = static_cast<s16> (data[4] * 0x100 + data[5]),
        };

        return result;
    }

    static SpriteMappingEntry fromSocket(FILE* dataStream) {
        u8 data[sizeof(SpriteMappingEntry)];
        recvStrict(dataStream, reinterpret_cast<char*>(&data), sizeof(SpriteMappingEntry));
        return fromBytes(data);
    }
private:
    void for_rows(const stdr::range auto& rows, stdr::range auto& output, int tileX, const auto& map) const {
        for (auto [row, outRow]: stdr::zip_view(rows, output)) {
            const auto pixelStart = outRow.begin() + Tile::WIDTH * tileX;
            if (art_tile.flip_x) {
                stdr::transform(stdr::reverse_view(row), pixelStart, map);
            } else {
                stdr::transform(row, pixelStart, map);
            }
        }
    };
    [[nodiscard]] pixelType build_bytes(const TileSet& tile_set) const {
        auto map_palette = [this] (const u8 pixel) -> u8 {
            if (pixel == 0)
                return 0;
            int color = pixel + 0x10 * art_tile.palette;
            if (art_tile.priority)
                color |= 1<<7;
            return color;
        };

        std::array<std::array<s16, 4>, 4> art_indexes{};

        s16 start = static_cast<s16>(art_tile.vram_index);

        for (int i = 0; i <= dim.width; i++) {
            for (int j = 0; j <= dim.height; j++) {
                const int jj = art_tile.flip_y ? dim.height - j : j;
                const int ii = art_tile.flip_x ? dim.width - i : i;
                art_indexes[jj][ii] = start++;
            }
        }

        pixelType result{};

        for (int tileY = 0; tileY <= dim.height; tileY++) {
            for (int tileX = 0; tileX <= dim.width; tileX++) {
                auto [pixels] = tile_set.tiles[art_indexes[tileY][tileX]];
                const auto rowStart = result.begin() + Tile::WIDTH * tileY;
                auto partialResult = stdr::subrange(rowStart, rowStart+Tile::WIDTH);
                if (art_tile.flip_y) {
                    for_rows(stdr::reverse_view(pixels), partialResult, tileX, map_palette);
                } else {
                    for_rows(pixels, partialResult, tileX, map_palette);
                }
            }
        }
        return result;
    }

    friend RingMappingEntry;
public:
    [[nodiscard]] SpriteMappingEntry withBase(const BatCell& base) const {
        auto cpy = *this;
        cpy.art_tile = cpy.art_tile + base;
        return cpy;
    }

    [[nodiscard]] pixelType getBytes(const BatCell& base, TileSet& tile_set) const {
        auto mod = withBase(base);
        if (const auto bb = mappingIDs.find(mod); bb != std::end(mappingIDs)) {
            return mappingPixels[bb->second];
        }
        const s64 new_index = static_cast<s64>(mappingPixels.size());
        const auto bytes = mod.build_bytes(tile_set);
        mappingIDs[mod] = new_index;
        mappingPixels.push_back(bytes);
        return bytes;
    }
};

struct SpriteMappingFrame {
    s16 size{};
    std::vector<SpriteMappingEntry> entries;

    constexpr static SpriteMappingFrame fromBytes(const u8* data)  {
        s16 size;
        std::memcpy(&size, data, sizeof(size));
        data += sizeof(size);
        SpriteMappingFrame result{};
        result.size = size;
        result.entries.reserve(size);
        for (int i = 0; i < size; ++i) {
            result.entries.push_back(SpriteMappingEntry::fromBytes(data));
            data += sizeof(SpriteMappingEntry);
        }
        return result;
    }

    static SpriteMappingFrame fromSocket(FILE* dataStream) {
        s16 size;
        recvStrict(dataStream, reinterpret_cast<char*>(&size), sizeof(size));
        SpriteMappingFrame result{};
        result.size = size;
        result.entries.reserve(size);
        for (int i = 0; i < size; ++i) {
            result.entries.push_back(SpriteMappingEntry::fromSocket(dataStream));
        }
        return result;
    }
};

#pragma pack(push, 1)
struct ObjectTableEntry {
    struct subchild {
        s16 x_pos;
        s16 y_pos;
        u8 blank;
        u8 map_frame;
    };

    u32 routine_address;

    struct {
        bool horizontal_mirror : 1;
        bool vertical_mirror : 1;
        bool use_level_coordinates : 1;
        bool p1_multi_flag: 1;
        bool p2_multi_flag: 1;
        bool static_mapping : 1;
        bool compound_sprite : 1;
        bool on_screen : 1;
    } render_flags;

    u8 routine;
    u8 height_pixels;
    u8 width_pixels;
    u16 priority;
    BatCell art_tile;
    u32 mappings;

    s16 x_pos;
    u16 sub_x_coordinate;
    s16 y_pos;
    union {
        // Regular Object
        struct {
            u16 sub_y_coordinate;
            u16 x_vel;
            u16 y_vel;
            u16 ground_vel;
            u8 y_radius;
            u8 x_radius;
            u8 anim;
            u8 prev_anim;
            u8 mapping_frame;
            u8 anim_frame;
            u8 anim_frame_timer;
            u8 double_jump_property;

            u8 angle;
            u8 flip_angle;
            struct {
                u8 size: 6;
                u8 type : 2;
            } collision_flags;
            u8 collision_property;

            struct {
                bool x_right : 1;
                bool y_down : 1;
                bool rss: 1;
                bool sonic_standing: 1;
                bool tails_standing: 1;
                bool sonic_pushing: 1;
                bool tails_pushing: 1;
                bool to_be_deleted: 1;
            } status;

            union {
                struct {
                    u8 blank : 3;
                    bool bounce_off: 1;
                    bool negate_fire: 1;
                    bool negate_lightning: 1;
                    bool negate_bubble: 1;
                    u8 blank2: 1;
                } shield_reaction;
                struct {
                    bool has_shield : 1;
                    bool is_invincible: 1;
                    bool has_speed_shoes: 1;
                    bool unused: 1;
                    bool fire_shield: 1;
                    bool lightning_shield: 1;
                    bool bubble_shield: 1;
                    bool infinite_inertia: 1;
                } status_secondary;
            };

            union {
                u8 subtype;
                u8 air_left;
            };
            union {
                u8 bird_valid_target;
                u8 currently_interacting;
                u8 flip_type;
            };

            u8 object_control;
            u8 double_jump_flag;
            u8 flips_remaining;
            u8 flip_speed;
            u16 move_lock;
            u8 invulnerability_timer;
            u8 invincibility_timer;
            u8 speed_shoes_timer;
            u8 status_tertiary;
            u8 character_id;
            u8 scroll_delay_counter;
            u8 next_tilt;
            union {
                u8 tilt;
                u8 ros_bit;
            };
            union {
                u16 ros_addr;
                struct {
                    union {
                        u8 routine_secondary;
                        u8 spin_dash_flag;
                    };
                    u8 stick_to_convex;
                };
            };

            u16 spin_dash_counter;
            union {
                u16 vram_art;
                struct {
                    u8 unused;
                    u8 jumping;
                };
            };

            union {
                u16 interact;
                u16 parent;
                struct {
                    u8 child_dy;
                    u8 child_dx;
                };
            };
            u8 default_y_radius;
            u8 default_x_radius;

            union {
                u16 parent3;
                struct {
                    u8 lrb_solid_bit;
                    u8 top_solid_bit;
                };
            };
            union {
                u16 parent2;
                u16 respawn_addr;
            };
        };
        // Compound Object
        struct {
            u16 mainspr_childsprites;
            std::array<subchild, 8> children;
            u16 blank;
        };
    };

    static ObjectTableEntry fromSocket(FILE* dataStream) {
        ObjectTableEntry result{};
        recvStrict(dataStream, reinterpret_cast<char*>(&result), sizeof(result));

        result.routine_address = std::byteswap(result.routine_address);
        result.priority = std::byteswap(result.priority);
        result.mappings = std::byteswap(result.mappings);
        result.x_pos = std::byteswap(result.x_pos);
        result.sub_x_coordinate = std::byteswap(result.sub_x_coordinate);
        result.y_pos = std::byteswap(result.y_pos);

        result.art_tile = BatCell::fromBytes(reinterpret_cast<u8*>(&result.art_tile));

        if (result.render_flags.compound_sprite) {
            result.mainspr_childsprites = std::byteswap(result.mainspr_childsprites);
            for (auto& c : result.children) {
                c.x_pos = std::byteswap(c.x_pos);
                c.y_pos = std::byteswap(c.y_pos);
            }
        } else {
            result.sub_y_coordinate = std::byteswap(result.sub_y_coordinate);
            result.x_vel = std::byteswap(result.x_vel);
            result.y_vel = std::byteswap(result.y_vel);
            result.ground_vel = std::byteswap(result.ground_vel);
            result.move_lock = std::byteswap(result.move_lock);
            result.ros_addr = std::byteswap(result.ros_addr);
            result.spin_dash_counter = std::byteswap(result.spin_dash_counter);
            result.vram_art = std::byteswap(result.vram_art);
            result.interact = std::byteswap(result.sub_y_coordinate);
            result.parent3 = std::byteswap(result.sub_y_coordinate);
            result.parent2 = std::byteswap(result.sub_y_coordinate);
        }

        return result;
    }
};
#pragma pack(pop)

struct Sprite {
    ObjectTableEntry object{};
    SpriteMappingFrame frame{};
    std::vector<SpriteMappingFrame> children{};

    static Sprite fromSocket(FILE* dataStream) {
        Sprite result{};
        result.object = ObjectTableEntry::fromSocket(dataStream);
        result.frame = SpriteMappingFrame::fromSocket(dataStream);

        if (result.object.render_flags.compound_sprite) {
            u16 count;
            recvStrict(dataStream, reinterpret_cast<char*>(&count), sizeof(count));
            result.children.resize(result.object.mainspr_childsprites);
            stdr::generate(result.children, [=]{ return SpriteMappingFrame::fromSocket(dataStream); });
        }
        return result;
    }


};

struct RingMappingEntry {
    s8 y_pos;
    struct dimentions {
        u8 height : 2;
        u8 width : 2;
        auto operator<=>(const dimentions& ) const noexcept = default;
    } dim;
    BatCell art_tile;
    s16 x_pos;

    using pixelType = SpriteMappingEntry::pixelType;

    static RingMappingEntry fromSocket(FILE* dataStream) {
        u8 data[8];
        recvStrict(dataStream, reinterpret_cast<char*>(&data), 8);

        RingMappingEntry result {
            .y_pos = static_cast<s8>(data[1]),
            .dim = {
                .height = static_cast<u8>(data[3] & 0b11),
                .width = static_cast<u8>((data[3] & 0b1100) >> 2),
            },
            .art_tile = BatCell::fromBytes(&data[4]),
            .x_pos = static_cast<s16> (data[6] * 0x100 + data[7]),
        };

        return result;
    }

    void for_rows(const stdr::range auto& rows, stdr::range auto& output, int tileX, const auto& map) const {
        for (auto [row, outRow]: stdr::zip_view(rows, output)) {
            const auto pixelStart = outRow.begin() + Tile::WIDTH * tileX;
            if (art_tile.flip_x) {
                stdr::transform(stdr::reverse_view(row), pixelStart, map);
            } else {
                stdr::transform(row, pixelStart, map);
            }
        }
    };
    [[nodiscard]] pixelType build_bytes(const TileSet& tile_set) const {
        const SpriteMappingEntry tmp{
            .y_pos = y_pos,
            .dim = {
                .height = static_cast<u8>(dim.height),
                .width = static_cast<u8>(dim.width),
            },
            .art_tile = art_tile,
            .x_pos = x_pos,
        };

        return tmp.build_bytes(tile_set);
    }
};

struct ringStatus {
    s8 timer;
    s8 frame;
};

struct ringLocation {
    s16 x_pos;
    s16 y_pos;
};

struct RingData {
    static constexpr auto COUNT = (0xEB00 - 0xE702)/2;
    std::array<ringStatus, COUNT> status{};
private:
    std::array<ringLocation, COUNT> locations{};
public:
    std::array<RingMappingEntry, 8> ring_mappings{};
    int ringCount{};
    u8 ringAnimFrame{};

    inline static std::set<s32> tileDependencies = {};

    auto locationSubrange() {
        return stdr::subrange(locations.begin(), locations.begin() + ringCount);
    }

    void set_location_from_socket(FILE* dataStream) {
        auto out = this->locations.begin();
        while (true) {
            s16 b1;
            s16 b2;
            recvStrict(dataStream, reinterpret_cast<char*>(&b1), sizeof(b1));
            if (b1 == -1) {
                *out = {-1, -1};
                ringCount = static_cast<int>(out - this->locations.begin());
                break;
            }
            recvStrict(dataStream, reinterpret_cast<char*>(&b2), sizeof(b2));
            *out++ = {b1, b2};
        }
    }

    void set_status_from_socket(FILE* dataStream) {
        recvStrict(dataStream, reinterpret_cast<char*>(&this->ringAnimFrame), 1);
        recvStrict(dataStream, reinterpret_cast<char*>(&this->status), sizeof(status));
    }

    void set_mappings_from_socket(FILE* dataStream) {
        stdr::generate(this->ring_mappings,
            [=] { return RingMappingEntry::fromSocket(dataStream); });
        calculateTileDependencies();
    }

    void calculateTileDependencies() const {
        for (const auto& m : ring_mappings) {
            auto start = m.art_tile.vram_index;
            for (int j = 0; j <= m.dim.height; ++j) {
                for (int i = 0; i <= m.dim.width; ++i) {
                    tileDependencies.insert(start++);
                }
            }
        }
    }

    void setBytes(TileSet& tile_set) const {
        for (int i = 0; i < 8; ++i) {
            const auto bytes = ring_mappings[i].build_bytes(tile_set);
            SpriteMappingEntry::mappingPixels[i] = bytes;
        }
    }
};

struct RenderingData {
    inline static std::array<u8, 0x10000> currentVRAM;
    Palette palette;
    Palette water_palette;
    int water_line = 0;
    TileSet tileset;
    BlockMap blocks;
    ChunkMap chunks;
    std::pair<s16, s16> screen_position_A;
    std::pair<s16, s16> screen_position_B;

    std::vector<std::vector<u8>> level_chunks;
    std::vector<std::vector<u8>> background_chunks;
    std::vector<Sprite> sprites;

    int scroll_x;
    int scroll_y;

    RingData ring_data;
    bool has_water;
    u8 backgroundColor;
    u16 vertical_loop;
    s16 screen_min_x;
    s16 screen_min_y;
    s16 screen_max_x;
    s16 screen_max_y;

    // ============================================
    // Used to track which chunks use each tile
    // So that what tiles are partially updated,
    // Only *some* of the chunks need to be rebuilt
    // ============================================
    std::vector<std::set<int>> tileChunkDependencies;
    std::set<int> newly_updated_tiles;
    void setChunkDependecies() {
        tileChunkDependencies.clear();
        tileChunkDependencies.resize(tileset.tiles.size());
        for (auto [i, chunk] : stdv::enumerate(chunks.chunks)) {
            for (const auto& block : chunk.parts | stdv::join) {
                for (const auto& tile : blocks.blocks[block.vram_index].cells | stdv::join) {
                    tileChunkDependencies[tile.vram_index].insert(static_cast<int>(i));
                }
            }
        }
    }

    std::set<int> chunksToUpdate() {
        std::set<int> result;
        for (auto c :
            newly_updated_tiles |
            stdv::transform([this](const int i) { return tileChunkDependencies[i]; })) {
            result.merge(c);
            }
        return result;
    };


    u16 currentZoneAct;
    u16 bgEvent;
    u16 fgEvent;
    std::array<u16, 9> bgEventVars;
    std::array<u16, 6> fgEventVars;
    std::array<u16, 4> unknownEventVars;
    u16 lbzDeathEggEvent;
    u16 gamePaused;
    u16 lagFrames;
    u16 frameCount;
    u16 shakeFlag;
    s16 shakeOffset;
    u8 gameMode;

    static void clearCaches() {
        BlockMap::computed_pixels.reset();
        ChunkMap::computed_pixels.reset();
        ChunkTile::catCache.clear();
        BatCell::batCache.clear();
        SpriteMappingEntry::mappingIDs.clear();
        SpriteMappingEntry::mappingPixels.resize(8);
    }

    [[nodiscard]] u32 getCurrentActFGEvent() const {
        return (currentZoneAct << 16) + fgEvent / 4;
    }

    [[nodiscard]] u32 getCurrentActBGEvent() const {
        return (currentZoneAct << 16) + bgEvent / 4;
    }
};

// static_assert(sizeof(Color3Bit) == 2);
// static_assert(std::has_unique_object_representations_v<Color3Bit>);
static_assert(sizeof(Tile::tilerow) == 4);
static_assert(std::has_unique_object_representations_v<Tile::tilerow>);
static_assert(sizeof(Tile) == 0x8*0x8);
static_assert(std::has_unique_object_representations_v<Tile>);
static_assert(sizeof(BatCell) == 2);
static_assert(std::has_unique_object_representations_v<BatCell>);
static_assert(sizeof(Block) == 8);
static_assert(std::has_unique_object_representations_v<Block>);
static_assert(sizeof(ChunkTile) == 2);
static_assert(std::has_unique_object_representations_v<ChunkTile>);
static_assert(sizeof(Chunk) == 128);
static_assert(std::has_unique_object_representations_v<Chunk>);
static_assert(sizeof(ChunkMap) == 0x8000);
static_assert(std::has_unique_object_representations_v<ChunkMap>);

static_assert(sizeof(SpriteMappingEntry) == 6);

static_assert(sizeof(RingData::status) == (0xEB00 - 0xE702));
static_assert(std::has_unique_object_representations_v<decltype(RingData::status)>);
// static_assert(sizeof(RingData::locations) == 2*(0xEB00 - 0xE702));
// static_assert(std::has_unique_object_representations_v<decltype(RingData::locations)>);

static_assert(sizeof(ObjectTableEntry) == 0x4A);
static_assert(std::has_unique_object_representations_v<ObjectTableEntry>);
// ; universally followed object conventions:
static_assert(offsetof(ObjectTableEntry, render_flags) == 4);
static_assert(offsetof(ObjectTableEntry, height_pixels) == 6);
static_assert(offsetof(ObjectTableEntry, width_pixels) == 7);
static_assert(offsetof(ObjectTableEntry, priority) == 8);
static_assert(offsetof(ObjectTableEntry, art_tile) == 0xA);
static_assert(offsetof(ObjectTableEntry, mappings) == 0xC);
static_assert(offsetof(ObjectTableEntry, x_pos) == 0x10);
static_assert(offsetof(ObjectTableEntry, y_pos) == 0x14);
static_assert(offsetof(ObjectTableEntry, mapping_frame) == 0x22);
//; conventions followed by most objects:
static_assert(offsetof(ObjectTableEntry, routine) == 0x5);
static_assert(offsetof(ObjectTableEntry, x_vel) == 0x18);
static_assert(offsetof(ObjectTableEntry, y_vel) == 0x1A);
static_assert(offsetof(ObjectTableEntry, y_radius) == 0x1E);
static_assert(offsetof(ObjectTableEntry, x_radius) == 0x1F);
static_assert(offsetof(ObjectTableEntry, anim) == 0x20);
static_assert(offsetof(ObjectTableEntry, prev_anim) == 0x21);
static_assert(offsetof(ObjectTableEntry, anim_frame) == 0x23);
static_assert(offsetof(ObjectTableEntry, anim_frame_timer) == 0x24);
static_assert(offsetof(ObjectTableEntry, angle) == 0x26);
static_assert(offsetof(ObjectTableEntry, status) == 0x2A);
//; conventions followed by many objects but not Sonic/Tails/Knuckles:
static_assert(offsetof(ObjectTableEntry, collision_flags) == 0x28);
static_assert(offsetof(ObjectTableEntry, collision_property) == 0x29);
static_assert(offsetof(ObjectTableEntry, shield_reaction) == 0x2B);
static_assert(offsetof(ObjectTableEntry, subtype) == 0x2C);
static_assert(offsetof(ObjectTableEntry, ros_bit) == 0x3B);
static_assert(offsetof(ObjectTableEntry, ros_addr) == 0x3C);
static_assert(offsetof(ObjectTableEntry, routine_secondary) == 0x3C);
static_assert(offsetof(ObjectTableEntry, vram_art) == 0x40);
static_assert(offsetof(ObjectTableEntry, parent) == 0x42);
static_assert(offsetof(ObjectTableEntry, child_dx) == 0x43); // Union with word requires bit flip
static_assert(offsetof(ObjectTableEntry, child_dy) == 0x42);
static_assert(offsetof(ObjectTableEntry, parent3) == 0x46);
static_assert(offsetof(ObjectTableEntry, parent2) == 0x48);
static_assert(offsetof(ObjectTableEntry, respawn_addr) == 0x48);
//; conventions followed by many objects but not Sonic/Tails/Knuckles:
static_assert(offsetof(ObjectTableEntry, ground_vel) == 0x1C);
static_assert(offsetof(ObjectTableEntry, double_jump_property) == 0x25);
static_assert(offsetof(ObjectTableEntry, flip_angle) == 0x27);
static_assert(offsetof(ObjectTableEntry, status_secondary) == 0x2B);
static_assert(offsetof(ObjectTableEntry, air_left) == 0x2C);
static_assert(offsetof(ObjectTableEntry, flip_type) == 0x2D);
static_assert(offsetof(ObjectTableEntry, object_control) == 0x2E);
static_assert(offsetof(ObjectTableEntry, flips_remaining) == 0x30);
static_assert(offsetof(ObjectTableEntry, flip_speed) == 0x31);
static_assert(offsetof(ObjectTableEntry, move_lock) == 0x32);
static_assert(offsetof(ObjectTableEntry, invulnerability_timer) == 0x34);
static_assert(offsetof(ObjectTableEntry, invincibility_timer) == 0x35);
static_assert(offsetof(ObjectTableEntry, speed_shoes_timer) == 0x36);
static_assert(offsetof(ObjectTableEntry, status_tertiary) == 0x37);
static_assert(offsetof(ObjectTableEntry, character_id) == 0x38);
static_assert(offsetof(ObjectTableEntry, scroll_delay_counter) == 0x39);
static_assert(offsetof(ObjectTableEntry, next_tilt) == 0x3A);
static_assert(offsetof(ObjectTableEntry, tilt) == 0x3B);
static_assert(offsetof(ObjectTableEntry, stick_to_convex) == 0x3D); // Union with word requires bit flip
static_assert(offsetof(ObjectTableEntry, spin_dash_flag) == 0x3C);
static_assert(offsetof(ObjectTableEntry, spin_dash_counter) == 0x3E);
static_assert(offsetof(ObjectTableEntry, jumping) == 0x41); // Union with word requires bit flip
static_assert(offsetof(ObjectTableEntry, interact) == 0x42);
static_assert(offsetof(ObjectTableEntry, default_y_radius) == 0x44);
static_assert(offsetof(ObjectTableEntry, default_x_radius) == 0x45);
static_assert(offsetof(ObjectTableEntry, top_solid_bit) == 0x47); // Union with word requires bit flip
static_assert(offsetof(ObjectTableEntry, lrb_solid_bit) == 0x46);
//; when childsprites are activated (i.e. bit #6 of render_flags set)
static_assert(offsetof(ObjectTableEntry, mainspr_childsprites) == 0x16);



#endif //SONIC3ATLUS_STRUCTS_H
