//
// Created by alexoxorn on 8/15/26.
//

#include "main_menu.h"
#include "common.h"

// Dear ImGui: standalone example application for SDL3 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context
// creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <concepts>
#include <cstdio>
#include <map>
#include <ranges>

#include "common.h"
#include "consts.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

namespace Menu {
    static SDL_Window* window;
    static SDL_GLContext gl_context;
    static ImGuiIO* io = nullptr;
    static int in_type_selected; // Here we store our selection data as an index.
    static int internal_type_selected;
    static int output_type_selected;

    static const char* resolutions[] = {"HD", "2K", "4K", "8K", "Genesis", "Full Height"};
    static const char* outOfBounds[] = {"Render Nothing", "Prevent Scrolling", "Simulate Loopback/Sewer"};
    static const char* followNames[] = {"Player 1", "Camera"};
    constexpr int maxOutputIndex = 5;
    static Options::ResolutionType indexToResolution[] = {
            Options::ResolutionType{std::pair R_HD},
            Options::ResolutionType{std::pair R_2K},
            Options::ResolutionType{std::pair R_4K},
            Options::ResolutionType{std::pair R_8K},
            Options::ResolutionType{GENESIS_RESOLUTION},
            Options::ResolutionType{Options::SpecialResolution::FullHeight}};
#define INDEX_TO_INDEX(x) (std::pair{indexToResolution[(x)], (x)})
    static std::map resolutionToIndex{
            INDEX_TO_INDEX(0),
            INDEX_TO_INDEX(1),
            INDEX_TO_INDEX(2),
            INDEX_TO_INDEX(3),
            INDEX_TO_INDEX(4),
            INDEX_TO_INDEX(5),
    };

    static void FileInputSelect(void* userdata, const char* const* filelist, int filter) {
        if (filelist == nullptr) {
            SDL_Log("File dialog error: %s", SDL_GetError());
        }
        else if (*filelist == nullptr) {
            SDL_Log("User canceled the selection.");
        }
        else {
            std::string& out = *static_cast<std::string*>(userdata);
            out = filelist[0];
        }
    }

    static void SelectFile(const std::string& dir, const std::span<const SDL_DialogFileFilter> filters,
                           std::string& out) {
        const char* directory;
        const stdfs::path dirpath = dir;
        if (dir.empty()) {
            directory = nullptr;
        }
        else {
            directory = dirpath.c_str();
        }
        SDL_SetHint(SDL_HINT_FILE_DIALOG_DRIVER, "zenity");
        SDL_ShowOpenFileDialog(FileInputSelect, &out, window, filters.data(), filters.size(), directory, false);
    }

    static void SelectFolder(const std::string& dir, std::string& out) {
        const char* directory;
        const stdfs::path dirpath = dir;
        if (dir.empty()) {
            directory = nullptr;
        }
        else {
            directory = dirpath.c_str();
        }
        SDL_SetHint(SDL_HINT_FILE_DIALOG_DRIVER, "zenity");
        SDL_ShowOpenFolderDialog(FileInputSelect, &out, window, directory, false);
    }

