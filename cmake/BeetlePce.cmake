include(FetchContent)
set(AC_BEETLE_REVISION "ae99235c2139c176c1a8d0fde2957bf701d3cab0")
function(ac_provide_beetle_pce)
    FetchContent_Declare(ac_beetle_upstream
        GIT_REPOSITORY https://github.com/libretro/beetle-pce-libretro.git
        GIT_TAG ${AC_BEETLE_REVISION}
        GIT_SHALLOW FALSE GIT_SUBMODULES "")
    FetchContent_MakeAvailable(ac_beetle_upstream)
    set(beetle_root "${ac_beetle_upstream_SOURCE_DIR}")
    # Deliberately no Devil's Crush execution or device replacement patches.
    set(
        beetle_cxx_sources
        "${beetle_root}/libretro.cpp"
        "${beetle_root}/mednafen/pce/huc6280.cpp"
        "${beetle_root}/mednafen/pce/huc.cpp"
        "${beetle_root}/mednafen/pce/input.cpp"
        "${beetle_root}/mednafen/pce/mcgenjin.cpp"
        "${beetle_root}/mednafen/pce/pce.cpp"
        "${beetle_root}/mednafen/pce/pcecd.cpp"
        "${beetle_root}/mednafen/pce/tsushin.cpp"
        "${beetle_root}/mednafen/pce/vce.cpp"
        "${beetle_root}/mednafen/pce/input/gamepad.cpp"
        "${beetle_root}/mednafen/pce/input/mouse.cpp"
        "${beetle_root}/mednafen/pce/input/tsushinkb.cpp"
        "${beetle_root}/mednafen/hw_misc/arcade_card/arcade_card.cpp"
        "${beetle_root}/mednafen/hw_sound/pce_psg/pce_psg.cpp"
        "${beetle_root}/mednafen/hw_video/huc6270/vdc.cpp"
        "${beetle_root}/mednafen/cdrom/CDAccess.cpp"
        "${beetle_root}/mednafen/cdrom/CDAccess_Image.cpp"
        "${beetle_root}/mednafen/cdrom/CDAccess_CCD.cpp"
        "${beetle_root}/mednafen/cdrom/CDAFReader.cpp"
        "${beetle_root}/mednafen/cdrom/CDAFReader_Vorbis.cpp"
        "${beetle_root}/mednafen/cdrom/cdromif.cpp"
        "${beetle_root}/mednafen/cdrom/CDUtility.cpp"
        "${beetle_root}/mednafen/cdrom/lec.cpp"
        "${beetle_root}/mednafen/cdrom/galois.cpp"
        "${beetle_root}/mednafen/cdrom/recover-raw.cpp"
        "${beetle_root}/mednafen/cdrom/l-ec.cpp"
        "${beetle_root}/mednafen/cdrom/edc_crc32.cpp"
        "${beetle_root}/mednafen/cdrom/scsicd.cpp"
        "${beetle_root}/mednafen/cdrom/CDAccess_CHD.cpp"
        "${beetle_root}/mednafen/settings.cpp"
        "${beetle_root}/mednafen/general.cpp"
        "${beetle_root}/mednafen/FileStream.cpp"
        "${beetle_root}/mednafen/MemoryStream.cpp"
        "${beetle_root}/mednafen/Stream.cpp"
        "${beetle_root}/mednafen/state.cpp"
        "${beetle_root}/mednafen/mempatcher.cpp"
        "${beetle_root}/mednafen/sound/okiadpcm.cpp"
        "${beetle_root}/mednafen/sound/OwlResampler.cpp"
    )

    set(
        beetle_c_sources
        "${beetle_root}/mednafen/sound/Blip_Buffer.c"
        "${beetle_root}/mednafen/file.c"
        "${beetle_root}/mednafen/mednafen-endian.c"
        "${beetle_root}/mednafen/cputest/cputest.c"
        "${beetle_root}/libretro-common/streams/file_stream.c"
        "${beetle_root}/libretro-common/streams/file_stream_transforms.c"
        "${beetle_root}/libretro-common/file/file_path.c"
        "${beetle_root}/libretro-common/file/retro_dirent.c"
        "${beetle_root}/libretro-common/lists/string_list.c"
        "${beetle_root}/libretro-common/lists/dir_list.c"
        "${beetle_root}/libretro-common/compat/compat_strl.c"
        "${beetle_root}/libretro-common/compat/compat_snprintf.c"
        "${beetle_root}/libretro-common/compat/compat_posix_string.c"
        "${beetle_root}/libretro-common/compat/compat_strcasestr.c"
        "${beetle_root}/libretro-common/compat/fopen_utf8.c"
        "${beetle_root}/libretro-common/encodings/encoding_utf.c"
        "${beetle_root}/libretro-common/encodings/encoding_crc32.c"
        "${beetle_root}/libretro-common/vfs/vfs_implementation.c"
        "${beetle_root}/libretro-common/memmap/memalign.c"
        "${beetle_root}/libretro-common/string/stdstring.c"
        "${beetle_root}/libretro-common/time/rtime.c"
        "${beetle_root}/deps/lzma-19.00/src/Alloc.c"
        "${beetle_root}/deps/lzma-19.00/src/Bra86.c"
        "${beetle_root}/deps/lzma-19.00/src/BraIA64.c"
        "${beetle_root}/deps/lzma-19.00/src/CpuArch.c"
        "${beetle_root}/deps/lzma-19.00/src/Delta.c"
        "${beetle_root}/deps/lzma-19.00/src/LzFind.c"
        "${beetle_root}/deps/lzma-19.00/src/Lzma86Dec.c"
        "${beetle_root}/deps/lzma-19.00/src/LzmaDec.c"
        "${beetle_root}/deps/lzma-19.00/src/LzmaEnc.c"
        "${beetle_root}/deps/libchdr/src/libchdr_bitstream.c"
        "${beetle_root}/deps/libchdr/src/libchdr_cdrom.c"
        "${beetle_root}/deps/libchdr/src/libchdr_chd.c"
        "${beetle_root}/deps/libchdr/src/libchdr_flac.c"
        "${beetle_root}/deps/libchdr/src/libchdr_huffman.c"
        "${beetle_root}/deps/zstd/lib/common/entropy_common.c"
        "${beetle_root}/deps/zstd/lib/common/error_private.c"
        "${beetle_root}/deps/zstd/lib/common/fse_decompress.c"
        "${beetle_root}/deps/zstd/lib/common/zstd_common.c"
        "${beetle_root}/deps/zstd/lib/common/xxhash.c"
        "${beetle_root}/deps/zstd/lib/decompress/huf_decompress.c"
        "${beetle_root}/deps/zstd/lib/decompress/zstd_ddict.c"
        "${beetle_root}/deps/zstd/lib/decompress/zstd_decompress.c"
        "${beetle_root}/deps/zstd/lib/decompress/zstd_decompress_block.c"
        "${beetle_root}/deps/zlib-1.2.11/adler32.c"
        "${beetle_root}/deps/zlib-1.2.11/crc32.c"
        "${beetle_root}/deps/zlib-1.2.11/inffast.c"
        "${beetle_root}/deps/zlib-1.2.11/inflate.c"
        "${beetle_root}/deps/zlib-1.2.11/inftrees.c"
        "${beetle_root}/deps/zlib-1.2.11/zutil.c"
        "${beetle_root}/mednafen/tremor/bitwise.c"
        "${beetle_root}/mednafen/tremor/block.c"
        "${beetle_root}/mednafen/tremor/codebook.c"
        "${beetle_root}/mednafen/tremor/floor0.c"
        "${beetle_root}/mednafen/tremor/floor1.c"
        "${beetle_root}/mednafen/tremor/framing.c"
        "${beetle_root}/mednafen/tremor/info.c"
        "${beetle_root}/mednafen/tremor/mapping0.c"
        "${beetle_root}/mednafen/tremor/mdct.c"
        "${beetle_root}/mednafen/tremor/registry.c"
        "${beetle_root}/mednafen/tremor/res012.c"
        "${beetle_root}/mednafen/tremor/sharedbook.c"
        "${beetle_root}/mednafen/tremor/synthesis.c"
        "${beetle_root}/mednafen/tremor/vorbisfile.c"
        "${beetle_root}/mednafen/tremor/window.c"
    )

    if(AC_REFERENCE_STATE_EXTENSION)
        include(ReferenceState)
        set(state_source_root "${CMAKE_CURRENT_BINARY_DIR}/reference-state")
        ac_prepare_reference_state("${beetle_root}" "${state_source_root}")
        list(REMOVE_ITEM beetle_cxx_sources "${beetle_root}/mednafen/pce/pce.cpp"
            "${beetle_root}/mednafen/hw_sound/pce_psg/pce_psg.cpp"
            "${beetle_root}/mednafen/pce/huc6280.cpp" "${beetle_root}/mednafen/pce/vce.cpp")
        list(APPEND beetle_cxx_sources "${state_source_root}/pce.cpp" "${state_source_root}/pce_psg.cpp"
            "${state_source_root}/huc6280.cpp" "${state_source_root}/vce.cpp")
    endif()
    add_library(
        ac_beetle_pce
        STATIC
        ${beetle_cxx_sources}
        ${beetle_c_sources}
    )
    add_library(AlienCrush::BeetlePce ALIAS ac_beetle_pce)

    target_include_directories(
        ac_beetle_pce
        SYSTEM
        PUBLIC
            "${beetle_root}/libretro-common/include"
        PRIVATE
            "${beetle_root}"
            "${beetle_root}/mednafen/pce"
            "${beetle_root}/mednafen/hw_sound/pce_psg"
            "${beetle_root}/mednafen"
            "${beetle_root}/mednafen/include"
            "${beetle_root}/mednafen/hw_sound"
            "${beetle_root}/mednafen/hw_cpu"
            "${beetle_root}/mednafen/hw_misc"
            "${beetle_root}/deps/lzma-19.00/include"
            "${beetle_root}/deps/libchdr/include"
            "${beetle_root}/deps/zstd/lib"
            "${beetle_root}/deps/zlib-1.2.11"
    )

    target_compile_features(ac_beetle_pce PRIVATE cxx_std_20)
    target_compile_definitions(
        ac_beetle_pce
        PUBLIC
            RETRO_API=
        PRIVATE
            WINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP
            PACKAGE="mednafen"
            MEDNAFEN_VERSION_NUMERIC=931
            WANT_PCE_EMU
            WANT_STEREO_SOUND
            STDC_HEADERS
            __STDC_LIMIT_MACROS
            __LIBRETRO__
            _LOW_ACCURACY_
            INLINE=_inline
            WANT_16BPP
            FRONTEND_SUPPORTS_RGB565
            NEED_CD
            HAVE_CHD
            _7ZIP_ST
            ZSTD_DISABLE_ASM
            NEED_TREMOR
            GIT_VERSION=" ae99235"
    )

    if(MSVC)
        target_compile_options(ac_beetle_pce PRIVATE /W0 /FS)
    endif()

    set_target_properties(
        ac_beetle_pce
        PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
    )
endfunction()
