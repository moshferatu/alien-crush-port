// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/runtime/game_runtime.hpp"
#include "alien_crush/core/rom_image.hpp"
#include <libretro.h>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef AC_REFERENCE_VIDEO_OBSERVER
extern "C" void ac_set_display_observer(void (*)(unsigned,unsigned,unsigned,const unsigned short*));
extern "C" void ac_set_palette_observer(void (*)(int,int,unsigned,unsigned,unsigned,unsigned),void (*)(int,int));
#endif

namespace alien_crush {
namespace {
struct CoreOption { std::string_view key; const char* value; };
#include "reference_options.inc"
class Reference final : public ReferenceRuntime {
    inline static std::atomic<Reference*> active{};
    std::thread::id owner = std::this_thread::get_id();
    bool initialized{}, loaded{}, failed{};
    std::string rom_path, directory;
    retro_system_av_info av{};
    PadInput input{};
    std::uint64_t frame_number{};
    int width{}, height{};
    std::vector<std::uint32_t> pixels;
    std::vector<std::int16_t> audio;
    std::array<diagnostics::DisplayRow,1024> observed_rows{};
    std::size_t observed_count{};
    std::vector<diagnostics::PaletteWrite> observed_palette=std::vector<diagnostics::PaletteWrite>(65536);
    std::size_t palette_count{};
    std::array<std::uint16_t,32768> observed_vram{};
    std::vector<render::RasterWrite> observed_memory=std::vector<render::RasterWrite>(65536);
    std::size_t memory_count{};
    bool observation_failed{};
    static void observe_row(unsigned row,unsigned control,unsigned burst,const unsigned short* vram) noexcept {
        auto* self=active.load();
        if(!self) return;
        if(self->observed_count==self->observed_rows.size() || row>65535 || control>65535) {self->observation_failed=true;return;}
        const bool first=self->observed_count==0;
        if(first) self->memory_count=0;
        if(first || std::memcmp(self->observed_vram.data(),vram,65536)!=0) {
            for(unsigned i=0;i<32768;++i) if(first || self->observed_vram[i]!=vram[i]) {
                if(self->memory_count==self->observed_memory.size()) {self->observation_failed=true;return;}
                self->observed_memory[self->memory_count++]={static_cast<std::uint16_t>(row),render::RasterTarget::vram,
                    static_cast<std::uint16_t>(i),vram[i]};
            }
            std::memcpy(self->observed_vram.data(),vram,65536);
        }
        self->observed_rows[self->observed_count++]={static_cast<std::uint16_t>(row),static_cast<std::uint16_t>(control),burst!=0};
    }
    static void observe_palette(int row,int x,unsigned address,unsigned before,unsigned after,unsigned done) noexcept {
        auto* self=active.load();
        if(!self) return;
        if(self->palette_count==self->observed_palette.size() || address>511 || before>511 || after>511) {self->observation_failed=true;return;}
        self->observed_palette[self->palette_count++]={row,x,static_cast<std::uint16_t>(address),
            static_cast<std::uint16_t>(before),static_cast<std::uint16_t>(after),done!=0};
    }
    static void palette_geometry(int x,int y) noexcept {
        auto* self=active.load();
        if(!self) return;
        for(std::size_t i=0;i<self->palette_count;++i) {self->observed_palette[i].x-=x;self->observed_palette[i].row-=y;}
    }
    void check_thread() const {
        if(std::this_thread::get_id()!=owner) throw std::logic_error("Reference runtime must stay on its owning thread");
    }
    void check_loaded() const { check_thread(); if(!loaded) throw std::logic_error("No ROM loaded"); }
    void close() noexcept {
#ifdef AC_REFERENCE_VIDEO_OBSERVER
        ac_set_display_observer(nullptr);
        ac_set_palette_observer(nullptr,nullptr);
#endif
        observed_count=0; palette_count=0; observation_failed=false;
        if(loaded) retro_unload_game();
        if(initialized) retro_deinit();
        loaded=initialized=false;
    }
    static bool RETRO_CALLCONV environment(unsigned command, void* data) {
        auto* self=active.load();
        if(!self || !data) return false;
        switch(command) {
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            *static_cast<const char**>(data)=self->directory.c_str(); return true;
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            return *static_cast<retro_pixel_format*>(data)==RETRO_PIXEL_FORMAT_RGB565;
        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            auto* variable=static_cast<retro_variable*>(data);
            variable->value=nullptr;
            if(variable->key) for(auto option:kCoreOptions) if(option.key==variable->key) {
                variable->value=option.value; return true;
            }
            return false;
        }
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: *static_cast<bool*>(data)=false; return true;
        case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: *static_cast<int*>(data)=3; return true;
        case RETRO_ENVIRONMENT_SET_GEOMETRY: self->av.geometry=*static_cast<retro_game_geometry*>(data); return true;
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: static_cast<retro_log_callback*>(data)->log=&log; return true;
        default: return false;
        }
    }
    static void RETRO_CALLCONV log(retro_log_level level,const char* format,...) {
        if(level<RETRO_LOG_WARN) return;
        va_list arguments; va_start(arguments,format); std::vfprintf(stderr,format,arguments); va_end(arguments);
    }
    static void RETRO_CALLCONV video(const void* data,unsigned w,unsigned h,std::size_t pitch) noexcept {
        auto* self=active.load();
        if(!self || !data) return; // libretro duplicate frame
        if(w==0 || h==0 || w>2048 || h>264 || pitch<w*2U) { self->failed=true; return; }
        try {
            self->pixels.resize(static_cast<std::size_t>(w)*h);
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
                std::uint16_t p;
                std::memcpy(&p,static_cast<const std::byte*>(data)+y*pitch+x*2U,2);
                const std::uint32_t r=(p>>11)&31U,g=(p>>5)&63U,b=p&31U;
                self->pixels[static_cast<std::size_t>(y)*w+x]=0xff000000U|(((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|(b<<3)|(b>>2);
            }
            self->width=static_cast<int>(w); self->height=static_cast<int>(h);
        } catch(...) { self->failed=true; }
    }
    static std::size_t RETRO_CALLCONV samples(const std::int16_t* data,std::size_t frames) noexcept {
        auto* self=active.load();
        if(!self) return 0;
        if(frames==0) return 0;
        if(!data || frames>65536 || self->audio.size()+2*frames>131072) { self->failed=true; return 0; }
        try { self->audio.insert(self->audio.end(),data,data+2*frames); }
        catch(...) { self->failed=true; return 0; }
        return frames;
    }
    static void RETRO_CALLCONV sample(std::int16_t l,std::int16_t r) noexcept { const std::int16_t pair[]{l,r}; samples(pair,1); }
    static void RETRO_CALLCONV poll() {}
    static std::int16_t RETRO_CALLCONV controller(unsigned port,unsigned device,unsigned index,unsigned id) {
        const auto* self=active.load();
        if(!self || port>1 || device!=RETRO_DEVICE_JOYPAD || index!=0) return 0;
        const auto mask=port==0?self->input.player_one:self->input.player_two;
        unsigned bit=0;
        switch(id) {
        case RETRO_DEVICE_ID_JOYPAD_A: bit=Pad::button_i; break;
        case RETRO_DEVICE_ID_JOYPAD_B: bit=Pad::button_ii; break;
        case RETRO_DEVICE_ID_JOYPAD_SELECT: bit=Pad::select; break;
        case RETRO_DEVICE_ID_JOYPAD_START: bit=Pad::run; break;
        case RETRO_DEVICE_ID_JOYPAD_UP: bit=Pad::up; break;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT: bit=Pad::right; break;
        case RETRO_DEVICE_ID_JOYPAD_DOWN: bit=Pad::down; break;
        case RETRO_DEVICE_ID_JOYPAD_LEFT: bit=Pad::left; break;
        default: return 0;
        }
        return (mask&bit)?1:0;
    }
public:
    Reference() {
        pixels.reserve(2048U*264U); audio.reserve(131072);
        Reference* empty=nullptr;
        if(!active.compare_exchange_strong(empty,this)) throw std::logic_error("Beetle is single-instance; use isolated processes for concurrent comparisons");
    }
    ~Reference() override { close(); active.store(nullptr); }
    void load(const std::filesystem::path& path) override {
        check_thread();
        auto result=rom::validate_file(path);
        if(!result.supported()) throw std::runtime_error(result.message);
        const auto absolute=std::filesystem::absolute(path);
        const auto encoded=absolute.u8string(), parent=absolute.parent_path().u8string();
        close();
        rom_path.assign(reinterpret_cast<const char*>(encoded.data()),encoded.size());
        directory.assign(reinterpret_cast<const char*>(parent.data()),parent.size());
        input={}; frame_number=0; width=height=0; pixels.clear(); audio.clear(); failed=false;
        if(retro_api_version()!=RETRO_API_VERSION) throw std::runtime_error("Incompatible libretro ABI");
        retro_set_environment(environment); retro_set_video_refresh(video);
        retro_set_audio_sample(sample); retro_set_audio_sample_batch(samples);
        retro_set_input_poll(poll); retro_set_input_state(controller);
        retro_init(); initialized=true;
        const retro_game_info game{rom_path.c_str(),nullptr,0,nullptr};
        if(!retro_load_game(&game)) { close(); throw std::runtime_error("Beetle rejected Alien Crush ROM"); }
        loaded=true;
        retro_set_controller_port_device(0,RETRO_DEVICE_JOYPAD);
        retro_set_controller_port_device(1,RETRO_DEVICE_JOYPAD);
        retro_get_system_av_info(&av);
        if(av.timing.fps<=0 || av.timing.sample_rate<=0) throw std::runtime_error("Invalid reference timing");
    }
    void reset() override { check_loaded(); retro_reset(); input={}; frame_number=0; pixels.clear(); audio.clear(); width=height=0; failed=false; observed_count=0; palette_count=0; observation_failed=false; }
    Frame advance(PadInput buttons) override {
        check_loaded(); input=buttons; audio.clear(); failed=false;
        observed_count=0; palette_count=0; observation_failed=false; retro_run(); ++frame_number;
        if(failed) throw std::runtime_error("Invalid video/audio received from reference core");
        return {frame_number,width,height,pixels,audio};
    }
    double frames_per_second() const override { return av.timing.fps; }
    int sample_rate() const override { return static_cast<int>(av.timing.sample_rate+0.5); }
    std::string_view revision() const override {
#ifdef AC_REFERENCE_STATE_EXTENSION
        return "beetle-" AC_BEETLE_REVISION "-state1";
#else
        return "beetle-" AC_BEETLE_REVISION;
#endif
    }
    std::span<const std::byte> work_ram() const override {
        check_loaded();
        const auto size=retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
        const auto* memory=static_cast<const std::byte*>(retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
        if(!memory || size<8192) throw std::runtime_error("Reference work RAM unavailable");
        return {memory,8192};
    }
    void observe_display(bool enabled) override {
        check_loaded(); observed_count=0; palette_count=0; observation_failed=false;
#ifdef AC_REFERENCE_VIDEO_OBSERVER
        ac_set_display_observer(enabled?&observe_row:nullptr);
        ac_set_palette_observer(enabled?&observe_palette:nullptr,enabled?&palette_geometry:nullptr);
#else
        if(enabled) throw std::runtime_error("Reference video observer is disabled in this build");
#endif
    }
    std::span<const diagnostics::DisplayRow> display_rows() const override {
        check_loaded();
        if(observation_failed) throw std::runtime_error("Reference display observation overflow");
        return {observed_rows.data(),observed_count};
    }
    std::span<const diagnostics::PaletteWrite> palette_writes() const override {
        check_loaded();
        if(observation_failed) throw std::runtime_error("Reference video observation overflow");
        return {observed_palette.data(),palette_count};
    }
    std::span<const render::RasterWrite> vram_rows() const override {
        check_loaded();
        if(observation_failed) throw std::runtime_error("Reference video observation overflow");
        return {observed_memory.data(),observed_count?memory_count:0};
    }
    state::SaveStateContainer save() const override {
        check_loaded();
        state::SaveStateContainer result;
        result.rom_sha256=rom::kSupportedSha256; result.core_revision=revision(); result.frame_number=frame_number;
        // Owl's active filter tail changes size. Reserve its maximum delta so
        // a quick state remains loadable at a different resampler phase.
        const auto needed=retro_serialize_size();
        if(needed==0 || needed>63U*1024U*1024U) throw std::runtime_error("Invalid reference state size");
        result.core_state.resize(needed+65536U);
        if(result.core_state.empty() || !retro_serialize(result.core_state.data(),result.core_state.size())) throw std::runtime_error("Reference serialization failed");
        return result;
    }
    void restore(const state::SaveStateContainer& saved) override {
        check_loaded();
        if(saved.format_version!=state::kSaveStateFormatVersion || saved.rom_sha256!=rom::kSupportedSha256 || saved.core_revision!=revision()) throw std::runtime_error("Incompatible quick state ROM, backend, or version");
        if(saved.core_state.empty() || saved.core_state.size()>64U*1024U*1024U) throw std::runtime_error("Incompatible quick state payload size");
        const auto rollback=save();
        if(!retro_unserialize(saved.core_state.data(),saved.core_state.size())) {
            if(!retro_unserialize(rollback.core_state.data(),rollback.core_state.size())) { close(); throw std::runtime_error("Quick state and rollback failed; runtime closed"); }
            throw std::runtime_error("Reference rejected quick state; previous state restored");
        }
        frame_number=saved.frame_number; input={}; pixels.clear(); audio.clear(); width=height=0; failed=false;
        observed_count=0; palette_count=0; observation_failed=false;
    }
};
}
std::unique_ptr<ReferenceRuntime> make_reference_runtime() { return std::make_unique<Reference>(); }
}