    SDL_AppResult INIT() {
        // Select GL version + let the backend select a GLSL version
        const char* glsl_version = nullptr;
#if defined(IMGUI_IMPL_OPENGL_ES2)
        // GL ES 2.0 + GLSL 100 (WebGL 1.0)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
        // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
        // GL 3.2 Core + generally GLSL 150
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
        // GL 3.0 + generally GLSL 130
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        const float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
        constexpr SDL_WindowFlags window_flags =
                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        window = SDL_CreateWindow(
                "Sonic 3 Atlas", static_cast<int>(1280 * main_scale), static_cast<int>(800 * main_scale), window_flags);
        if (window == nullptr) {
            printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
            return SDL_APP_FAILURE;
        }
        gl_context = SDL_GL_CreateContext(window);
        if (gl_context == nullptr) {
            printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        SDL_GL_MakeCurrent(window, gl_context);
        SDL_GL_SetSwapInterval(1); // Enable vsync
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_ShowWindow(window);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        io = &ImGui::GetIO();
        (void)*io;
        io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(main_scale);
        style.FontScaleDpi = main_scale;

        // Setup Platform/Renderer backends
        ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
        ImGui_ImplOpenGL3_Init(glsl_version);

        in_type_selected = baseConfig.getStreamType();
        internal_type_selected = resolutionToIndex[baseConfig.internalResolution];
        output_type_selected = resolutionToIndex[baseConfig.outputResolution];
        return SDL_APP_CONTINUE;
    }

    static void displayFile(std::string base) {
        static char* home = getenv("HOME");
        static size_t home_len = strlen(home);
        if (size_t pos = base.find(home); pos != std::string::npos) {
            base.replace(pos, home_len, "~");
        }
        ImGui::Text("%s", base.c_str());
    }

    SDL_AppResult EVENT(void* appstate, SDL_Event* event) {
        ImGui_ImplSDL3_ProcessEvent(event);
        if (event->type == SDL_EVENT_QUIT)
            return SDL_APP_FAILURE;
        if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event->window.windowID == SDL_GetWindowID(window))
            return SDL_APP_FAILURE;
        return SDL_APP_CONTINUE;
    }
    SDL_AppResult ITER(void* appstate) {
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            return SDL_APP_CONTINUE;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;


            ImGui::Begin("Input Type");
            const char* items[] = {"File", "Socket"};

            if (const char* combo_preview_value = items[in_type_selected];
                ImGui::BeginCombo("Input Type", combo_preview_value)) {
                for (int n = 0; n < IM_COUNTOF(items); n++) {
                    const bool is_selected = (in_type_selected == n);
                    if (ImGui::Selectable(items[n], is_selected)) {
                        in_type_selected = n;
                        switch (in_type_selected) {
                            case 0:
                                baseConfig.inStream = Options::FileInStream{};
                                break;
                            case 1:
                                baseConfig.inStream = Options::SocketInStream{};
                                break;
                            default:
                                std::unreachable();
                        }
                    }

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (in_type_selected == 0) {
                auto& [path] = std::get<0>(baseConfig.inStream);
                if (ImGui::Button("Select Log File")) {
                    // 2. Set up file filters
                    constexpr std::array filters = {
                            SDL_DialogFileFilter{.name = "Binary Files (*.bin)", .pattern = "bin"},
                            SDL_DialogFileFilter{.name = "All Files", .pattern = "*"}};
                    SelectFile(path, filters, path);
                };
                ImGui::SameLine();
                displayFile(path);
            }
            else if (in_type_selected == 1) {
                ImGui::Text("Not Yet Implemented");
            }

            ImGui::End();

            ImGui::Begin("Output");
            ImGui::Checkbox("Render To File", &baseConfig.ffmpeg.active);

            if (baseConfig.ffmpeg.active) {
                char bufferFilename[128];
                char bufferEncoder[128];
                char bufferOptions[1024];
                if (ImGui::Button("Select Output Directory")) {
                    SelectFolder(baseConfig.ffmpeg.directory, baseConfig.ffmpeg.directory);
                };
                ImGui::SameLine();

                strcpy(bufferFilename, baseConfig.ffmpeg.filename.c_str());
                if (ImGui::InputText("Filename", bufferFilename, 128, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    baseConfig.ffmpeg.filename = bufferFilename;
                }
                const auto filename =
                        stdfs::path(baseConfig.ffmpeg.directory) / stdfs::path(baseConfig.ffmpeg.filename);
                displayFile(filename.c_str());
                if (ImGui::Checkbox("Stream In Audio", &baseConfig.ffmpeg.withAudio)) {
                }
                if (baseConfig.ffmpeg.withAudio) {
                    if (ImGui::Button("Select Audio File: ")) {
                        constexpr std::array filters = {SDL_DialogFileFilter{.name = "Audio Files (.mp3, .wav, .ogg)",
                                                                             .pattern = "wav;mp3;ogg"},
                                                        SDL_DialogFileFilter{.name = "All Files", .pattern = "*"}};
                        SelectFile(baseConfig.ffmpeg.audioInput, filters, baseConfig.ffmpeg.audioInput);
                    };
                    ImGui::SameLine();
                    displayFile(baseConfig.ffmpeg.audioInput);
                }
                strcpy(bufferEncoder, baseConfig.ffmpeg.video_encoder.c_str());
                if (ImGui::InputText("Video Encoder", bufferEncoder, 128, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    baseConfig.ffmpeg.video_encoder = bufferEncoder;
                }
                strcpy(bufferOptions, baseConfig.ffmpeg.other_options.c_str());
                if (ImGui::InputText("Other Options", bufferOptions, 1024, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    baseConfig.ffmpeg.other_options = bufferOptions;
                }
            }

            ImGui::End();
            ImGui::Begin("Render Options");

            if (const char* combo_preview_value = resolutions[internal_type_selected];
                ImGui::BeginCombo("Internal Resolution", combo_preview_value)) {
                for (int n = 0; n < IM_COUNTOF(resolutions); n++) {
                    const bool is_selected = (internal_type_selected == n);
                    if (ImGui::Selectable(resolutions[n], is_selected)) {
                        internal_type_selected = n;
                        baseConfig.internalResolution = indexToResolution[internal_type_selected];
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (const char* combo_preview_value = resolutions[output_type_selected];
                ImGui::BeginCombo("Output Resolution", combo_preview_value)) {
                for (int n = 0; n < maxOutputIndex; n++) {
                    const bool is_selected = (output_type_selected == n);
                    if (ImGui::Selectable(resolutions[n], is_selected)) {
                        output_type_selected = n;
                        baseConfig.outputResolution =
                                std::get<std::pair<int, int>>(indexToResolution[output_type_selected]);
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            using oobType = std::underlying_type_t<Options::OOB>;
            auto* x_oob = reinterpret_cast<oobType*>(&baseConfig.horizontal_oob);
            auto* y_oob = reinterpret_cast<oobType*>(&baseConfig.vertical_oob);

            if (const char* combo_preview_value = outOfBounds[*x_oob];
                ImGui::BeginCombo("Horizontal Out of Bounds", combo_preview_value)) {
                for (int n = 0; n < IM_COUNTOF(outOfBounds); n++) {
                    const bool is_selected = (*x_oob == n);
                    if (ImGui::Selectable(outOfBounds[n], is_selected)) {
                        *x_oob = n;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (const char* combo_preview_value = outOfBounds[*y_oob];
                ImGui::BeginCombo("Vertical Out of Bounds", combo_preview_value)) {
                for (int n = 0; n < IM_COUNTOF(outOfBounds); n++) {
                    const bool is_selected = (*y_oob == n);
                    if (ImGui::Selectable(outOfBounds[n], is_selected)) {
                        *y_oob = n;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }


            using followType = std::underlying_type_t<Options::Follow>;
            auto* follow = reinterpret_cast<followType*>(&baseConfig.follow);
            if (const char* combo_preview_value = followNames[*follow];
                ImGui::BeginCombo("Camera Follows", combo_preview_value)) {
                for (int n = 0; n < IM_COUNTOF(followNames); n++) {
                    const bool is_selected = (*follow == n);
                    if (ImGui::Selectable(followNames[n], is_selected)) {
                        *follow = n;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::End();
        }

        ImGui::Begin("Start Encoding");
        if (ImGui::Button("Start")) {
            return SDL_APP_SUCCESS;
        }
        ImGui::End();

        ImGui::Render();
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
        return SDL_APP_CONTINUE;
    }

    SDL_AppResult CLEAR(void* appstate) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        SDL_GL_DestroyContext(gl_context);
        SDL_DestroyWindow(window);
        return SDL_APP_CONTINUE;
    }
} // namespace Menu
