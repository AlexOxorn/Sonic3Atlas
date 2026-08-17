//
// Created by alexoxorn on 8/4/26.
//

#ifndef COMMON_H
#define COMMON_H

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <print>
#include <ranges>
#include <sstream>
#include <variant>
#include <cmath>

// #include "consts.h"


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

namespace stdr = std::ranges;
namespace stdv = std::ranges::views;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8 = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

namespace stdfs = std::filesystem;

#define MOD(x, y) (((x) % (y) + (y)) % (y))

template <typename M>
struct trueMod {
    M baseMod;

    template <typename L, typename R>
    auto operator()(L l, R r) const {
        return baseMod(baseMod(l, r) + r, r);
    }
};

inline constexpr trueMod<float(*)(float, float)> trueFMod{std::fmod};
inline constexpr trueMod trueIMod{std::modulus{}};

struct Options {
    struct FileInStream {
        std::string path;
    };
    struct SocketInStream {
        std::string address;
        int port;
    };
    using InputVariant = std::variant<FileInStream, SocketInStream>;
    enum class SpecialResolution {
        FullHeight
    };
    using ResolutionType = std::variant<std::pair<int, int>, SpecialResolution>;
    enum class OOB {
        NONE,
        CLAMP,
        LOOP,
    };
    struct FFMpeg {
        std::string directory;
        std::string filename;
        bool withAudio;
        std::string audioInput;
        std::string video_encoder;
        std::string other_options;
        bool active;
    };

    InputVariant inStream;
    FFMpeg ffmpeg;
    ResolutionType internalResolution;
    std::pair<int, int> outputResolution;
    OOB horizontal_oob = OOB::NONE;
    OOB vertical_oob = OOB::NONE;


    [[nodiscard]] std::string serialize() const {
        std::stringstream ss;
        std::println(ss, "input_type:{}", std::holds_alternative<FileInStream>(inStream) ? "file" : "socket");
        if (std::holds_alternative<FileInStream>(inStream)) {
            std::println(ss, "input_path:{}", std::get<FileInStream>(inStream).path.c_str());
        } else if (std::holds_alternative<SocketInStream>(inStream)) {
            std::println(ss, "socket_addr:{}", std::get<SocketInStream>(inStream).address);
            std::println(ss, "socket_port:{}", std::get<SocketInStream>(inStream).port);
        }

        std::println(ss, "ffmpeg-active:{}", static_cast<int>(ffmpeg.active));
        std::println(ss, "ffmpeg-directory:{}", ffmpeg.directory);
        std::println(ss, "ffmpeg-filename:{}", ffmpeg.filename);
        std::println(ss, "ffmpeg-hasAudio:{}", static_cast<int>(ffmpeg.withAudio));
        std::println(ss, "ffmpeg-audioInput:{}", ffmpeg.audioInput);
        std::println(ss, "ffmpeg-video_encoder:{}", ffmpeg.video_encoder);
        std::println(ss, "ffmpeg-other_options:{}", ffmpeg.other_options);

        if (std::holds_alternative<SpecialResolution>(internalResolution)) {
            std::println(ss, "dynamic_resolution:{}", std::to_underlying(std::get<SpecialResolution>(internalResolution)));
        } else {
            auto res = std::get<std::pair<int, int>>(internalResolution);
            std::println(ss, "internal_resolution:{}x{}", res.first, res.second);
        }
        std::println(ss, "output_resolution:{}x{}",  outputResolution.first, outputResolution.second);

        std::println(ss, "horizontal_oob:{}", std::to_underlying(horizontal_oob));
        std::println(ss, "vertical_oob:{}", std::to_underlying(vertical_oob));

        return ss.str();
    }

    void deserialize(const std::string& data) {
        std::stringstream ss(data);
        std::string line;
        for (const auto& line : data | stdv::split('\n')) {
            if (line.begin() == line.end())
                return;
            auto mid = stdr::find(line, ':');
            if (mid == line.end()) {
                std::string manif{line.begin(), line.end()};
                fprintf(stderr, "Failed to Parse %s\n", manif.c_str());
                return;
            }
            std::string opt{line.begin(), mid};
            std::string val{mid+1, line.end()};

            if (opt == "input_type") {
                inStream = val == "file" ? InputVariant{FileInStream{}} : InputVariant{SocketInStream{}};
            }
            else if (opt == "input_path") {
                std::get<FileInStream>(inStream).path = val;
            }
            else if (opt == "socket_addr") {
                std::get<SocketInStream>(inStream).address = val;
            }
            else if (opt == "socket_port") {
                std::get<SocketInStream>(inStream).port = static_cast<int>(strtol(val.c_str(), nullptr, 10));
            }
            else if (opt == "ffmpeg-active") {
                ffmpeg.active = val == "1";
            }
            else if (opt == "ffmpeg-directory") {
                ffmpeg.directory = val;
            }
            else if (opt == "ffmpeg-filename") {
                ffmpeg.filename = val;
            }
            else if (opt == "ffmpeg-hasAudio") {
                ffmpeg.withAudio = val != "0";
            }
            else if (opt == "ffmpeg-audioInput") {
                ffmpeg.audioInput = val;
            }
            else if (opt == "ffmpeg-video_encoder") {
                ffmpeg.video_encoder = val;
            }
            else if (opt == "ffmpeg-other_options") {
                ffmpeg.other_options = val;
            }
            else if (opt == "dynamic_resolution") {
                auto int_val = static_cast<int>(strtol(val.c_str(), nullptr, 10));
                internalResolution = static_cast<SpecialResolution>(int_val);
            }
            else if (opt == "internal_resolution") {
                std::pair<int, int> in_res;
                std::sscanf(val.c_str(), "%dx%d", &in_res.first, &in_res.second);
                internalResolution = in_res;
            }
            else if (opt == "output_resolution") {
                std::sscanf(val.c_str(), "%dx%d", &outputResolution.first, &outputResolution.second);
            }
            else if (opt == "horizontal_oob") {
                auto int_val = static_cast<int>(strtol(val.c_str(), nullptr, 10));
                horizontal_oob = static_cast<OOB>(int_val);
            }
            else if (opt == "vertical_oob") {
                auto int_val = static_cast<int>(strtol(val.c_str(), nullptr, 10));
                vertical_oob = static_cast<OOB>(int_val);
            } else {
                fprintf(stderr, "Failed to Parse %s:%s\n", opt.c_str(), val.c_str());
                return;
            }
        }
    }

    void setDefaults() {
        inStream = FileInStream{};
        ffmpeg.active = false;
        ffmpeg.withAudio = false;
        ffmpeg.filename = "";
        ffmpeg.directory = "";
        ffmpeg.audioInput = "";
        ffmpeg.video_encoder = "h264_nvenc";
        ffmpeg.other_options = "-cq 6 -pix_fmt yuv420p ";
        internalResolution = std::pair R_4K;
        outputResolution = std::pair R_2K;
        horizontal_oob = OOB::NONE;
        vertical_oob = OOB::NONE;
    }

    [[nodiscard]] size_t getStreamType() const {
        return inStream.index();
    }
};

inline Options config;

inline std::string slurp(const std::string& path) {
    auto fp = std::fopen(path.c_str(), "rb");

    if (fp == nullptr) {
        throw std::runtime_error(path + ": " + std::strerror(errno));
    }

    std::fseek(fp, 0u, SEEK_END);
    const auto size = std::ftell(fp);
    std::fseek(fp, 0u, SEEK_SET);

    std::string s;
    s.resize(size);

    const auto read = std::fread(&s[0], 1u, size, fp);
    std::fclose(fp);

    assert(read == size);

    return s;
}

#endif
