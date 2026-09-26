// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/runtime/game_runtime.hpp"
#include "alien_crush/runtime/reference_launcher.hpp"
#include "alien_crush/core/rom_image.hpp"
#include "alien_crush/input/nudge_multiplier.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/gameplay/cabinet_actors.hpp"
#include "music_voice_overlay.hpp"
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
extern "C" bool ac_copy_cpu_snapshot(unsigned short*,unsigned char*);
extern "C" bool ac_copy_video_snapshot(unsigned short*,unsigned short*,unsigned short*,unsigned short*);
extern "C" void ac_set_continuous_handoff(bool);
extern "C" void ac_set_cabinet_controls(bool);
extern "C" void ac_set_flipper_masks(unsigned char,unsigned char);
extern "C" bool ac_set_music_voice_overlay(bool,const unsigned char*,unsigned);
extern "C" void ac_set_nudge_motion(unsigned short (*)(unsigned short,unsigned short));
extern "C" void ac_set_cabinet_actors(unsigned (*)(unsigned,unsigned,unsigned,int*),const unsigned short*,unsigned);
extern "C" void ac_set_cabinet_watch(int);
#endif

namespace alien_crush {
namespace {
struct CoreOption { std::string_view key; const char* value; };
#include "reference_options.inc"
class Reference final : public ReferenceRuntime {
    HandoffMode handoff;
    ControlMode controls;
    double nudge_multiplier;
    input::FlipperMasks flippers;
    ActorMode actor_mode;
    std::unique_ptr<gameplay::CabinetActors> actors;
    std::string custom_revision;
    inline static std::atomic<Reference*> active{};
    std::thread::id owner = std::this_thread::get_id();
    bool initialized{}, loaded{}, failed{}, core_started{};
    bool actors_failed{};
    std::string actors_failure;
    bool watching{};
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
    // The dispatcher (01:45A8) starting, or entering an owned actor's routine
    // with its return, 45C7, on the stack. Returns the next PC, or 0 for the
    // ROM to continue; *a, when set, is A for that PC. In verification, bit 16
    // of pc marks the watched PC, with the ROM's A in *a.
    static unsigned cabinet_actors(unsigned pc,unsigned x,unsigned caller,int* a) noexcept {
        auto* self=active.load();
        auto* memory=static_cast<std::byte*>(retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
        if(!self || !self->actors || !memory || retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM)<8192) return 0;
        const std::span<std::byte> ram{memory,8192};
        try {
            if(pc&0x10000) {
                self->watching=false;
                const auto failure=self->actors->mismatch(ram,static_cast<unsigned>(*a));
                if(!failure.empty()) {self->actors_failure=failure;self->actors_failed=true;}
                return 0;
            }
            if(pc==0xf292) { // The vblank's palette scheduler.
                self->actors->update_palettes(ram);
                return 0;
            }
            if(pc==0x45a8) {
                // The table's game loops: live 01:4098, demo 01:4035, and the
                // loops at 00:FDD0 and 00:FEC7 that run the table between balls.
                if(caller==0x409a || caller==0x4037 || caller==0xfdd2 || caller==0xfec9)
                    self->actors->update_inactive(ram,self->frame_number);
                return 0;
            }
            if(caller!=0x45c7 || x<4 || x>=64) return 0;
            const auto type=std::to_integer<std::uint8_t>(ram[0x1500+x]);
            if(!self->actors->owns(type) || self->actors->entry(type)!=pc) return 0;
            if(self->actor_mode==ActorMode::verify) {
                if(self->watching) {
                    self->actors_failure="The ROM never reached the port's predicted PC";self->actors_failed=true;
                }
                watch(self->actors->predict(ram,x));
                self->watching=true;
                return 0;
            }
            const auto next=self->actors->update_active(ram,x,self->frame_number);
            if(!next) return 0;
            if(!next->pc) return 0x45e1; // The RTS at 01:45E1 returns to the dispatcher.
            *a=next->a;
            return next->pc;
        } catch(const std::exception& e) {self->actors_failure=e.what();self->actors_failed=true;return 0;}
        catch(...) {self->actors_failed=true;return 0;}
    }
    static void watch([[maybe_unused]] int pc) noexcept {
#ifdef AC_REFERENCE_VIDEO_OBSERVER
        ac_set_cabinet_watch(pc);
#endif
    }
    void forget_watch() noexcept {watching=false;watch(-1);}
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
        if(loaded) ac_set_music_voice_overlay(false,nullptr,0);
        ac_set_display_observer(nullptr);
        ac_set_palette_observer(nullptr,nullptr);
        ac_set_cabinet_actors(nullptr,nullptr,0);
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
    explicit Reference(HandoffMode mode,ControlMode input_mode,double multiplier,input::FlipperMasks masks,ActorMode actor_ownership) :
        handoff(mode),controls(input_mode),nudge_multiplier(multiplier),flippers(masks),actor_mode(actor_ownership) {
        if(controls!=ControlMode::cabinet && flippers!=input::FlipperMasks{})
            throw std::runtime_error("Flipper inputs apply to cabinet controls only");
        if(actor_mode!=ActorMode::original && handoff!=HandoffMode::continuous)
            throw std::runtime_error("Cabinet actors require continuous handoff");
        // The nudge factor is last: recorded_nudge_multiplier reads to the end.
        const auto marker=actor_mode==ActorMode::cabinet?kCabinetActorsRevision:
            actor_mode==ActorMode::cabinet_actors1?kCabinetActors1Revision:
            actor_mode==ActorMode::cabinet_aliens?kCabinetAliensRevision:std::string_view{};
        const auto suffix=std::string(marker)+
            input::flipper_revision_suffix(flippers)+input::nudge_revision_suffix(multiplier);
        if(!suffix.empty()) {
            const auto base=std::string(revision());
            // Save headers allow 64 bytes. Keep the pinned revision prefix and
            // all mode markers, with room for a lossless double representation:
            // eight characters of the revision, fewer only if that is too long.
            const auto hash_end=7+std::string_view(AC_BEETLE_REVISION).size();
            const auto markers=base.substr(hash_end);
            std::size_t keep=8;
            while(keep>1 && 7+keep+markers.size()+suffix.size()>64) --keep;
            custom_revision=base.substr(0,7+keep)+markers+suffix;
        }
#ifndef AC_REFERENCE_VIDEO_OBSERVER
        if(mode==HandoffMode::continuous) throw std::runtime_error("Continuous handoff requires the video-enabled backend");
        if(controls==ControlMode::cabinet) throw std::runtime_error("Cabinet controls require the video-enabled backend");
        if(multiplier!=1) throw std::runtime_error("Nudge multiplier requires the video-enabled backend");
        if(flippers!=input::FlipperMasks{}) throw std::runtime_error("Flipper inputs require the video-enabled backend");
        if(actor_mode!=ActorMode::original) throw std::runtime_error("Cabinet actors require the video-enabled backend");
#endif
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
        input={}; frame_number=0; width=height=0; pixels.clear(); audio.clear(); failed=false;core_started=false;
        if(retro_api_version()!=RETRO_API_VERSION) throw std::runtime_error("Incompatible libretro ABI");
        retro_set_environment(environment); retro_set_video_refresh(video);
        retro_set_audio_sample(sample); retro_set_audio_sample_batch(samples);
        retro_set_input_poll(poll); retro_set_input_state(controller);
        retro_init(); initialized=true;
        const retro_game_info game{rom_path.c_str(),nullptr,0,nullptr};
        if(!retro_load_game(&game)) { close(); throw std::runtime_error("Beetle rejected Alien Crush ROM"); }
        loaded=true;
        std::vector<std::uint16_t> hooked;
        if(actor_mode!=ActorMode::original) {
            actors=std::make_unique<gameplay::CabinetActors>(core::read_file(path),
                actor_mode==ActorMode::cabinet_aliens?gameplay::CabinetActors::Scope::aliens:gameplay::CabinetActors::Scope::table,
                actor_mode!=ActorMode::verify,actor_mode!=ActorMode::cabinet_actors1);
            hooked=actors->entries();hooked.push_back(0x45a8);hooked.push_back(0xf292);
        }
#ifdef AC_REFERENCE_VIDEO_OBSERVER
        ac_set_cabinet_actors(actors?&cabinet_actors:nullptr,hooked.data(),static_cast<unsigned>(hooked.size()));
        ac_set_continuous_handoff(handoff==HandoffMode::continuous);
        ac_set_cabinet_controls(controls==ControlMode::cabinet);
        ac_set_flipper_masks(flippers.left,flippers.right);
        ac_set_nudge_motion(nudge_multiplier==1?nullptr:+[](unsigned short motion,unsigned short impulse) -> unsigned short {
            return input::scale_nudge_motion(motion,impulse,active.load()->nudge_multiplier);
        });
#endif
        retro_set_controller_port_device(0,RETRO_DEVICE_JOYPAD);
        retro_set_controller_port_device(1,RETRO_DEVICE_JOYPAD);
        retro_get_system_av_info(&av);
        if(av.timing.fps<=0 || av.timing.sample_rate<=0) throw std::runtime_error("Invalid reference timing");
    }
    void reset() override { check_loaded(); retro_reset(); if(actors) {actors->reset();forget_watch();} input={}; frame_number=0; pixels.clear(); audio.clear(); width=height=0; failed=false; observed_count=0; palette_count=0; observation_failed=false; }
    Frame advance(PadInput buttons) override {
        check_loaded(); input=buttons; audio.clear(); failed=false; actors_failed=false; actors_failure.clear();
        observed_count=0; palette_count=0; observation_failed=false; retro_run(); core_started=true; ++frame_number;
        if(failed) throw std::runtime_error("Invalid video/audio received from reference core");
        if(actors_failed) throw std::runtime_error("Cabinet actor update failed"+(actors_failure.empty()?std::string{}:": "+actors_failure));
        return {frame_number,width,height,pixels,audio};
    }
    double frames_per_second() const override { return av.timing.fps; }
    int sample_rate() const override { return static_cast<int>(av.timing.sample_rate+0.5); }
    std::string_view revision() const override {
        if(!custom_revision.empty()) return custom_revision;
#ifdef AC_REFERENCE_STATE_EXTENSION
        if(controls==ControlMode::cabinet) return handoff==HandoffMode::continuous?
            "beetle-" AC_BEETLE_REVISION "-s1-h1-controls1":"beetle-" AC_BEETLE_REVISION "-s1-controls1";
        if(handoff==HandoffMode::continuous) return "beetle-" AC_BEETLE_REVISION "-state1-handoff1";
        return "beetle-" AC_BEETLE_REVISION "-state1";
#else
        if(controls==ControlMode::cabinet) return handoff==HandoffMode::continuous?
            "beetle-" AC_BEETLE_REVISION "-h1-controls1":"beetle-" AC_BEETLE_REVISION "-controls1";
        if(handoff==HandoffMode::continuous) return "beetle-" AC_BEETLE_REVISION "-handoff1";
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
    bool apply_plunger_draw(std::uint8_t percent) override {
        check_loaded();
        const auto size=retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
        auto* memory=static_cast<std::byte*>(retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
        if(!memory || size<8192) return false;
        return apply_reference_launcher_draw({memory,8192},percent);
    }
    bool restore_high_scores(const core::HighScores& table) override {
        check_loaded();
        auto* memory=static_cast<std::byte*>(retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
        if(!memory || retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM)<8192) return false;
        return core::write_high_scores({memory,8192},table);
    }
    bool cancel_plunger_draw() override {
        check_loaded();
        const auto size=retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
        auto* memory=static_cast<std::byte*>(retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
        if(!memory || size<8192) return false;
        return cancel_reference_launcher_draw({memory,8192});
    }
    void music_voice_overlay(bool enabled) override {
        check_loaded();
#ifdef AC_REFERENCE_VIDEO_OBSERVER
        std::vector<std::byte> waveforms;
        if(enabled) {
            // The driver's waveforms, as it uploads them to the PSG.
            const auto rom=core::read_file(core::path_from_utf8(rom_path));
            constexpr auto offset=audio::MusicVoiceOverlay::kWaveformOffset,size=audio::MusicVoiceOverlay::kWaveformTableSize;
            if(rom.size()<offset+size) throw std::runtime_error("The ROM has no sound driver waveforms");
            waveforms.assign(rom.begin()+offset,rom.begin()+offset+size);
        }
        if(!ac_set_music_voice_overlay(enabled,reinterpret_cast<const unsigned char*>(waveforms.data()),static_cast<unsigned>(waveforms.size())))
            throw std::runtime_error("The music voice overlay could not start");
#else
        if(enabled) throw std::runtime_error("The music voice overlay requires the video-enabled backend");
#endif
    }
    bool supports_display_observation() const override {
#ifdef AC_REFERENCE_VIDEO_OBSERVER
        return true;
#else
        return false;
#endif
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
    diagnostics::ReferenceSnapshot video_snapshot() const override {
        check_loaded();
#ifdef AC_REFERENCE_VIDEO_OBSERVER
        diagnostics::ReferenceSnapshot result;result.frame=frame_number;
        std::array<unsigned short,4> registers{};
        if(!ac_copy_cpu_snapshot(&result.pc,result.mpr.data()) ||
            !ac_copy_video_snapshot(result.vram.data(),result.palette.data(),result.sat.data(),registers.data()))
            throw std::runtime_error("Reference snapshot unavailable");
        result.control=registers[0];result.memory_width=registers[1];result.scroll_x=registers[2];result.scroll_y=registers[3];
        return result;
#else
        return diagnostics::decode_reference_snapshot(save());
#endif
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
        // Libretro configures its resampler on the first retro_run. Do that
        // before loading an in-progress state, otherwise SoundFormatChanged
        // discards the restored audio history on the first playback frame.
        if(!core_started && saved.frame_number) advance({});
        const auto rollback=save();
        if(!retro_unserialize(saved.core_state.data(),saved.core_state.size())) {
            if(!retro_unserialize(rollback.core_state.data(),rollback.core_state.size())) { close(); throw std::runtime_error("Quick state and rollback failed; runtime closed"); }
            throw std::runtime_error("Reference rejected quick state; previous state restored");
        }
        if(actors) {actors->reset();forget_watch();}
        frame_number=saved.frame_number; input={}; pixels.clear(); audio.clear(); width=height=0; failed=false;
        observed_count=0; palette_count=0; observation_failed=false;
    }
};
}
std::unique_ptr<ReferenceRuntime> make_reference_runtime(HandoffMode mode,ControlMode controls,double multiplier,input::FlipperMasks flippers,ActorMode actors) { return std::make_unique<Reference>(mode,controls,multiplier,flippers,actors); }
}
