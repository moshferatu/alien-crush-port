cmake_minimum_required(VERSION 3.30)
project(AlienCrushPort VERSION 0.1.0 LANGUAGES C CXX)
if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Alien Crush currently targets Windows x64 with MSVC.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(AC_BUILD_APP "Build the SDL desktop/cabinet host" ON)
option(AC_BUILD_REFERENCE "Build the pinned reference emulator and diagnostics" ON)
option(AC_REFERENCE_STATE_EXTENSION "Preserve upstream audio filter history in quick states" ON)
option(AC_REFERENCE_VIDEO_OBSERVER "Build optional read-only reference display and palette observation" ON)
option(AC_BUILD_TESTS "Build tests" ON)
option(AC_ENABLE_ROM_TESTS "Enable local proprietary-ROM tests" OFF)
option(AC_FETCH_SDL "Fetch pinned SDL" ON)
set(AC_ROM_PATH "" CACHE FILEPATH "Local Alien Crush ROM for tests")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
add_library(ac_core STATIC src/core/rom_image.cpp src/core/save_state.cpp
    src/core/atomic_file.cpp src/core/data_files.cpp src/core/files.cpp src/core/recording.cpp
    src/render/sharp_pixel_scaler.cpp src/input/plunger.cpp
    src/diagnostics/reference_snapshot.cpp src/diagnostics/display_trace.cpp src/render/pce_graphics.cpp
    src/assets/table_assets.cpp src/assets/actor_assets.cpp src/diagnostics/alien_state.cpp
    src/render/table_playfield.cpp src/gameplay/launcher_contact.cpp src/gameplay/cabinet_actors.cpp src/gameplay/cabinet_actors_lower.cpp src/gameplay/cabinet_actors_upper.cpp src/runtime/reference_launcher.cpp src/render/score_panel.cpp src/render/title_logo.cpp src/render/flipper_swing.cpp)
target_include_directories(ac_core PUBLIC include)
target_link_libraries(ac_core PRIVATE bcrypt)
ac_enable_strict_warnings(ac_core)
if(AC_BUILD_REFERENCE)
    include(BeetlePce)
    ac_provide_beetle_pce()
    add_library(ac_reference STATIC src/runtime/reference_runtime.cpp)
    target_link_libraries(ac_reference PUBLIC ac_core PRIVATE AlienCrush::BeetlePce)
    target_compile_definitions(ac_reference PRIVATE AC_BEETLE_REVISION="${AC_BEETLE_REVISION}")
    if(AC_REFERENCE_VIDEO_OBSERVER)
        target_compile_definitions(ac_reference PRIVATE AC_REFERENCE_VIDEO_OBSERVER=1)
    endif()
    if(AC_REFERENCE_STATE_EXTENSION)
        target_compile_definitions(ac_reference PRIVATE AC_REFERENCE_STATE_EXTENSION=1)
    endif()
    ac_enable_strict_warnings(ac_reference)
    add_executable(alien-crush-tools src/tools/main.cpp src/tools/video_capture.cpp)
    target_link_libraries(alien-crush-tools PRIVATE ac_reference)
    ac_enable_strict_warnings(alien-crush-tools)
