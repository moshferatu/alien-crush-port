// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/render/table_playfield.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
namespace alien_crush::render {
namespace {
// Ordinary bonus entry: 40B4 -> FDC1 -> FDE5 -> FE92 (fade out).
// Return: FF04 -> FFB5 (fade in). Require the enclosing ordinary-play
// return, not a stale scratch byte or a palette that happens to look faded.
int bonus_table_fade(std::span<const std::byte> ram) {
    const auto b=[&](unsigned p){return std::to_integer<unsigned>(ram[p]);};
    if(b(0x1fe)!=0xb6 || b(0x1ff)!=0x40 || b(0x79)>1) return 0;
    if(b(0x1fc)==0xc3 && b(0x1fd)==0xfd && b(0x1fb)==0xfe &&
        (b(0x1fa)==6 || b(0x1fa)==9)) return -1;
    if(b(0x1fa)==0xb7 && b(0x1fb)==0xff &&
        ((b(0x1fc)==0x18 && b(0x1fd)==4) || (b(0x1fc)==0xc0 && b(0x1fd)==0xff))) return 1;
    return 0;
}
}
bool cabinet_viewport_blank(const diagnostics::ReferenceSnapshot& current,
    const diagnostics::ReferenceSnapshot* previous,std::span<const std::byte> ram) {
    const auto scene=diagnostics::observe_alien_state(ram);
    if(scene.title_transition) return true;
    if(scene.high_score_menu) {
        // 1C:46FD -> 48B5 waits at E003 before disabling the outgoing
        // display. The score wrapper has two extra stack bytes when entered
        // from pause or practice, compared with normal/demo game end.
        const unsigned outer=(ram[0x1f7]==std::byte{0xfd} && ram[0x1f8]==std::byte{0xeb})?0x1f7:0x1f9;
        return ram[outer-2]==std::byte{0xff} && ram[outer-1]==std::byte{0x46};
    }
    // FE37 enables video before EEDF clears the newly loaded palette for
    // its fade. The first enabled raster consequently flashes at full
    // brightness. Keep this one frame black, then show the original fade.
    return scene.bonus_room && previous && current.frame==previous->frame+1 &&
        !(previous->control&0xc0) && (current.control&0xc0);
}
bool TableTransitionClock::omit(const diagnostics::ReferenceSnapshot& video,std::span<const std::byte> ram,bool table_available) {
    if(ram.size()!=8192) throw std::runtime_error("Table clock requires 8192-byte work RAM");
    const auto b=[&](unsigned p){return std::to_integer<unsigned>(ram[p]);};
    const auto caller=b(0x1fc)|(b(0x1fd)<<8);
    // Saved returns from the literal calls in 03:89B3..89EF / 8A0A..8A47.
    // These cover frame waits, asset/actor loading and scheduler setup. Require
    // the enclosing 01:409E call too; stale stack data alone cannot arm this.
    constexpr unsigned upper[]{0x89b5,0x89b8,0x89cf,0x89d2,0x89d5,0x89eb,0x89ee};
    constexpr unsigned lower[]{0x8a1d,0x8a20,0x8a2a,0x8a2d,0x8a30,0x8a43,0x8a46};
    const bool to_upper=std::find(std::begin(upper),std::end(upper),caller)!=std::end(upper);
    const bool to_lower=std::find(std::begin(lower),std::end(lower),caller)!=std::end(lower);
    const bool scene=table_available && b(0x79)<=1 && !b(0x76) && !b(0x7c) && !b(0x16) && !b(0x17) &&
        !(b(0x53)&0x80) && !(b(0x54)&0x80) && b(0x1500)==1 && (b(0x1540)&0xa0)==0x80;
    // The lower handoff subtracts E0 before changing table code. Compare full
    // fixed-point coordinates in the original world's 224-pixel coordinate
    // system, independently of the artwork's 216-pixel projection.
    const unsigned half=to_lower && b(0x1fe)==0xa0 && b(0x1ff)==0x40?0:b(0x79);
    const unsigned x=b(0x1580)|(b(0x15c0)<<8)|(b(0x1600)<<16);
    const unsigned y=(b(0x1640)|(b(0x1680)<<8)|(b(0x16c0)<<16))+(half==0?224*256:0);
    const bool stationary=valid_ && video.frame==frame_+1 && x==x_ && y==y_;
    const bool wait=video.mpr[7]==0 && video.pc>=0xe2f8 && video.pc<=0xe2fd;
    const bool copying=(video.mpr[7]==0 && video.pc>=0xe3f7 && video.pc<=0xe4a0) ||
        (video.mpr[2]==1 && video.pc>=0x46fd && video.pc<0x4740);
    const bool transition=(to_upper || to_lower) && b(0x1fe)==0xa0 && b(0x1ff)==0x40;
    transition_frames_=transition?(video.frame==frame_+1?std::min(33u,transition_frames_+1):1):0;
    const bool final_wait=transition_ && wait && (caller==0x89ee || caller==0x8a46) &&
        b(0x1fe)==0x88 && b(0x1ff)==0x40;
    const bool result=scene && ((transition && transition_frames_<=32 && (wait || copying)) || final_wait) && stationary;
    upload_follows_=scene && transition && transition_frames_<=32 && wait && (caller==0x89b5 || caller==0x8a1d);
    frame_=video.frame;x_=x;y_=y;valid_=scene;transition_=scene && transition && transition_frames_<=32;
    return result;
}
bool table_transition_upload(const diagnostics::ReferenceSnapshot& video,std::span<const std::byte> ram) {
    if(ram.size()!=8192) throw std::runtime_error("Table upload requires 8192-byte work RAM");
    const auto b=[&](unsigned p){return std::to_integer<unsigned>(ram[p]);};
    // E3F7..E438 is the original word-RLE upload loop. Require its bank and
    // complete enclosing call chain, not merely an old return address in RAM.
    if(video.mpr[7]!=0 || video.pc<0xe3f7 || video.pc>=0xe439 || b(0x79)>1 ||
        b(0x76) || b(0x7c) || b(0x16) || b(0x17) || (b(0x53)&0x80) || (b(0x54)&0x80) ||
        b(0x1500)!=1 || (b(0x1540)&0xa0)!=0x80) return false;
    constexpr unsigned chain[]{0x35,0xeb,0x18,4,3,2,1,0x0c,0xe7};
    for(unsigned i=0;i<std::size(chain);++i) if(b(0x1f3+i)!=chain[i]) return false;
    // 03:89CD / 03:8A28 -> E705 -> EB27 -> table loader -> E3xx.
    return b(0x1fc)==(b(0x79)?0xcfu:0x2au) && b(0x1fd)==(b(0x79)?0x89u:0x8au) &&
        b(0x1fe)==0xa0 && b(0x1ff)==0x40;
}
void TableTransitionHold::reset() {
    image_.reset(); backdrop_.reset(); frame_=0; table_=2; remaining_=0; held_=false;ball_updated_=false;
}
std::optional<Image> TableTransitionHold::present(std::optional<Image> live,std::uint64_t frame,
    unsigned table,unsigned camera,std::span<const std::byte> ram,const TableBallBackdrop* backdrop,const Sprite* ball) {
    if(ram.size()!=8192) throw std::runtime_error("Table transition requires 8192-byte work RAM");
    ball_updated_=false;
    if(backdrop && (backdrop->image.width!=256 || backdrop->image.height!=table_playfield_height ||
        backdrop->image.pixels.size()!=256*table_playfield_height || backdrop->sprites.size()!=256*table_playfield_height))
        throw std::runtime_error("Invalid transition ball backdrop");
    if(ball && (ball->width!=16 || ball->height!=16 || ball->image.pixels.size()!=256))
        throw std::runtime_error("Invalid transition ball sprite");
    if(table>1 || camera!=0 || (std::to_integer<unsigned>(ram[0x53])&0x80) ||
        (std::to_integer<unsigned>(ram[0x54])&0x80)) {reset();return live;}
    if(frame!=frame_+1) reset();
    const bool was_held=held_;
    held_=false;
    const bool changed=table_<=1 && table<=1 && table!=table_;
    frame_=frame; table_=table;
    const auto draw_ball=[&]() {
        auto result=backdrop_->image;
        // Preserve uncertain sprite occlusion pixel by pixel, without keeping
        // the old ball or reverting to its delayed SAT position after loading.
        for(int y=0;y<ball->height;++y) for(int x=0;x<ball->width;++x) {
            const int dx=ball->x+x,dy=ball->y+y;
            const auto color=ball->image.pixels[y*ball->width+x];
            if(!(color>>24) || dx<0 || dx>=result.width || dy<0 || dy>=result.height) continue;
            const auto p=static_cast<std::size_t>(dy)*result.width+dx;
            if(!backdrop_->sprites[p]) result.pixels[p]=color;
        }
        ball_updated_=true;
        return result;
    };
    if(live) {
        backdrop_=backdrop?std::optional<TableBallBackdrop>(*backdrop):std::nullopt;remaining_=0;
        image_=backdrop_ && ball?draw_ball():std::move(*live);
        return image_;
    }
    // A table-code change immediately after a complete image arms the hold.
    // Same-table unsupported scenes cannot start or indefinitely extend it.
    if(changed && image_ && !was_held) remaining_=32;
    else if(changed) remaining_=0;
    if(remaining_ && image_) {
        --remaining_;held_=true;
        if(backdrop_ && ball) {
            return draw_ball();
        }
        return image_;
    }
    image_.reset(); backdrop_.reset();
    return std::nullopt;
}
Image stitch_table_views(const Image& lower,const Image& upper,unsigned active_table,
    std::span<const std::uint8_t> lower_sprites,std::span<const std::uint8_t> upper_sprites) {
    for(const auto* view:{&lower,&upper})
        if(view->width!=256 || view->height<240 || view->height>256 ||
            view->pixels.size()!=static_cast<std::size_t>(view->width*view->height))
            throw std::runtime_error("Table stitching requires complete 256 by 240..256 views");
    if(active_table>1) throw std::runtime_error("Unknown main table half");
    if((!lower_sprites.empty() || !upper_sprites.empty()) &&
        (lower_sprites.size()!=256*240 || upper_sprites.size()!=256*240))
        throw std::runtime_error("Table sprite masks require two complete visible views");
    Image result{256,table_playfield_height,std::vector<std::uint32_t>(256*table_playfield_height)};
    auto copy=[&](const Image& view,int y) {
        std::copy_n(view.pixels.begin(),256*240,result.pixels.begin()+y*256);
    };
    copy(lower,table_half_offset); copy(upper,0);
    if(!lower_sprites.empty()) for(unsigned y=0;y<240-table_half_offset;++y) for(unsigned x=0;x<256;++x) {
        const auto lower_pixel=y*256+x,upper_pixel=(y+table_half_offset)*256+x;
        if(lower_sprites[lower_pixel] && (!upper_sprites[upper_pixel] || active_table==0))
            result.pixels[upper_pixel]=lower.pixels[lower_pixel];
    }
    return result;
}
TableAnimation::TableAnimation(std::span<const std::byte> rom) {
    if(rom.size()!=262144) throw std::runtime_error("Invalid table animation ROM");
    std::copy_n(rom.begin()+0x36000,palette_rom_.size(),palette_rom_.begin());
    std::copy_n(rom.begin()+0x1192,palette_tables_.size(),palette_tables_.begin());
    std::copy_n(rom.begin()+0x3c9d,actor_offsets_.size(),actor_offsets_.begin()); // 01:5C9D
}
void TableAnimation::update(const assets::ActorAssets& assets,std::span<const std::byte> ram) {
    if(ram.size()!=8192 || std::to_integer<unsigned>(ram[0x79])>1)
        throw std::runtime_error("Invalid table animation state");
    const unsigned active=std::to_integer<unsigned>(ram[0x79]);
    const auto byte=[&](unsigned p){return std::to_integer<unsigned>(ram[p]);};
    // 89CA / 8A21 change the viewport selector BEFORE E705 and 46FD
    // exchange the actor banks. During these calls, the banks still belong
    // to the old halves (or contain a partial field-by-field exchange).
    const unsigned caller=byte(0x1fc)|(byte(0x1fd)<<8);
    const bool exchanging=byte(0x1ff)==0x40 && byte(0x1fe)==(byte(0x76)?0x3a:0xa0) &&
        (caller==0x89cf || caller==0x89d2 || caller==0x8a2a || caller==0x8a2d);
    if(exchanging && !valid_) {
        // A cold restore has no coherent bank to show yet.
        for(auto& bank:actors_) bank.fill(std::byte{});
        for(auto& colors:color_owned_) colors.fill(false);
        return;
    }
    for(unsigned half=0;half<2;++half) {
        std::array<std::byte,8192> incoming;
        if(exchanging) incoming=source_[half];
        else std::copy(ram.begin(),ram.end(),incoming.begin());
        if(!exchanging && half!=active) for(unsigned field=0;field<32;++field)
            std::copy_n(ram.begin()+0x204+field*64,60,incoming.begin()+0x1504+field*64);
        incoming[0x79]=static_cast<std::byte>(half);
        incoming[0x2e]=std::byte{};incoming[0x2f]=std::byte{2};incoming[0x50]=std::byte{10};
        // Cabinet play runs the table's actors and palette scripts in both
        // halves (gameplay::CabinetActors), so each bank is drawn as it is.
        // Only the stored half's flippers follow the controls.
        auto& visual=actors_[half];
        visual=incoming;
        assets.mirror_flippers(visual,ram,half);
        source_[half]=incoming;

        // The colors each running script last uploaded, by its frame.
        color_owned_[half].fill(false);
        const auto b=[&](unsigned p){return std::to_integer<unsigned>(palette_tables_[p]);};
        const auto rom_color=[&](unsigned p) {
            return static_cast<std::uint16_t>(std::to_integer<unsigned>(palette_rom_[p])|
                ((std::to_integer<unsigned>(palette_rom_[p+1])&1)<<8));
        };
        for(unsigned x=half*32;x<(half+1)*32;++x) {
            const unsigned script=std::to_integer<unsigned>(visual[0x1245+x]);
            unsigned frame=std::to_integer<unsigned>(visual[0x13c5+x]);
            if(script==2) { // F4CA: a whole palette from the flash table at 1B:B81C.
                const unsigned offset=0x181c+frame*32;
                if(offset+32>palette_rom_.size()) continue;
                for(unsigned i=0;i<16;++i) {
                    colors_[half][(x%32)*16+i]=rom_color(offset+i*2);color_owned_[half][(x%32)*16+i]=true;
                }
                continue;
            }
            if(script<6 && script!=1) continue;
            if(script>11) continue;
            // Descending scripts may initialize at phase == count. F2E5
            // advances that sentinel before its first F463 upload. It is not
            // a ROM color frame: drawing it reads the next sequence's colors.
            // Keep the loaded palette until a real frame becomes available.
            if(script!=8 && frame>=std::to_integer<unsigned>(visual[0x1405+x])) continue;
            if(script==10) frame=(frame+std::to_integer<unsigned>(actor_offsets_[std::to_integer<unsigned>(ram[0xa8])&7]))&255;
            // F463 selects a packed ROM frame using F192/F1D2, F212/F252.
            const auto first=b(128+x),last=b(192+x);
            if(first>last || last>30 || (first&1) || (last&1)) continue;
            const unsigned bytes=last-first+2;
            const unsigned pointer=b(x)|(b(64+x)<<8);
            const unsigned offset=pointer+bytes*frame;
            if(offset<0xa000 || offset+bytes>0xe000) continue;
            for(unsigned i=0;i<bytes;i+=2) {
                const unsigned color=(x%32)*16+(first+i)/2;
                colors_[half][color]=rom_color(offset-0xa000+i);
                color_owned_[half][color]=true;
            }
        }
    }
    valid_=true;
}
void TableAnimation::apply_palette(unsigned half,std::span<std::uint16_t> palette) const {
    for(unsigned i=0;i<512;++i) if(color_owned_[half][i]) palette[i]=colors_[half][i];
}
TablePlayfield::TablePlayfield(std::span<const std::byte> rom)
    :tables_(assets::decode_table_backgrounds(rom)),sprites_(assets::decode_table_sprites(rom)),actors_(rom),
    indicators_(assets::decode_table_indicators(rom)),counter_(assets::decode_lower_counter_tiles(rom)),animation_(rom) {
    // E4A1 starts black, visits thresholds 21..1, and increments the channel
    // with the largest remaining distance (ties: red, green, blue). Recognize
    // only complete original palette states; do not guess a brightness scalar.
    for(unsigned table=0;table<2;++table) {
        std::array<std::uint16_t,512> palette{};
        partial_fades_[table].push_back(palette);
        for(unsigned threshold=21;threshold>1;--threshold) {
            palette=assets::advance_palette_fade(palette,tables_[table].palette,threshold);
            partial_fades_[table].push_back(palette);
        }
        // E5EC subtracts one from every nonzero RGB channel per pass.
        palette=tables_[table].palette;
        for(unsigned step=0;step<7;++step) {
            for(auto& color:palette) {
                unsigned next=0;
                for(unsigned shift:{0u,3u,6u}) {
                    const unsigned channel=(color>>shift)&7;
                    next|=(channel?channel-1:0)<<shift;
                }
                color=static_cast<std::uint16_t>(next);
            }
            partial_fades_[table].push_back(palette);
        }
    }
}
Image TablePlayfield::inactive_view(unsigned table,std::span<const std::byte> ram,std::span<std::uint8_t> visible,
    std::span<std::uint8_t> blockers,std::span<const std::uint16_t> palette,CompositionCapture* capture) const {
    if(table>1) throw std::runtime_error("Unknown inactive table");
    auto projected=assets::inactive_table_actor_ram(ram);
    projected[0x79]=static_cast<std::byte>(table);
    for(const auto address:{0x53,0x54,0x99,0x9a}) projected[address]=ram[address];
    auto vram=tables_[table].vram;
    for(const auto& write:assets::restored_actor_backgrounds(actors_,projected)) vram[write.address]=write.value;
    // The original return path restores indicator palettes after actor uploads.
    assets::restore_table_indicators(std::span(vram).first(1024),indicators_,table,ram);
    (void)assets::restore_lower_counter(std::span(vram).first(1024),counter_,table,ram);
    std::copy(sprites_[table].vram.begin()+0x4000,sprites_[table].vram.end(),vram.begin()+0x4000);
    // The lower arms replace six sprite patterns each. Recreate the stored
    // selection from ROM even when its one-shot upload bit has already cleared.
    for(unsigned actor=4;actor<64;++actor) {
        const auto type=std::to_integer<std::uint8_t>(projected[0x1500+actor]);
        if(type!=26 && type!=27) continue;
        const auto transfer=actors_.pattern_transfer(type,std::to_integer<std::uint8_t>(projected[0x1a80+actor]),0x200);
        std::copy(transfer.words.begin(),transfer.words.end(),vram.begin()+transfer.destination);
    }
    std::array<std::uint16_t,256> sat{};
    const auto pieces=assets::assemble_table_actors(actors_,projected);
    for(const auto& piece:pieces)
        std::copy(piece.words.begin(),piece.words.end(),sat.begin()+piece.slot*4);
    const unsigned next_slot=pieces.empty()?10:pieces.back().slot+1;
    for(const auto& piece:assets::assemble_table_overlay(projected,next_slot))
        std::copy(piece.words.begin(),piece.words.end(),sat.begin()+piece.slot*4);
    return render::compose(vram,palette.empty()?std::span<const std::uint16_t>(tables_[table].palette):palette,
        sat,{256,240,0,0,0,0xc0},visible,blockers,0,capture);
}
std::optional<Image> TablePlayfield::cabinet_frame(const diagnostics::ReferenceSnapshot& current,
    const diagnostics::ReferenceSnapshot* previous,std::span<const std::byte> ram,
    const TableRaster* raster,TableTransitionHold& transition) const {
    if(previous && current.frame==cabinet_frame_number_) return cabinet_image_;
    animation_scene_.reset();
    const bool frozen=cabinet_image_ && previous && (std::to_integer<unsigned>(ram[0xf6])&2) &&
        current.frame==animation_frame_+1 && animation_half_==1-std::to_integer<unsigned>(ram[0x79]);
    cabinet_image_=cabinet_frame_impl(current,previous,ram,raster,transition);
    const auto state=diagnostics::observe_alien_state(ram);
    if(!frozen && cabinet_image_ && state.match_visible && (current.control&0xc0)==0xc0) {
        // 03:831A..8342 writes these directly to reserved SAT slots, outside
        // the ordinary actor assembler. Keep them above the stitched table,
        // using live state and the match artwork loaded by the game (which
        // differs from the normal HUD font).
        std::array<std::uint16_t,256> digits_sat{};
        for(unsigned i=0;i<3;++i) {
            digits_sat[i*4+2]=static_cast<std::uint16_t>(40+(i==0?state.match_digit*2:0));
            digits_sat[i*4+3]=0x8f;
        }
        const auto digits=render::sprites(current.vram,current.palette,digits_sat,0);
        const unsigned y=112+(state.table_code==0?table_half_offset:0);
        for(unsigned i=0;i<3;++i) {
            const auto& digit=digits[i].image;
            if(animation_scene_) animation_scene_->layers.push_back({0x300000+i,{double(136+i*16),double(y)},digit,true});
            for(unsigned row=0;row<16;++row) for(unsigned col=0;col<16;++col) {
                const auto color=digit.pixels[row*16+col];
                if(color>>24) cabinet_image_->pixels[(y+row)*256+136+i*16+col]=color;
            }
        }
    }
    cabinet_frame_number_=current.frame;
    return cabinet_image_;
}
std::optional<Image> TablePlayfield::cabinet_frame_impl(const diagnostics::ReferenceSnapshot& current,
    const diagnostics::ReferenceSnapshot* previous,std::span<const std::byte> ram,
    const TableRaster* raster,TableTransitionHold& transition) const {
    const auto scene=diagnostics::observe_alien_state(ram);
    const unsigned half=scene.table_code;
    if(half>1 || scene.title_menu || scene.high_score_menu || scene.title_transition) {
        transition.reset();animation_half_=2;animation_.reset();return std::nullopt;
    }
    if(const auto fade=bonus_table_fade(ram)) {
        transition.reset();
        if((current.control&0xc0)!=0xc0)
            return Image{256,table_playfield_height,std::vector<std::uint32_t>(256*table_playfield_height,0xff000000)};
        // Palette uploads can straddle video frames. Select a complete ROM
        // fade step from the static background colors, then apply that same
        // step to both halves. Never reapply animated full-bright colors here.
        // Snapshots contain large VRAM arrays; keep this optional path off
        // the host's stack, including callers that already own snapshots.
        auto fade_current=std::make_unique<diagnostics::ReferenceSnapshot>(current);
        auto& active=*fade_current;
        unsigned best_error=~0u;
        const auto consider=[&](const auto& palette) {
            unsigned error=0;
            for(unsigned i=0;i<256;++i) for(unsigned shift:{0u,3u,6u}) {
                const int a=(palette[i]>>shift)&7,c=(current.palette[i]>>shift)&7;
                error+=static_cast<unsigned>(a>c?a-c:c-a);
            }
            if(error<best_error) {best_error=error;active.palette=palette;}
        };
        consider(tables_[half].palette);
        for(unsigned i=fade<0?21:0;i<(fade<0?28u:21u);++i) consider(partial_fades_[half][i]);
        auto latched=std::make_unique<diagnostics::ReferenceSnapshot>(active);latched->frame=active.frame-1;
        return compose_impl(active,*latched,half,0,ram,nullptr,false,nullptr,true);
    }
    // The original pause stops the actors (both halves' in cabinet play) but
    // its video/palette machinery still runs. Freeze the complete cabinet
    // image, including palette pulses.
    if((std::to_integer<unsigned>(ram[0xf6])&2) && previous &&
        current.frame==animation_frame_+1 && animation_half_==1-half && cabinet_image_) {
        animation_frame_=current.frame;
        return cabinet_image_;
    }
    // Start over only after a discontinuity (a restore or a skipped frame);
    // the banks carry everything else across a bank swap.
    const bool reseed=!previous || animation_half_>1 ||
        (current.frame!=animation_frame_ && current.frame!=animation_frame_+1);
    if(reseed) animation_.reset();
    animation_.update(actors_,ram);
    animation_half_=1-half;
    animation_frame_=current.frame;
    std::array<std::byte,8192> visual_ram;
    std::copy(ram.begin(),ram.end(),visual_ram.begin());
    for(unsigned field=0;field<32;++field) {
        std::copy_n(animation_.actors(1-half).begin()+0x1504+field*64,60,visual_ram.begin()+0x204+field*64);
        std::copy_n(animation_.actors(half).begin()+0x1504+field*64,60,visual_ram.begin()+0x1504+field*64);
    }
    ram=visual_ram;
    const unsigned camera=std::to_integer<unsigned>(ram[0x16])|(std::to_integer<unsigned>(ram[0x17])<<8);
    std::optional<Image> live;
    std::optional<TableBallBackdrop> backdrop;
    const auto ball=transition_ball(half,ram);
    if(previous && current.frame==previous->frame+1 && camera==0 && matches(current,half)) {
        // The drawn banks are already the destination half's. Its first
        // loaded frame need not hold the outgoing image.
        const auto* ready_raster=matches(*previous,half)?raster:nullptr;
        if(!ball) live=compose_impl(current,*previous,half,camera,ram,ready_raster,false,nullptr,true);
        if(ball || live) {
            // The transition presenter replaces the ball from this clean
            // backdrop anyway. Composing an additional ball-included image
            // first only doubled the most expensive work on each game tick.
            TableBallBackdrop clean;
            if(auto image=compose_impl(current,*previous,half,camera,ram,ready_raster,true,&clean.sprites,true)) {
                if(ball) live=*image;
                clean.image=std::move(*image);backdrop=std::move(clean);
            }
        }
    }
    live=transition.present(std::move(live),current.frame,half,camera,ram,
        backdrop?&*backdrop:nullptr,ball?&*ball:nullptr);
    if(live) return live;

    // Loading never changes the cabinet's projection. Respect the game's blank
    // display, then render its fade across both ROM table halves. No emulated
    // cycles, audio samples or gameplay updates are skipped.
    if((current.control&0xc0)!=0xc0)
        return Image{256,table_playfield_height,std::vector<std::uint32_t>(256*table_playfield_height,0xff000000)};
    auto active=current;
    const auto& asset=tables_[half];
    const bool loaded=std::equal(asset.vram.begin()+0x800,asset.vram.begin()+0x800+asset.pattern_words,
        current.vram.begin()+0x800);
    if(!loaded) {
        active.vram=asset.vram;
        active.sat.fill(0); // Do not interpret title/bonus sprites as table actors.
    }
    auto latched=active;
    latched.frame=active.frame-1;
    if(loaded && previous && previous->frame+1==current.frame) latched.sat=previous->sat;
    // The cabinet fixes the camera to the whole table. Frozen composition is
    // used only while the exact raster contract is unavailable (e.g. fade-in).
    const auto fallback=compose_impl(active,latched,half,0,ram,nullptr,false,nullptr,true);
    if(loaded && ball && fallback) {
        TableBallBackdrop clean;
        if(auto image=compose_impl(active,latched,half,0,ram,nullptr,true,&clean.sprites,true)) {
            clean.image=std::move(*image);
            return transition.present(fallback,current.frame,half,0,ram,&clean,&*ball);
        }
    }
    return fallback;
}
bool TablePlayfield::matches(const diagnostics::ReferenceSnapshot& video,unsigned table) const {
    const auto& asset=tables_[table];
    // The inactive half uses its settled ROM palette. Keep the original view
    // while the active half is in a known partial fade, including bonus returns.
    if(video.palette!=asset.palette && std::find(partial_fades_[table].begin(),partial_fades_[table].end(),video.palette)!=partial_fades_[table].end()) return false;
    return video.memory_width==0 && video.scroll_x==0 && video.scroll_y==0 &&
        (video.control&0xc0)==0xc0 &&
        std::equal(asset.vram.begin()+0x800,asset.vram.begin()+0x800+asset.pattern_words,video.vram.begin()+0x800);
}
std::optional<Image> TablePlayfield::compose(const diagnostics::ReferenceSnapshot& current,
    const diagnostics::ReferenceSnapshot& previous,unsigned table,unsigned camera,std::span<const std::byte> ram,
    const TableRaster* raster,std::optional<TableBallBackdrop>* backdrop) const {
    if(backdrop) backdrop->reset();
    auto result=compose_impl(current,previous,table,camera,ram,raster,false,nullptr);
    if(result && backdrop) {
        TableBallBackdrop clean;
        if(auto image=compose_impl(current,previous,table,camera,ram,raster,true,&clean.sprites)) {
            clean.image=std::move(*image); *backdrop=std::move(clean);
        }
    }
    return result;
}
std::optional<Sprite> TablePlayfield::transition_ball(unsigned table,std::span<const std::byte> ram,MotionPosition* subpixel) const {
    if(ram.size()!=8192) throw std::runtime_error("Transition ball requires 8192-byte RAM");
    auto b=[&](std::size_t p){return std::to_integer<unsigned>(ram[p]);};
    if(table>1 || b(0x1500)!=1 || (b(0x1540)&0xa0)!=0x80 || (b(0x1a00)&0xa0)) return std::nullopt;
    const auto id=b(0x1800);
    if(id!=0 && (id<155 || id>157)) return std::nullopt;
    const auto pieces=actors_.pieces(static_cast<std::uint8_t>(id));
    if(pieces.size()!=1 || pieces[0].flags!=0x80) return std::nullopt;
    const auto& piece=pieces[0];
    std::array<std::uint16_t,256> sat{};
    sat[0]=static_cast<std::uint16_t>((b(0x1680)|(b(0x16c0)<<8))+piece.y);
    sat[1]=static_cast<std::uint16_t>((b(0x15c0)|(b(0x1600)<<8))+piece.x);
    sat[2]=static_cast<std::uint16_t>(0x200+piece.pattern); sat[3]=piece.flags;
    auto ball=render::sprites(sprites_[table].vram,tables_[table].palette,sat,0)[0];
    // Physics changes local origins by 224, while the artwork overlaps by
    // 216. A half-dependent offset jumps by eight pixels at every crossing.
    // Map the shared world coordinate instead: smoothly compress those eight
    // pixels across world Y 256..320, preserving both native alignments beyond
    // the join. Fractional input avoids a new rounding step at each half change.
    const unsigned local_y=b(0x1680)|(b(0x16c0)<<8);
    const double world_y=local_y+b(0x1640)/256.0+(table==0?224:0);
    const double t=std::clamp((world_y-256.0)/64.0,0.0,1.0);
    const double correction=(224-table_half_offset)*t*t*(3-2*t);
    ball.y=static_cast<int>(std::floor(world_y-correction))+piece.y-64;
    // Interpolate the same raster endpoints as unsmoothed presentation. Stored
    // fractional residue must not displace a stationary ball in the launcher.
    if(subpixel) *subpixel={double(ball.x),double(ball.y)};
    return ball;
}
std::optional<Image> TablePlayfield::compose_impl(const diagnostics::ReferenceSnapshot& current,
    const diagnostics::ReferenceSnapshot& previous,unsigned table,unsigned camera,std::span<const std::byte> ram,
    const TableRaster* raster,bool hide_ball,std::vector<std::uint8_t>* coverage,bool cabinet) const {
    if(table>1 || current.frame!=previous.frame+1 || (!cabinet && (camera!=0 ||
        !matches(current,table) || !matches(previous,table)))) return std::nullopt;
    auto sat=previous.sat;
    auto active_vram=current.vram;
    auto active_palette=current.palette;
    const bool fading=cabinet && bonus_table_fade(ram)!=0;
    std::uint64_t mouth_slots=0;
    std::vector<assets::BackgroundWrite> visual_writes;
    std::array<bool,32768> visual_addresses{};
    if(cabinet) {
        // Draw the active half from the drawn banks too: while the game
        // exchanges them, they are the last coherent ones.
        std::array<std::byte,8192> projected;
        std::copy(ram.begin(),ram.end(),projected.begin());
        projected[0x16]=projected[0x17]=std::byte{};
        projected[0x2e]=std::byte{};projected[0x2f]=std::byte{2};projected[0x50]=std::byte{10};
        if(hide_ball) projected[0x1540]|=std::byte{0x20};
        sat.fill(0);
        const auto pieces=assets::assemble_table_actors(actors_,projected);
        for(const auto& piece:pieces) {
            std::copy(piece.words.begin(),piece.words.end(),sat.begin()+piece.slot*4);
            const auto type=std::to_integer<unsigned>(projected[0x1500+piece.actor]);
            // Upper-table swallow/return mouths: 02:684B, 6A0C, 6B8C,
            // and the tentacled mouth at 6D3C. Keep their ROM foreground
            // pieces in front of the ball, without restoring decorative occlusion.
            if((type==11 || type==12 || type==13 || type==34) &&
                (std::to_integer<unsigned>(projected[0x1540+piece.actor])&0x0c))
                mouth_slots|=std::uint64_t{1}<<piece.slot;
        }
        const unsigned next=pieces.empty()?10:pieces.back().slot+1;
        for(const auto& piece:assets::assemble_table_overlay(projected,next))
            std::copy(piece.words.begin(),piece.words.end(),sat.begin()+piece.slot*4);
        visual_writes=assets::restored_actor_backgrounds(actors_,projected);
        for(unsigned x=4;x<64;++x) {
            const auto type=std::to_integer<std::uint8_t>(projected[0x1500+x]);
            if(type!=26 && type!=27) continue;
            const auto transfer=actors_.pattern_transfer(type,std::to_integer<std::uint8_t>(projected[0x1a80+x]),0x200);
            for(unsigned i=0;i<transfer.words.size();++i)
                visual_writes.push_back({static_cast<std::uint16_t>(transfer.destination+i),transfer.words[i]});
        }
        for(const auto& write:visual_writes) {
            active_vram[write.address]=write.value;visual_addresses[write.address]=true;
        }
        assets::restore_table_indicators(std::span(active_vram).first(1024),indicators_,table,ram);
        if(!fading && matches(current,table)) animation_.apply_palette(table,active_palette);
    }
    std::uint64_t hidden=0;
    if(hide_ball) {
        unsigned found=0;
        for(unsigned i=0;i<64;++i) {
            auto* s=sat.data()+i*4;
            if(s[3]==0x80 && s[2]>=0x220 && s[2]<=0x226 && s[2]%2==0 &&
                s[0]>48 && s[0]<304 && s[1]>16 && s[1]<288) {
                ++found; hidden|=std::uint64_t{1}<<i;
            }
        }
        if(found!=1 && !cabinet) return std::nullopt;
    }
    // Recognize the recovered standard HUD by position, attributes and digit
    // pattern range. Suppress pixels without freeing their hardware fetch slots.
    for(unsigned i=0;i<64;++i) {
        auto* s=sat.data()+i*4;
        const bool digit=(s[2]>=40 && s[2]<=58 && s[2]%2==0);
        const bool score=s[0]==80 && s[1]>=48 && s[1]<=128 && (s[1]-48)%10==0;
        const bool ball=s[0]==96 && s[1]==264;
        if(s[3]==0x8f && digit && (cabinet || score || ball)) hidden|=std::uint64_t{1}<<i;
    }
    std::array<CompositionCapture,2> captures;
    const bool capture=cabinet && capture_animation_;
    Image live;
    std::vector<std::uint8_t> live_sprites(256*240),inactive_sprites(256*240);
    std::vector<std::uint8_t> active_blocks(coverage?256*240:0);
    if(raster) {
        // An incomplete observation must not silently become a frozen frame.
        if(raster->rows.size()!=240 || raster->vram.size()<32768) return std::nullopt;
        for(unsigned row=0;row<240;++row)
            if(raster->rows[row].row!=row) return std::nullopt;
        for(unsigned address=0;address<32768;++address) {
            const auto& write=raster->vram[address];
            if(write.row!=0 || write.target!=RasterTarget::vram || write.address!=address) return std::nullopt;
        }
        // The end-of-ball score moves out of the standard HUD while its digit
        // patterns can be reloaded during display. Row-boundary VRAM alone does
        // not describe the sprite fetch latch during that upload. Present the
        // original viewport for these frames until the upload has completed.
        const bool digit_upload=std::any_of(raster->vram.begin()+32768,raster->vram.end(),[](const auto& write) {
            return write.row!=0 && write.target==RasterTarget::vram && write.address>=0x500 && write.address<0x800;
        });
        if(digit_upload) for(unsigned i=0;i<64;++i) {
            const auto* s=sat.data()+i*4;
            if(!(hidden&(std::uint64_t{1}<<i)) && s[3]==0x8f && s[2]>=40 && s[2]<=58 && s[2]%2==0 &&
                s[0]>48 && s[0]<304 && s[1]>16 && s[1]<288) return std::nullopt;
        }
        auto writes=diagnostics::display_control_schedule(raster->rows,240);
        writes.insert(writes.end(),raster->vram.begin(),raster->vram.end());
        std::stable_sort(writes.begin(),writes.end(),[](const auto& a,const auto& b){return a.row<b.row;});
        auto palette=diagnostics::palette_pixel_schedule(previous.palette,current.palette,raster->palette,256,240);
        auto initial_palette=previous.palette;
        if(cabinet) {
            for(auto& write:writes) if(write.target==RasterTarget::vram && visual_addresses[write.address])
                write.value=active_vram[write.address];
            if(!fading && matches(current,table)) {
                animation_.apply_palette(table,initial_palette);
                for(auto& write:palette) if(animation_.owns_color(table,write.address)) write.value=active_palette[write.address];
            }
        }
        live=render::compose_palette_raster(active_vram,initial_palette,sat,{256,240,0,0,0,0xc0},writes,palette,live_sprites,active_blocks,hidden,capture?&captures[table]:nullptr);
    } else live=render::compose(active_vram,active_palette,sat,{256,240,0,0,0,0xc0},live_sprites,active_blocks,hidden,capture?&captures[table]:nullptr);
    std::vector<std::uint8_t> inactive_blocks(coverage?256*240:0);
    std::span<const std::uint16_t> inactive_palette;
    auto animated_palette=tables_[1-table].palette;
    if(cabinet) {
        const auto fade=std::find(partial_fades_[table].begin(),partial_fades_[table].end(),current.palette);
        if(fade!=partial_fades_[table].end())
            inactive_palette=partial_fades_[1-table][static_cast<std::size_t>(fade-partial_fades_[table].begin())];
        else {
            if(!fading) animation_.apply_palette(1-table,animated_palette);
            inactive_palette=animated_palette;
        }
    }
    const auto inactive=inactive_view(1-table,ram,inactive_sprites,inactive_blocks,inactive_palette,capture?&captures[1-table]:nullptr);
    if(coverage) {
        coverage->assign(256*table_playfield_height,0);
        const auto& lower=table==0?active_blocks:inactive_blocks;
        const auto& upper=table==0?inactive_blocks:active_blocks;
        std::copy(lower.begin(),lower.end(),coverage->begin()+256*table_half_offset);
        for(unsigned p=0;p<upper.size();++p) (*coverage)[p]|=upper[p];
        // Cabinet ball readability takes precedence over decorative sprites,
        // point labels and the PCE's per-row sprite fetch limit. Only swallowing
        // mouths retain occlusion. Keep the exact diagnostic mask unchanged.
        if(cabinet) {
            std::fill(coverage->begin(),coverage->end(),std::uint8_t{0});
            if(mouth_slots) {
                const auto mouths=render::sprites(active_vram,active_palette,sat,0);
                for(unsigned slot=0;slot<mouths.size();++slot) {
                    if(!(mouth_slots&(std::uint64_t{1}<<slot))) continue;
                    const auto& mouth=mouths[slot];
                    for(int y=0;y<mouth.height;++y) for(int x=0;x<mouth.width;++x) {
                        const int dx=mouth.x+x,dy=mouth.y+y;
                        if(dx<0 || dx>=256 || dy<0 || dy>=240 ||
                            !(mouth.image.pixels[static_cast<std::size_t>(y)*mouth.width+x]>>24)) continue;
                        (*coverage)[static_cast<std::size_t>(dy+(table==0?table_half_offset:0))*256+dx]=1;
                    }
                }
            }
        }
    }
    auto result=table==0?stitch_table_views(live,inactive,0,live_sprites,inactive_sprites):
        stitch_table_views(inactive,live,1,inactive_sprites,live_sprites);
    if(capture) {
        AnimationScene scene;
        scene.key=1;
        scene.background=stitch_table_views(captures[0].background,captures[1].background,table);
        for(unsigned half=0;half<2;++half) {
            auto projected=half==table?std::array<std::byte,8192>{}:assets::inactive_table_actor_ram(ram);
            if(half==table) std::copy(ram.begin(),ram.end(),projected.begin());
            else {
                projected[0x79]=static_cast<std::byte>(half);
                for(const auto address:{0x53,0x54,0x99,0x9a}) projected[address]=ram[address];
            }
            projected[0x16]=projected[0x17]=std::byte{};
            projected[0x2e]=std::byte{};projected[0x2f]=std::byte{2};projected[0x50]=std::byte{10};
            if(hide_ball) projected[0x1540]|=std::byte{0x20};
            const auto pieces=assets::assemble_table_actors(actors_,projected);
            std::array<std::uint16_t,256> attributes{};
            std::array<std::uint64_t,64> identities{};
            std::array<unsigned,64> ordinal{};
            std::array<MotionPosition,64> anchors{};
            std::array<bool,64> actor_piece{};
            std::array<bool,64> mouths{},flippers{};
            for(unsigned slot=0;slot<64;++slot) identities[slot]=0x100000+half*64+slot;
            for(const auto& piece:pieces) {
                std::copy(piece.words.begin(),piece.words.end(),attributes.begin()+piece.slot*4);
                const auto type=std::to_integer<unsigned>(projected[0x1500+piece.actor]);
                identities[piece.slot]=half*0x10000+piece.actor*256+ordinal[piece.actor]++;
                identities[piece.slot]|=static_cast<std::uint64_t>(type)<<32;
                const auto word=[&](unsigned lo,unsigned hi) {
                    return std::to_integer<unsigned>(projected[lo+piece.actor])|
                        (std::to_integer<unsigned>(projected[hi+piece.actor])<<8);
                };
                anchors[piece.slot]={double(static_cast<std::int16_t>(word(0x15c0,0x1600)))-32,
                    double(static_cast<std::int16_t>(word(0x1680,0x16c0)))-64};
                actor_piece[piece.slot]=true;
                flippers[piece.slot]=type==2 || type==3;
                mouths[piece.slot]=half==table && (type==11 || type==12 || type==13 || type==34) &&
                    (std::to_integer<unsigned>(projected[0x1540+piece.actor])&0x0c);
            }
            unsigned overlay_index=0;
            for(const auto& piece:assets::assemble_table_overlay(projected,pieces.empty()?10:pieces.back().slot+1)) {
                std::copy(piece.words.begin(),piece.words.end(),attributes.begin()+piece.slot*4);
                identities[piece.slot]=0x100000+half*64+overlay_index++;
            }
            // Only positions/dimensions are used; colors come from the exact raster.
            const auto decoded=animation_geometry(attributes);
            const auto& source=half==table?live:inactive;
            auto split=split_animation(source,captures[half],decoded,1);
            for(auto& layer:split.layers) {
                const auto slot=static_cast<unsigned>(layer.id);
                layer.id=identities[slot];layer.foreground=mouths[slot];layer.swings=flippers[slot];
                const int ox=static_cast<int>(layer.position.x),oy=static_cast<int>(layer.position.y);
                if(actor_piece[slot]) {
                    layer.pose_offset={layer.position.x-anchors[slot].x,layer.position.y-anchors[slot].y};
                    layer.actor_group=layer.id&~std::uint64_t{255};
                }
                for(int y=0;y<layer.image.height;++y) for(int x=0;x<layer.image.width;++x) {
                    const int dx=ox+x,dy=oy+y;
                    if(dx<0 || dx>=256 || dy<0 || dy>=240) continue;
                    bool visible=true;
                    if(half==0 && dy<240-table_half_offset)
                        visible=!captures[1].owner[(dy+table_half_offset)*256+dx] || table==0;
                    if(half==1 && dy>=table_half_offset)
                        visible=!captures[0].owner[(dy-table_half_offset)*256+dx] || table==1;
                    if(!visible) layer.image.pixels[y*layer.image.width+x]=0;
                }
                layer.position.y+=half==0?table_half_offset:0;
                scene.layers.push_back(std::move(layer));
            }
        }
        animation_scene_=std::move(scene);
    }
    // Flyers that leave their half across the seam: the slingshots' (52, 53)
    // rise into the upper table, and the brain's (55, 01:559C) fall into the
    // lower one, to local Y 320, before 559C removes them.
    if(cabinet) for(unsigned half=0;half<2;++half) {
        auto projected=half==table?std::array<std::byte,8192>{}:assets::inactive_table_actor_ram(ram);
        if(half==table) std::copy(ram.begin(),ram.end(),projected.begin());
        projected[0x1540]=std::byte{0x20};projected[0x50]=std::byte{};
        projected[0x16]=projected[0x17]=projected[0x2e]=std::byte{};projected[0x2f]=std::byte{2};
        bool any=false;
        for(unsigned x=4;x<64;++x) {
            const auto type=std::to_integer<unsigned>(projected[0x1500+x]);
            if(type!=52 && type!=53 && type!=55) {projected[0x1540+x]|=std::byte{0x20};continue;}
            any=true;
            // Translate BEFORE the SAT's ten-bit Y wrap. Negative local Y
            // belongs above the seam, not at the bottom of a wrapped viewport.
            if(!half) {
                const unsigned y=std::to_integer<unsigned>(projected[0x1680+x])|
                    (std::to_integer<unsigned>(projected[0x16c0+x])<<8);
                projected[0x1680+x]=static_cast<std::byte>((y+table_half_offset)&255);
                projected[0x16c0+x]=static_cast<std::byte>(((y+table_half_offset)>>8)&255);
            }
        }
        if(!any) continue;
        std::array<std::uint16_t,256> flight_sat{};
        const auto pieces=assets::assemble_table_actors(actors_,projected);
        for(const auto& piece:pieces)
            std::copy(piece.words.begin(),piece.words.end(),flight_sat.begin()+piece.slot*4);
        const std::span<const std::uint16_t> palette=half==table?active_palette:
            inactive_palette.empty()?std::span<const std::uint16_t>(tables_[half].palette):inactive_palette;
        const auto flight=render::sprites(sprites_[half].vram,palette,flight_sat,0);
        // The ordinary half composition already draws the local portion with
        // its correct ordering. Add only pixels clipped by that old viewport.
        // Smoothing moves these pieces with the rest of their actor: the same
        // group and anchor as its ordinary layers, one identity per piece.
        std::array<unsigned,64> ordinal{};
        for(auto it=pieces.rbegin();it!=pieces.rend();++it) {
            const auto& sprite=flight[it->slot];
            const auto word=[&](unsigned lo,unsigned hi) {
                return std::to_integer<unsigned>(projected[lo+it->actor])|(std::to_integer<unsigned>(projected[hi+it->actor])<<8);
            };
            const auto type=std::to_integer<unsigned>(projected[0x1500+it->actor]);
            const MotionPosition anchor{double(static_cast<std::int16_t>(word(0x15c0,0x1600)))-32,
                double(static_cast<std::int16_t>(word(0x1680,0x16c0)))-64};
            AnimationLayer extension{0x200000u+half*0x10000u+it->actor*256u+ordinal[it->actor]++,
                {double(sprite.x),double(sprite.y)},
                {sprite.width,sprite.height,std::vector<std::uint32_t>(sprite.image.pixels.size())}};
            extension.pose_offset={sprite.x-anchor.x,sprite.y-anchor.y};
            extension.actor_group=(half*0x10000u+it->actor*256u)|(static_cast<std::uint64_t>(type)<<32);
            bool drawn=false;
            for(int y=0;y<sprite.height;++y) for(int x=0;x<sprite.width;++x) {
                const int dx=sprite.x+x,dy=sprite.y+y;
                if(dx<0 || dx>=256 || dy<0 || dy>=table_playfield_height ||
                    (!half?dy>=table_half_offset:dy<240)) continue;
                const auto color=sprite.image.pixels[y*sprite.width+x];
                if(color>>24) {
                    result.pixels[dy*256+dx]=color;drawn=true;
                    if(capture) extension.image.pixels[y*sprite.width+x]=color;
                }
            }
            if(capture && drawn) animation_scene_->layers.push_back(std::move(extension));
        }
    }
    return result;
}
}