endif()
if(AC_BUILD_APP)
    if(NOT AC_BUILD_REFERENCE)
        message(FATAL_ERROR "The native runtime is not implemented yet. Use AC_BUILD_APP=OFF for core-only builds.")
    endif()
    include(Dependencies)
    ac_provide_sdl3()
    add_executable(alien-crush src/host/main.cpp src/host/presentation.cpp src/host/cabinet.cpp src/host/backglass.cpp src/host/cabinet_config.cpp src/host/cabinet_input.cpp src/host/cabinet_plunger.cpp src/host/cabinet_nudge.cpp)
    target_link_libraries(alien-crush PRIVATE ac_reference SDL3::SDL3)
    ac_enable_strict_warnings(alien-crush)

    # The launcher's icon and heading (tools/make_launcher_art.cpp): the title
    # screen's logo, decoded by the port from the player's ROM, as a Windows
    # .ico and a C header. The launcher target below runs it when there is a
    # ROM to run it on.
    add_executable(ac_launcher_art tools/make_launcher_art.cpp)
    target_link_libraries(ac_launcher_art PRIVATE ac_core SDL3::SDL3)
    ac_enable_strict_warnings(ac_launcher_art)

    # The launcher (src/launcher/launcher.cpp): alien-crush.ini with
    # a face on it, and a Play button. It ships beside alien-crush.exe and
    # starts it. SDL as the game is, the game's own reading and writing of the
    # file, and stb_truetype to draw the system's font.
    add_executable(alien-crush-launcher WIN32 src/launcher/launcher.cpp
        src/host/cabinet_config.cpp src/host/cabinet_input.cpp src/host/cabinet_plunger.cpp src/host/cabinet_nudge.cpp)
    target_include_directories(alien-crush-launcher PRIVATE src/host third_party/stb)
    target_link_libraries(alien-crush-launcher PRIVATE ac_core SDL3::SDL3 comdlg32)
    ac_enable_strict_warnings(alien-crush-launcher)
    # Its icon and its heading are the title's logo, made from the ROM at
    # build time, since nothing of the ROM's is kept in the repository.
    # Without one the launcher builds without an icon, and its heading is
    # text.
    set(AC_LAUNCHER_ART_ROM "${CMAKE_CURRENT_SOURCE_DIR}/Alien Crush.pce"
        CACHE FILEPATH "The ROM the launcher's icon and heading are drawn from")
    if(EXISTS "${AC_LAUNCHER_ART_ROM}")
        set(ac_launcher_icon "${CMAKE_CURRENT_BINARY_DIR}/alien-crush-launcher.ico")
        set(ac_launcher_logo "${CMAKE_CURRENT_BINARY_DIR}/ac_launcher_logo.h")
        add_custom_command(
            OUTPUT "${ac_launcher_icon}" "${ac_launcher_logo}"
            COMMAND ac_launcher_art "${AC_LAUNCHER_ART_ROM}" "${ac_launcher_icon}" "${ac_launcher_logo}"
            DEPENDS ac_launcher_art "${AC_LAUNCHER_ART_ROM}"
            COMMENT "Drawing the launcher's icon and heading from the title's logo"
            VERBATIM
        )
        file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/alien-crush-launcher.rc"
             CONTENT "1 ICON \"${ac_launcher_icon}\"\n")
        target_sources(alien-crush-launcher PRIVATE
            "${ac_launcher_logo}" "${CMAKE_CURRENT_BINARY_DIR}/alien-crush-launcher.rc")
        set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/alien-crush-launcher.rc"
            PROPERTIES OBJECT_DEPENDS "${ac_launcher_icon}")
        target_include_directories(alien-crush-launcher PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
        target_compile_definitions(alien-crush-launcher PRIVATE AC_LAUNCHER_LOGO)
    else()
        message(STATUS "No ROM at ${AC_LAUNCHER_ART_ROM}: the launcher is built without its icon")
    endif()

    install(TARGETS alien-crush alien-crush-launcher RUNTIME DESTINATION . COMPONENT Runtime)
    install(FILES LICENSE README.md THIRD_PARTY_NOTICES.md DESTINATION . COMPONENT Runtime)
endif()
if(AC_BUILD_TESTS)
    enable_testing()
    add_executable(ac_core_tests tests/core_tests.cpp)
    target_link_libraries(ac_core_tests PRIVATE ac_core)
    ac_enable_strict_warnings(ac_core_tests)
    add_test(NAME core COMMAND ac_core_tests)
    add_executable(ac_launcher_contact_tests tests/launcher_contact_tests.cpp)
    target_link_libraries(ac_launcher_contact_tests PRIVATE ac_core)
    ac_enable_strict_warnings(ac_launcher_contact_tests)
    add_test(NAME launcher_contact COMMAND ac_launcher_contact_tests)
    set_tests_properties(launcher_contact PROPERTIES LABELS "core;rom_free;input")
    set_tests_properties(core PROPERTIES LABELS "core;rom_free")
    add_executable(ac_graphics_tests tests/graphics_tests.cpp)
    target_link_libraries(ac_graphics_tests PRIVATE ac_core)
    ac_enable_strict_warnings(ac_graphics_tests)
    add_test(NAME graphics COMMAND ac_graphics_tests)
    set_tests_properties(graphics PROPERTIES LABELS "core;rom_free;graphics")
    add_executable(ac_music_voice_overlay_model_tests tests/music_voice_overlay_tests.cpp)
    target_include_directories(ac_music_voice_overlay_model_tests PRIVATE src/runtime)
    ac_enable_strict_warnings(ac_music_voice_overlay_model_tests)
    add_test(NAME music_voice_overlay_model COMMAND ac_music_voice_overlay_model_tests)
    set_tests_properties(music_voice_overlay_model PROPERTIES LABELS "core;rom_free;audio")
    if(AC_ENABLE_ROM_TESTS AND AC_REFERENCE_VIDEO_OBSERVER)
        add_executable(ac_music_voice_overlay_tests tests/music_voice_overlay_tests.cpp)
        target_include_directories(ac_music_voice_overlay_tests PRIVATE src/runtime)
        target_compile_definitions(ac_music_voice_overlay_tests PRIVATE AC_OVERLAY_ROM_TESTS=1)
        target_link_libraries(ac_music_voice_overlay_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_music_voice_overlay_tests)
        add_test(NAME music_voice_overlay COMMAND ac_music_voice_overlay_tests "${AC_ROM_PATH}")
        set_tests_properties(music_voice_overlay PROPERTIES LABELS "rom_required;audio" TIMEOUT 120)
    endif()
    if(AC_BUILD_APP)
        add_executable(ac_backglass_tests tests/backglass_tests.cpp src/host/backglass.cpp)
        target_include_directories(ac_backglass_tests PRIVATE src/host)
        target_link_libraries(ac_backglass_tests PRIVATE ac_core)
        ac_enable_strict_warnings(ac_backglass_tests)
        add_test(NAME backglass COMMAND ac_backglass_tests "${CMAKE_BINARY_DIR}/backglass-test.bmp")
        set_tests_properties(backglass PROPERTIES LABELS "rom_free;host" TIMEOUT 10)
        add_executable(ac_cabinet_input_tests tests/cabinet_input_tests.cpp src/host/cabinet_config.cpp src/host/cabinet_input.cpp src/host/cabinet_plunger.cpp src/host/cabinet_nudge.cpp)
        target_include_directories(ac_cabinet_input_tests PRIVATE src/host)
        target_link_libraries(ac_cabinet_input_tests PRIVATE ac_core SDL3::SDL3)
        ac_enable_strict_warnings(ac_cabinet_input_tests)
        add_test(NAME cabinet_input COMMAND ac_cabinet_input_tests)
        if(AC_ENABLE_ROM_TESTS AND AC_REFERENCE_VIDEO_OBSERVER)
            add_executable(ac_motion_smoothing_tests tests/motion_smoothing_tests.cpp src/host/presentation.cpp)
            target_include_directories(ac_motion_smoothing_tests PRIVATE src/host)
            target_link_libraries(ac_motion_smoothing_tests PRIVATE ac_reference SDL3::SDL3)
            ac_enable_strict_warnings(ac_motion_smoothing_tests)
            add_test(NAME motion_smoothing COMMAND ac_motion_smoothing_tests "${AC_ROM_PATH}" "${CMAKE_BINARY_DIR}/smoothing-previews")
            set_tests_properties(motion_smoothing PROPERTIES TIMEOUT 90
                ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")
        endif()
        set_tests_properties(cabinet_input PROPERTIES LABELS "rom_free;host" TIMEOUT 30
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
        add_test(NAME input_probe_smoke COMMAND alien-crush --input-probe --frames 1)
        set_tests_properties(input_probe_smoke PROPERTIES LABELS "rom_free;host" TIMEOUT 10
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
    endif()
    if(AC_ENABLE_ROM_TESTS)
        if(NOT AC_BUILD_REFERENCE OR NOT EXISTS "${AC_ROM_PATH}")
            message(FATAL_ERROR "ROM tests require AC_BUILD_REFERENCE and an existing AC_ROM_PATH.")
        endif()
        add_test(NAME reference_replay COMMAND alien-crush-tools --verify-replay --rom "${AC_ROM_PATH}" --frames 1800)
        set_tests_properties(reference_replay PROPERTIES LABELS "rom_required" TIMEOUT 180)
        if(AC_REFERENCE_VIDEO_OBSERVER)
            add_test(NAME reference_video COMMAND alien-crush-tools --rom "${AC_ROM_PATH}" --frames 1800
                --require-display-match --verify-replay --compact --capture "${CMAKE_BINARY_DIR}/video-parity")
            set_tests_properties(reference_video PROPERTIES LABELS "rom_required;graphics" TIMEOUT 180)
        endif()
        add_test(NAME asset_discovery COMMAND alien-crush-tools --verify-discovery --rom "${AC_ROM_PATH}")
        add_executable(ac_title_logo_tests tests/title_logo_tests.cpp)
        add_executable(ac_attract_replay_tests tests/attract_replay_tests.cpp)
        add_executable(ac_bonus_replay_tests tests/bonus_replay_tests.cpp)
        add_executable(ac_bonus_stage_launch_tests tests/bonus_stage_launch_tests.cpp)
        target_link_libraries(ac_bonus_stage_launch_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_bonus_stage_launch_tests)
        add_test(NAME bonus_stage_launch COMMAND ac_bonus_stage_launch_tests "${AC_ROM_PATH}")
        set_tests_properties(bonus_stage_launch PROPERTIES LABELS "rom_required" TIMEOUT 120)
        add_executable(ac_score_start_tests tests/score_start_tests.cpp)
        target_link_libraries(ac_score_start_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_score_start_tests)
        add_test(NAME score_start COMMAND ac_score_start_tests "${AC_ROM_PATH}")
        set_tests_properties(score_start PROPERTIES LABELS "rom_required" TIMEOUT 120)
        add_executable(ac_ball_hold_tests tests/ball_hold_tests.cpp)
        target_link_libraries(ac_ball_hold_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_ball_hold_tests)
        add_test(NAME ball_hold COMMAND ac_ball_hold_tests "${AC_ROM_PATH}")
        set_tests_properties(ball_hold PROPERTIES LABELS "rom_required" TIMEOUT 120)
        add_executable(ac_ordinary_bonus_replay_tests tests/ordinary_bonus_replay_tests.cpp)
        add_executable(ac_game_over_replay_tests tests/game_over_replay_tests.cpp)
        add_executable(ac_match_replay_tests tests/match_replay_tests.cpp)
        add_executable(ac_high_score_replay_tests tests/high_score_replay_tests.cpp)
        add_executable(ac_name_entry_replay_tests tests/name_entry_replay_tests.cpp)
        add_executable(ac_high_score_save_tests tests/high_score_save_tests.cpp)
        target_include_directories(ac_high_score_save_tests PRIVATE src/host)
        target_link_libraries(ac_high_score_save_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_high_score_save_tests)
        add_test(NAME high_score_save COMMAND ac_high_score_save_tests "${AC_ROM_PATH}" "${CMAKE_BINARY_DIR}/high-score-save-test")
        set_tests_properties(high_score_save PROPERTIES TIMEOUT 30 LABELS "rom_required;host")
        target_link_libraries(ac_name_entry_replay_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_name_entry_replay_tests)
        add_test(NAME name_entry_replay COMMAND ac_name_entry_replay_tests "${AC_ROM_PATH}"
            "${CMAKE_BINARY_DIR}/bonus-smoke.clear.acrec" "${CMAKE_BINARY_DIR}/name-entry-smoke")
        set_tests_properties(name_entry_replay PROPERTIES LABELS "rom_required;graphics" TIMEOUT 60
            FIXTURES_REQUIRED bonus_recording FIXTURES_SETUP name_entry_recording)
        target_link_libraries(ac_high_score_replay_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_high_score_replay_tests)
        add_test(NAME high_score_replay COMMAND ac_high_score_replay_tests "${AC_ROM_PATH}" "${CMAKE_BINARY_DIR}/high-score-smoke")
        set_tests_properties(high_score_replay PROPERTIES LABELS "rom_required;graphics" TIMEOUT 30 FIXTURES_SETUP high_score_recording)
        target_link_libraries(ac_match_replay_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_match_replay_tests)
        add_test(NAME match_replay COMMAND ac_match_replay_tests "${AC_ROM_PATH}"
            "${CMAKE_BINARY_DIR}/ordinary-bonus-smoke.return.acrec" "${CMAKE_BINARY_DIR}/match-smoke")
        set_tests_properties(match_replay PROPERTIES LABELS "rom_required;graphics" TIMEOUT 60
            FIXTURES_REQUIRED ordinary_bonus_recording FIXTURES_SETUP match_recording)
        target_link_libraries(ac_game_over_replay_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_game_over_replay_tests)
        add_test(NAME game_over_replay COMMAND ac_game_over_replay_tests "${AC_ROM_PATH}"
            "${CMAKE_BINARY_DIR}/ordinary-bonus-smoke.return.acrec" "${CMAKE_BINARY_DIR}/game-over-smoke")
        set_tests_properties(game_over_replay PROPERTIES LABELS "rom_required;graphics" TIMEOUT 60
            FIXTURES_REQUIRED ordinary_bonus_recording FIXTURES_SETUP game_over_recording)
        target_link_libraries(ac_ordinary_bonus_replay_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_ordinary_bonus_replay_tests)
        add_test(NAME ordinary_bonus_replay COMMAND ac_ordinary_bonus_replay_tests "${AC_ROM_PATH}" "${CMAKE_BINARY_DIR}/ordinary-bonus-smoke")
        set_tests_properties(ordinary_bonus_replay PROPERTIES LABELS "rom_required;graphics" TIMEOUT 90 FIXTURES_SETUP ordinary_bonus_recording)
        target_link_libraries(ac_bonus_replay_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_bonus_replay_tests)
        add_test(NAME bonus_replay COMMAND ac_bonus_replay_tests "${AC_ROM_PATH}" "${CMAKE_BINARY_DIR}/bonus-smoke")
        set_tests_properties(bonus_replay PROPERTIES LABELS "rom_required;graphics" TIMEOUT 60 FIXTURES_SETUP bonus_recording)
        target_link_libraries(ac_attract_replay_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_attract_replay_tests)
        add_test(NAME attract_replay COMMAND ac_attract_replay_tests "${AC_ROM_PATH}" "${CMAKE_BINARY_DIR}/attract-score.ppm")
        set_tests_properties(attract_replay PROPERTIES LABELS "rom_required;graphics" TIMEOUT 60)
        target_link_libraries(ac_title_logo_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_title_logo_tests)
        add_test(NAME title_logo COMMAND ac_title_logo_tests "${AC_ROM_PATH}" "${CMAKE_BINARY_DIR}/title-logo.ppm")
        set_tests_properties(title_logo PROPERTIES LABELS "rom_required;graphics" TIMEOUT 30)
        set_tests_properties(asset_discovery PROPERTIES LABELS "rom_required;graphics" TIMEOUT 180)
        add_executable(ac_table_replay_tests tests/table_replay_tests.cpp)
        if(AC_REFERENCE_VIDEO_OBSERVER)
            add_executable(ac_cabinet_controls_tests tests/cabinet_controls_tests.cpp)
            add_executable(ac_nudge_multiplier_tests tests/nudge_multiplier_tests.cpp)
            target_link_libraries(ac_nudge_multiplier_tests PRIVATE ac_reference)
            ac_enable_strict_warnings(ac_nudge_multiplier_tests)
            add_test(NAME nudge_multiplier COMMAND ac_nudge_multiplier_tests "${AC_ROM_PATH}" "${CMAKE_BINARY_DIR}/nudge-multiplier.acrec")
            set_tests_properties(nudge_multiplier PROPERTIES TIMEOUT 90 FIXTURES_SETUP nudge_recording)
            add_test(NAME nudge_recording COMMAND alien-crush-tools --rom "${AC_ROM_PATH}" --replay "${CMAKE_BINARY_DIR}/nudge-multiplier.acrec" --verify-replay)
            set_tests_properties(nudge_recording PROPERTIES TIMEOUT 60 FIXTURES_REQUIRED nudge_recording)
            target_include_directories(ac_cabinet_controls_tests PRIVATE src/host)
            target_link_libraries(ac_cabinet_controls_tests PRIVATE ac_reference)
            ac_enable_strict_warnings(ac_cabinet_controls_tests)
            add_test(NAME cabinet_controls COMMAND ac_cabinet_controls_tests "${AC_ROM_PATH}" "${CMAKE_BINARY_DIR}/cabinet-controls.acrec")
            set_tests_properties(cabinet_controls PROPERTIES TIMEOUT 30 FIXTURES_SETUP cabinet_controls_recording)
            add_test(NAME cabinet_controls_replay COMMAND alien-crush-tools --rom "${AC_ROM_PATH}" --replay "${CMAKE_BINARY_DIR}/cabinet-controls.acrec" --verify-replay)
            set_tests_properties(cabinet_controls_replay PROPERTIES TIMEOUT 30 FIXTURES_REQUIRED cabinet_controls_recording)
            if(AC_BUILD_APP)
                add_test(NAME cabinet_controls_host COMMAND alien-crush --rom "${AC_ROM_PATH}" --cabinet --replay "${CMAKE_BINARY_DIR}/cabinet-controls.acrec" --data-dir "${CMAKE_BINARY_DIR}/cabinet-controls-host")
                set_tests_properties(cabinet_controls_host PROPERTIES TIMEOUT 30 FIXTURES_REQUIRED cabinet_controls_recording ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            endif()
            add_executable(ac_inactive_animation_tests tests/inactive_animation_tests.cpp)
            add_executable(ac_cabinet_actors_tests tests/cabinet_actors_tests.cpp)
            target_link_libraries(ac_cabinet_actors_tests PRIVATE ac_reference)
            ac_enable_strict_warnings(ac_cabinet_actors_tests)
            add_test(NAME cabinet_actors COMMAND ac_cabinet_actors_tests "${AC_ROM_PATH}")
            set_tests_properties(cabinet_actors PROPERTIES TIMEOUT 600 LABELS "rom_required;gameplay")
            add_executable(ac_mouth_layer_tests tests/mouth_layer_tests.cpp)
            target_link_libraries(ac_mouth_layer_tests PRIVATE ac_core)
            ac_enable_strict_warnings(ac_mouth_layer_tests)
            add_test(NAME mouth_layer COMMAND ac_mouth_layer_tests "${AC_ROM_PATH}")
            set_tests_properties(mouth_layer PROPERTIES TIMEOUT 60 LABELS "rom_required;graphics")
            target_link_libraries(ac_inactive_animation_tests PRIVATE ac_reference)
            ac_enable_strict_warnings(ac_inactive_animation_tests)
            add_test(NAME inactive_animation COMMAND ac_inactive_animation_tests "${AC_ROM_PATH}")
            set_tests_properties(inactive_animation PROPERTIES TIMEOUT 90 LABELS "rom_required;graphics")
            add_executable(ac_continuous_handoff_tests tests/continuous_handoff_tests.cpp)
            add_executable(ac_demo_handoff_tests tests/demo_handoff_tests.cpp)
            target_link_libraries(ac_demo_handoff_tests PRIVATE ac_reference)
            ac_enable_strict_warnings(ac_demo_handoff_tests)
            add_test(NAME demo_handoff COMMAND ac_demo_handoff_tests "${AC_ROM_PATH}" "${CMAKE_BINARY_DIR}/demo-crossing.acrec")
            set_tests_properties(demo_handoff PROPERTIES TIMEOUT 90 FIXTURES_SETUP demo_handoff_recording)
            if(AC_BUILD_APP)
                add_test(NAME demo_handoff_host COMMAND alien-crush --rom "${AC_ROM_PATH}" --cabinet
                    --replay "${CMAKE_BINARY_DIR}/demo-crossing.acrec" --data-dir "${CMAKE_BINARY_DIR}/demo-handoff-host"
                    --timing-log "${CMAKE_BINARY_DIR}/demo-handoff-host.csv")
                set_tests_properties(demo_handoff_host PROPERTIES TIMEOUT 30 FIXTURES_REQUIRED demo_handoff_recording
                    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            endif()
            target_link_libraries(ac_continuous_handoff_tests PRIVATE ac_reference)
            ac_enable_strict_warnings(ac_continuous_handoff_tests)
            add_test(NAME continuous_handoff COMMAND ac_continuous_handoff_tests "${AC_ROM_PATH}" "${CMAKE_BINARY_DIR}/continuous.acrec")
            set_tests_properties(continuous_handoff PROPERTIES TIMEOUT 60 FIXTURES_SETUP continuous_recording)
            add_test(NAME continuous_recording COMMAND alien-crush-tools --rom "${AC_ROM_PATH}" --replay "${CMAKE_BINARY_DIR}/continuous.acrec" --verify-replay)
            set_tests_properties(continuous_recording PROPERTIES TIMEOUT 60 FIXTURES_REQUIRED continuous_recording)
            if(AC_BUILD_APP)
                add_test(NAME continuous_host COMMAND alien-crush --rom "${AC_ROM_PATH}" --cabinet
                    --replay "${CMAKE_BINARY_DIR}/continuous.acrec.crossing" --frames 120
                    --data-dir "${CMAKE_BINARY_DIR}/continuous-host"
                    --timing-log "${CMAKE_BINARY_DIR}/continuous-host.csv")
                set_tests_properties(continuous_host PROPERTIES TIMEOUT 30 FIXTURES_REQUIRED continuous_recording
                    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            endif()
        endif()
        add_executable(ac_launcher_replay_tests tests/launcher_replay_tests.cpp)
        target_link_libraries(ac_launcher_replay_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_launcher_replay_tests)
        add_test(NAME launcher_replay COMMAND ac_launcher_replay_tests "${AC_ROM_PATH}" "${CMAKE_BINARY_DIR}/launcher-smoke.acrec")
        set_tests_properties(launcher_replay PROPERTIES LABELS "rom_required;input" TIMEOUT 60 FIXTURES_SETUP launcher_recording)
        add_test(NAME launcher_recording COMMAND alien-crush-tools --rom "${AC_ROM_PATH}"
            --replay "${CMAKE_BINARY_DIR}/launcher-smoke.acrec" --verify-replay)
        set_tests_properties(launcher_recording PROPERTIES LABELS "rom_required;input" TIMEOUT 30 FIXTURES_REQUIRED launcher_recording)
        target_link_libraries(ac_table_replay_tests PRIVATE ac_reference)
        ac_enable_strict_warnings(ac_table_replay_tests)
        add_test(NAME table_replay COMMAND ac_table_replay_tests "${AC_ROM_PATH}" "${CMAKE_BINARY_DIR}/table-smoke.acrec")
        set_tests_properties(table_replay PROPERTIES LABELS "rom_required;graphics" TIMEOUT 60 FIXTURES_SETUP table_smoke_recording)
        if(AC_BUILD_APP)
            add_test(NAME bonus_host_smoke COMMAND alien-crush --rom "${AC_ROM_PATH}" --cabinet
                --replay "${CMAKE_BINARY_DIR}/bonus-smoke.room4.acrec" --frames 32 --data-dir "${CMAKE_BINARY_DIR}/bonus-host-smoke")
            set_tests_properties(bonus_host_smoke PROPERTIES LABELS "rom_required;host" TIMEOUT 30
                FIXTURES_REQUIRED bonus_recording ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            add_test(NAME bonus_stage_host_smoke COMMAND alien-crush --rom "${AC_ROM_PATH}" --cabinet
                --bonus-stage 3 --near-max-score --frames 120 --data-dir "${CMAKE_BINARY_DIR}/bonus-stage-host-smoke")
            set_tests_properties(bonus_stage_host_smoke PROPERTIES LABELS "rom_required;host" TIMEOUT 30
                PASS_REGULAR_EXPRESSION "Started in bonus stage 3" ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            add_test(NAME ordinary_bonus_host_smoke COMMAND alien-crush --rom "${AC_ROM_PATH}" --cabinet
                --replay "${CMAKE_BINARY_DIR}/ordinary-bonus-smoke.return.acrec" --frames 300 --data-dir "${CMAKE_BINARY_DIR}/ordinary-bonus-host-smoke")
            set_tests_properties(ordinary_bonus_host_smoke PROPERTIES LABELS "rom_required;host" TIMEOUT 30
                FIXTURES_REQUIRED ordinary_bonus_recording ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            add_test(NAME game_over_host_smoke COMMAND alien-crush --rom "${AC_ROM_PATH}" --cabinet
                --replay "${CMAKE_BINARY_DIR}/game-over-smoke.acrec" --frames 1000 --data-dir "${CMAKE_BINARY_DIR}/game-over-host-smoke")
            set_tests_properties(game_over_host_smoke PROPERTIES LABELS "rom_required;host" TIMEOUT 45
                FIXTURES_REQUIRED game_over_recording ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            add_test(NAME match_host_smoke COMMAND alien-crush --rom "${AC_ROM_PATH}" --cabinet
                --replay "${CMAKE_BINARY_DIR}/match-smoke.acrec" --frames 900 --data-dir "${CMAKE_BINARY_DIR}/match-host-smoke")
            set_tests_properties(match_host_smoke PROPERTIES LABELS "rom_required;host" TIMEOUT 45
                FIXTURES_REQUIRED match_recording ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            add_test(NAME high_score_host_smoke COMMAND alien-crush --rom "${AC_ROM_PATH}" --cabinet
                --replay "${CMAKE_BINARY_DIR}/high-score-smoke.acrec" --frames 600 --data-dir "${CMAKE_BINARY_DIR}/high-score-host-smoke")
            set_tests_properties(high_score_host_smoke PROPERTIES LABELS "rom_required;host" TIMEOUT 30
                FIXTURES_REQUIRED high_score_recording ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            add_test(NAME name_entry_host_smoke COMMAND alien-crush --rom "${AC_ROM_PATH}" --cabinet
                --replay "${CMAKE_BINARY_DIR}/name-entry-smoke.acrec" --frames 1100 --data-dir "${CMAKE_BINARY_DIR}/name-entry-host-smoke")
            set_tests_properties(name_entry_host_smoke PROPERTIES LABELS "rom_required;host" TIMEOUT 45
                FIXTURES_REQUIRED name_entry_recording ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            add_test(NAME bonus_end_host_smoke COMMAND alien-crush --rom "${AC_ROM_PATH}" --cabinet
                --replay "${CMAKE_BINARY_DIR}/bonus-smoke.loss.acrec" --frames 300 --data-dir "${CMAKE_BINARY_DIR}/bonus-end-host-smoke")
            set_tests_properties(bonus_end_host_smoke PROPERTIES LABELS "rom_required;host" TIMEOUT 30
                FIXTURES_REQUIRED bonus_recording ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            add_test(NAME bonus_clear_host_smoke COMMAND alien-crush --rom "${AC_ROM_PATH}" --cabinet
                --replay "${CMAKE_BINARY_DIR}/bonus-smoke.clear.acrec" --frames 700 --data-dir "${CMAKE_BINARY_DIR}/bonus-clear-host-smoke")
            set_tests_properties(bonus_clear_host_smoke PROPERTIES LABELS "rom_required;host" TIMEOUT 45
                FIXTURES_REQUIRED bonus_recording ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            add_executable(ac_cabinet_window_tests tests/cabinet_window_tests.cpp src/host/cabinet.cpp src/host/presentation.cpp src/host/backglass.cpp
                src/host/cabinet_config.cpp src/host/cabinet_input.cpp src/host/cabinet_plunger.cpp src/host/cabinet_nudge.cpp)
            target_include_directories(ac_cabinet_window_tests PRIVATE src/host)
            target_link_libraries(ac_cabinet_window_tests PRIVATE ac_reference SDL3::SDL3)
            ac_enable_strict_warnings(ac_cabinet_window_tests)
            add_test(NAME cabinet_windows COMMAND ac_cabinet_window_tests "${AC_ROM_PATH}" "${CMAKE_BINARY_DIR}/cabinet-window-test")
            set_tests_properties(cabinet_windows PROPERTIES LABELS "rom_required;host" TIMEOUT 30
                ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            add_executable(ac_cabinet_plunger_tests tests/cabinet_plunger_tests.cpp src/host/cabinet_plunger.cpp)
            target_include_directories(ac_cabinet_plunger_tests PRIVATE src/host)
            target_link_libraries(ac_cabinet_plunger_tests PRIVATE ac_reference SDL3::SDL3)
            ac_enable_strict_warnings(ac_cabinet_plunger_tests)
            add_test(NAME cabinet_plunger COMMAND ac_cabinet_plunger_tests "${AC_ROM_PATH}")
            set_tests_properties(cabinet_plunger PROPERTIES LABELS "rom_required;host;input" TIMEOUT 30
                ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            add_test(NAME launcher_host_smoke COMMAND alien-crush --rom "${AC_ROM_PATH}" --cabinet
                --replay "${CMAKE_BINARY_DIR}/launcher-smoke.acrec" --frames 120 --data-dir "${CMAKE_BINARY_DIR}/launcher-host-smoke")
            set_tests_properties(launcher_host_smoke PROPERTIES LABELS "rom_required;host;input" TIMEOUT 30
                FIXTURES_REQUIRED launcher_recording ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            add_test(NAME stitched_smoke COMMAND alien-crush --rom "${AC_ROM_PATH}" --cabinet
                --replay "${CMAKE_BINARY_DIR}/table-smoke.acrec" --frames 32 --data-dir "${CMAKE_BINARY_DIR}/stitched-smoke")
            set_tests_properties(stitched_smoke PROPERTIES LABELS "rom_required;host" TIMEOUT 30
                FIXTURES_REQUIRED table_smoke_recording ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
            add_test(NAME desktop_smoke COMMAND alien-crush --rom "${AC_ROM_PATH}" --frames 30)
            set_tests_properties(desktop_smoke PROPERTIES LABELS "rom_required;host"
                ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy" TIMEOUT 30)
            add_test(NAME cabinet_smoke COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_SOURCE_DIR}/tests/cabinet_smoke.ps1" -Executable "$<TARGET_FILE:alien-crush>"
                -Rom "${AC_ROM_PATH}" -OutputRoot "${CMAKE_BINARY_DIR}/cabinet-smoke")
            set_tests_properties(cabinet_smoke PROPERTIES LABELS "rom_required;host"
                ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy" TIMEOUT 30)
        endif()
    endif()
endif()
