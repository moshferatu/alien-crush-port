// SPDX-License-Identifier: GPL-3.0-or-later
// The cabinet's picture of both table halves: each actor bank drawn as the
// game holds it (the port runs the stored half, gameplay::CabinetActors),
// with rendering that never changes the game.
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
void preview(const render::Image& image,const std::string& name) {
    std::filesystem::create_directories("inactive-previews");
    std::ofstream out("inactive-previews/"+name+".bmp",std::ios::binary);
    const auto word=[&](unsigned value,unsigned bytes) {
        for(unsigned i=0;i<bytes;++i) out.put(static_cast<char>(value>>(8*i)));
    };
    out<<"BM";word(54+256*456*4,4);word(0,4);word(54,4);word(40,4);
    word(256,4);word(456,4);word(1,2);word(32,2);word(0,4);word(256*456*4,4);
    for(unsigned i=0;i<4;++i) word(0,4);
    for(int y=455;y>=0;--y) for(int x=0;x<256;++x) word(image.pixels[y*256+x],4);
    require(bool(out),"Cannot write animation preview");
}
struct Artwork {
    std::array<assets::TableBackground,2> backgrounds;
    std::array<assets::TableSprites,2> patterns;
    explicit Artwork(std::span<const std::byte> image)
        :backgrounds(assets::decode_table_backgrounds(image)),patterns(assets::decode_table_sprites(image)) {}
    // A loaded table's video, for composing a synthetic bank arrangement.
    void prepare(diagnostics::ReferenceSnapshot& snapshot,unsigned half,std::uint64_t frame) const {
        snapshot.frame=frame;snapshot.vram=backgrounds[half].vram;
        std::copy(patterns[half].vram.begin()+0x4000,patterns[half].vram.end(),snapshot.vram.begin()+0x4000);
        snapshot.palette=backgrounds[half].palette;snapshot.sat.fill(0);snapshot.control=0xc0;
        snapshot.memory_width=0;snapshot.scroll_x=snapshot.scroll_y=0;
    }
};
void swap_banks(std::array<std::byte,8192>& ram) {
    for(unsigned field=0;field<32;++field) for(unsigned x=4;x<64;++x)
        std::swap(ram[0x1500+field*64+x],ram[0x200+field*64+x]);
}
// The same banks, whichever half the ball is in, give the same picture:
// the actors and the palette scripts of both halves.
void seam_invariance(std::span<const std::byte> rom,const diagnostics::ReferenceSnapshot& video,std::span<const std::byte> ram) {
    const auto artwork=std::make_unique<Artwork>(rom);
    auto fixed=std::make_unique<render::TablePlayfield>(rom);
    auto crossing=std::make_unique<render::TablePlayfield>(rom);
    render::TableTransitionHold fixed_hold,crossing_hold;
    auto fixed_video=std::make_unique<diagnostics::ReferenceSnapshot>(video),crossing_video=std::make_unique<diagnostics::ReferenceSnapshot>(video);
    auto fixed_previous=std::make_unique<diagnostics::ReferenceSnapshot>(video),crossing_previous=std::make_unique<diagnostics::ReferenceSnapshot>(video);
    std::array<std::byte,8192> source{};
    std::copy(ram.begin(),ram.end(),source.begin());
    require(source[0x79]==std::byte{1},"Seam fixture requires the ball upstairs");
    source[0x53]=source[0x54]=source[0xf6]=std::byte{};
    source[0x1500]=source[0x1540]=std::byte{}; // Leave the ball out.
    // And the flippers, which follow the controls in the stored half.
    for(unsigned base:{0x1500u,0x200u}) for(unsigned x=4;x<64;++x) {
        const auto type=source[base+x];
        if(type==std::byte{2} || type==std::byte{3}) for(unsigned field=0;field<32;++field) source[base+field*64+x]=std::byte{};
    }
    for(unsigned tick=0;tick<60;++tick) {
        auto swapped=source;
        const unsigned half=(tick/7)%2;
        if(!half) {swap_banks(swapped);swapped[0x79]=std::byte{};}
        artwork->prepare(*fixed_video,1,3000+tick);artwork->prepare(*crossing_video,half,3000+tick);
        const auto stationary=fixed->cabinet_frame(*fixed_video,tick?fixed_previous.get():nullptr,source,nullptr,fixed_hold);
        const auto moving=crossing->cabinet_frame(*crossing_video,tick?crossing_previous.get():nullptr,swapped,nullptr,crossing_hold);
        require(stationary && moving,"Bank swap lost cabinet image");
        if(stationary->pixels!=moving->pixels) {
            preview(*stationary,"seam-expected");preview(*moving,"seam-actual");
            throw std::runtime_error("The picture depends on which half holds the ball, tick "+std::to_string(tick));
        }
        *fixed_previous=*fixed_video;*crossing_previous=*crossing_video;
    }
}
// Cocoons change their sprite patterns (01:54EE); the stored half's are
// drawn from its bank's pattern selection, hidden or not.
void cocoon_rendering(std::span<const std::byte> rom,const diagnostics::ReferenceSnapshot& video,std::span<const std::byte> ram) {
    require(ram[0x79]==std::byte{1},"Cocoon rendering requires ball in upper half");
    std::array<std::byte,8192> shown{};
    std::copy(ram.begin(),ram.end(),shown.begin());
    shown[0x53]=shown[0x54]=shown[0xf6]=std::byte{};
    unsigned found=0;
    for(unsigned x=4;x<64;++x) {
        const auto type=std::to_integer<unsigned>(shown[0x200+x]);
        if(type!=26 && type!=27) continue;
        ++found;shown[0x240+x]=std::byte{0x88};shown[0x840+x]=std::byte{};
        const auto pointer=std::to_integer<unsigned>(rom[0x9df7+type-1])|(std::to_integer<unsigned>(rom[0x9e51+type-1])<<8);
        shown[0xa80+x]=rom[pointer-0x2000+8];
    }
    require(found==2,"Expected both original cocoon actors");
    auto hidden=shown;
    for(unsigned x=4;x<64;++x) {
        const auto type=std::to_integer<unsigned>(hidden[0x200+x]);
        if(type==26 || type==27) hidden[0x240+x]|=std::byte{0x60};
    }
    const auto draw=[&](const std::array<std::byte,8192>& state) {
        auto table=std::make_unique<render::TablePlayfield>(rom);render::TableTransitionHold hold;
        const auto image=table->cabinet_frame(video,nullptr,state,nullptr,hold);
        require(image.has_value(),"Cocoon fixture lost cabinet view");
        return *image;
    };
    const auto with=draw(shown),without=draw(hidden);
    unsigned left=0,right=0;
    for(unsigned y=240;y<456;++y) for(unsigned x=0;x<256;++x)
        if(with.pixels[y*256+x]!=without.pixels[y*256+x]) ++(x<128?left:right);
    require(left>20 && right>20,"The stored half's cocoons are not drawn from their bank");
    preview(with,"cocoons-shown-upstairs");preview(without,"cocoons-hidden-upstairs");
}
// A slingshot flyer leaving the lower half keeps its sprite across the seam
// until the game removes it, 64 pixels above the lower viewport (00:FB03).
void slingshot_rendering(std::span<const std::byte> rom,const diagnostics::ReferenceSnapshot& video,std::span<const std::byte> ram) {
    const auto artwork=std::make_unique<Artwork>(rom);
    for(unsigned type:{52u,53u}) {
        auto current=std::make_unique<diagnostics::ReferenceSnapshot>(video);
        artwork->prepare(*current,0,4000);
        std::array<std::byte,8192> source{};
        std::copy(ram.begin(),ram.end(),source.begin());
        std::fill(source.begin()+0x1500,source.begin()+0x1d00,std::byte{});
        std::fill(source.begin()+0x200,source.begin()+0xa00,std::byte{});
        std::fill(source.begin()+0x1245,source.begin()+0x1445,std::byte{});
        source[0x79]=source[0x53]=source[0x54]=source[0xf6]=std::byte{};
        auto empty=source;
        source[0x1504]=static_cast<std::byte>(type);source[0x1544]=std::byte{0x84};
        source[0x15c4]=std::byte{160};source[0x1684]=std::byte{0xe0};source[0x16c4]=std::byte{0xff};
        source[0x1844]=std::byte{1};source[0x1a44]=std::byte{3};source[0x1b84]=std::byte{32};
        const unsigned pointer=std::to_integer<unsigned>(rom[0x9df7+type-1])|(std::to_integer<unsigned>(rom[0x9e51+type-1])<<8);
        source[0x1804]=rom[pointer-0x2000];
        auto flying=std::make_unique<render::TablePlayfield>(rom),blank=std::make_unique<render::TablePlayfield>(rom);
        render::TableTransitionHold flying_hold,blank_hold;
        const auto with=flying->cabinet_frame(*current,nullptr,source,nullptr,flying_hold);
        const auto without=blank->cabinet_frame(*current,nullptr,empty,nullptr,blank_hold);
        require(with && without,"Missing slingshot views");
        unsigned changed=0;
        for(unsigned p=0;p<256*render::table_half_offset;++p) changed+=with->pixels[p]!=without->pixels[p];
        require(changed>20,"Flying alien is not drawn above the seam");
        if(type==52) preview(*with,"slingshot-above-seam");

        // Smoothing: a frame later the flyer is a pixel further on in its
        // next pose. Each piece above the seam keeps its own identity and
        // moves with its actor's anchor, never with another piece's.
        auto smoothed=std::make_unique<render::TablePlayfield>(rom);smoothed->capture_animation(true);
        render::TableTransitionHold smoothed_hold;
        auto next=std::make_unique<diagnostics::ReferenceSnapshot>(*current);++next->frame;
        const std::array poses=type==52?std::array{32u,33u}:std::array{115u,116u};
        source[0x1804]=static_cast<std::byte>(poses[0]);
        require(smoothed->cabinet_frame(*current,nullptr,source,nullptr,smoothed_hold).has_value(),"Missing smoothed flyer");
        const auto before=*smoothed->animation_scene();
        source[0x1804]=static_cast<std::byte>(poses[1]);source[0x15c4]=std::byte{159};source[0x1684]=std::byte{0xdf};
        require(smoothed->cabinet_frame(*next,current.get(),source,nullptr,smoothed_hold).has_value(),"Missing smoothed flyer");
        const auto after=*smoothed->animation_scene();
        unsigned pieces=0;
        std::set<std::uint64_t> identities;
        for(const auto& layer:after.layers) {
            if(layer.id<0x200000 || layer.id>=0x300000) continue;
            ++pieces;
            require(identities.insert(layer.id).second,"Pieces above the seam share an identity");
            require(layer.actor_group!=0,"A piece above the seam is not part of its actor");
            const auto old=std::find_if(before.layers.begin(),before.layers.end(),[&](const auto& c){return c.actor_group==layer.actor_group;});
            require(old!=before.layers.end(),"A piece above the seam lost its actor across a pose change");
            require(layer.position.x-layer.pose_offset.x==old->position.x-old->pose_offset.x-1 &&
                layer.position.y-layer.pose_offset.y==old->position.y-old->pose_offset.y-1,
                "A piece above the seam does not move with its actor");
        }
        require(pieces>1,"The flyer above the seam has too few pieces for the check");
    }
}
// A brain flyer falls out of the upper half, to local Y 320 before 01:559C
// removes it, and keeps its sprite below the seam whichever half the ball is
// in: from the ball's half's bank, or from the stored one.
void brain_flyer_rendering(std::span<const std::byte> rom,const diagnostics::ReferenceSnapshot& video,std::span<const std::byte> ram) {
    const auto artwork=std::make_unique<Artwork>(rom);
    for(const bool upstairs:{true,false}) {
        auto current=std::make_unique<diagnostics::ReferenceSnapshot>(video);
        artwork->prepare(*current,upstairs?1:0,4000);
        std::array<std::byte,8192> source{};
        std::copy(ram.begin(),ram.end(),source.begin());
        std::fill(source.begin()+0x1500,source.begin()+0x1d00,std::byte{});
        std::fill(source.begin()+0x200,source.begin()+0xa00,std::byte{});
        std::fill(source.begin()+0x1245,source.begin()+0x1445,std::byte{});
        source[0x53]=source[0x54]=source[0xf6]=std::byte{};
        source[0x79]=static_cast<std::byte>(upstairs?1:0);
        auto empty=source;
        const unsigned bank=upstairs?0x1500:0x200;
        const auto field=[&](unsigned address,unsigned value) {source[bank+address-0x3500+4]=static_cast<std::byte>(value);};
        field(0x3500,55);field(0x3540,0x84);field(0x35c0,150);field(0x3680,290&255);field(0x36c0,290>>8);
        field(0x3a00,3);field(0x3b80,32);
        const unsigned pointer=std::to_integer<unsigned>(rom[0x9df7+55-1])|(std::to_integer<unsigned>(rom[0x9e51+55-1])<<8);
        field(0x3800,std::to_integer<unsigned>(rom[pointer-0x2000]));
        auto flying=std::make_unique<render::TablePlayfield>(rom),blank=std::make_unique<render::TablePlayfield>(rom);
        flying->capture_animation(true);
        render::TableTransitionHold flying_hold,blank_hold;
        const auto with=flying->cabinet_frame(*current,nullptr,source,nullptr,flying_hold);
        const auto without=blank->cabinet_frame(*current,nullptr,empty,nullptr,blank_hold);
        require(with && without,"Missing brain flyer views");
        unsigned changed=0;
        for(unsigned p=256*240;p<with->pixels.size();++p) changed+=with->pixels[p]!=without->pixels[p];
        require(changed>20,upstairs?"Brain flyer is cut off at the seam":"Stored brain flyer is cut off at the seam");
        if(upstairs) preview(*with,"brain-flyer-below-seam");
        const auto& layers=flying->animation_scene()->layers;
        require(std::any_of(layers.begin(),layers.end(),[](const auto& layer){
            return layer.id>=0x200000 && layer.id<0x300000 && layer.actor_group!=0;}),
            "A brain flyer's piece below the seam is not part of its actor");
    }
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected ROM path");
        const auto path=core::path_from_utf8(argv[1]);
        const auto rom=core::read_file(path);
        assets::ActorAssets actors(rom);
        // The stored half's flippers take the pose of the ball's half's.
        for(unsigned half=0;half<2;++half) for(unsigned flipper=2;flipper<=3;++flipper) for(unsigned pose=0;pose<4;++pose) {
            std::array<std::byte,8192> live{},other{};
            live[0x79]=static_cast<std::byte>(half);
            live[0x1504]=other[0x1504]=static_cast<std::byte>(flipper);
            live[0x1804]=rom[0x9eaf+(flipper-2)*8+half*4+pose];
            actors.mirror_flippers(other,live,1-half);
            require(other[0x1804]==rom[0x9eaf+(flipper-2)*8+(1-half)*4+pose],"Inactive flipper used the wrong table pose");
        }

        auto table=std::make_unique<render::TablePlayfield>(rom);
        auto palette_probe=std::make_unique<render::TableAnimation>(rom);
        auto runtime=make_reference_runtime(HandoffMode::continuous,ControlMode::original,1.0,{},ActorMode::cabinet);
        runtime->load(path);runtime->observe_display(true);
        const auto initial=runtime->save();
        std::vector<std::string> fingerprints;
        std::optional<diagnostics::ReferenceSnapshot> previous;
        std::optional<render::Image> last;
        std::array<std::byte,8192> last_ram{};
        std::array<unsigned,2> animated{};
        render::TableTransitionHold hold;
        unsigned ball_pixels=0,mouth_pixels=0;
        for(unsigned n=0;n<1800;++n) {
            const auto frame=runtime->advance(recording::canonical_input(n));
            fingerprints.push_back(recording::fingerprint(frame,runtime->work_ram()));
            const auto video=runtime->video_snapshot();const auto ram=runtime->work_ram();
            const auto before=runtime->save();
            if(n==399) {
                // Before the first visit, the stored upper half already has the
                // brain's five orbs: their controller (02:7626) runs there.
                require(ram[0x79]==std::byte{},"Initial orb check requires lower half");
                const auto stored=assets::inactive_table_actor_ram(ram);
                require(std::count(stored.begin()+0x1504,stored.begin()+0x1540,std::byte{19})==5,
                    "The stored upper half lacks the brain orbs before the first visit");
            }
            const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
            const auto image=table->cabinet_frame(video,previous?&*previous:nullptr,ram,&raster,hold);
            if(n==363 || n==364) {
                // A descending palette script can start at phase == count
                // (the red eyes). That sentinel is not a color frame: the
                // loaded colors stay until F2E5 steps to the last real one.
                palette_probe->reset();palette_probe->update(actors,ram);
                auto colors=video.palette;
                palette_probe->apply_palette(0,colors);
                if(n==363) {
                    require(ram[0x1245+18]==std::byte{1} && ram[0x13c5+18]==ram[0x1405+18],
                        "Missing red-eye palette startup sentinel");
                    for(unsigned color=288;color<304;++color) {
                        require(!palette_probe->owns_color(0,color),"Startup sentinel was treated as a drawable eye palette");
                        require(colors[color]==video.palette[color],"Startup sentinel replaced the loaded eye colors");
                    }
                } else {
                    require(ram[0x13c5+18]==std::byte{7},"Descending palette did not enter its last valid frame");
                    const unsigned pointer=std::to_integer<unsigned>(rom[0x1192+18])|(std::to_integer<unsigned>(rom[0x11d2+18])<<8);
                    const unsigned first=std::to_integer<unsigned>(rom[0x1212+18]);
                    const unsigned bytes=std::to_integer<unsigned>(rom[0x1252+18])-first+2;
                    for(unsigned i=0;i<bytes;i+=2) {
                        const unsigned offset=0x36000+pointer-0xa000+7*bytes+i;
                        const auto expected=std::to_integer<unsigned>(rom[offset])|((std::to_integer<unsigned>(rom[offset+1])&1)<<8);
                        require(colors[288+(first+i)/2]==expected,"First valid eye colors differ from ROM");
                    }
                    require(image.has_value(),"Startup eye regression lacks cabinet image");
                    unsigned white=0;
                    for(unsigned y=260;y<310;++y) for(unsigned x=95;x<175;++x) white+=image->pixels[y*256+x]==0xffffffff;
                    require(white<30,"Red eyes flashed white on the first palette activation frame");
                    preview(*image,"startup-eyes-fixed");
                }
            }
            const auto repeated=table->cabinet_frame(video,previous?&*previous:nullptr,ram,&raster,hold);
            require(!image || (repeated && repeated->pixels==image->pixels),"Rendering twice advanced the picture");
            require(runtime->save().core_state==before.core_state,"Presentation changed runtime state");
            if(image && hold.ball_updated()) {
                const auto ball=table->transition_ball(std::to_integer<unsigned>(ram[0x79]),ram);
                require(ball.has_value(),"Updated ball is missing");
                auto mouth_ram=std::array<std::byte,8192>{};
                std::copy(ram.begin(),ram.end(),mouth_ram.begin());
                mouth_ram[0x16]=mouth_ram[0x17]=std::byte{};
                mouth_ram[0x2e]=std::byte{};mouth_ram[0x2f]=std::byte{2};mouth_ram[0x50]=std::byte{10};
                std::array<std::uint16_t,256> mouth_sat{};
                for(const auto& piece:assets::assemble_table_actors(actors,mouth_ram)) {
                    const auto type=mouth_ram[0x1500+piece.actor];
                    if(type==std::byte{11} || type==std::byte{12} || type==std::byte{13} || type==std::byte{34})
                        std::copy(piece.words.begin(),piece.words.end(),mouth_sat.begin()+piece.slot*4);
                }
                const auto mouths=render::sprites(video.vram,video.palette,mouth_sat,0);
                for(int y=0;y<16;++y) for(int x=0;x<16;++x) {
                    const int dx=ball->x+x,dy=ball->y+y;
                    const auto color=ball->image.pixels[y*16+x];
                    if(!(color>>24) || dx<0 || dx>=256 || dy<0 || dy>=456) continue;
                    if(image->pixels[dy*256+dx]!=color) {
                        const int local_y=dy-(ram[0x79]==std::byte{}?render::table_half_offset:0);
                        const bool mouth=std::any_of(mouths.begin(),mouths.end(),[&](const auto& sprite){
                            const int mx=dx-sprite.x,my=local_y-sprite.y;
                            return mx>=0 && my>=0 && mx<sprite.width && my<sprite.height &&
                                (sprite.image.pixels[my*sprite.width+mx]>>24);
                        });
                        require(mouth,"Only original mouth artwork may obscure the ball");
                        if(!mouth_pixels) preview(*image,"mouth-occlusion-fixed");
                        ++mouth_pixels;
                    }
                    ++ball_pixels;
                }
            }
            if(n==999) {
                seam_invariance(rom,video,ram);
                cocoon_rendering(rom,video,ram);
                slingshot_rendering(rom,video,ram);
                brain_flyer_rendering(rom,video,ram);
                require(ram[0x79]==std::byte{1} && image,"Overlap fixture requires active upper table");
                // Move only a private ball copy across the reported occluders.
                // This includes artwork on the other side of the stitch.
                const auto original_ball=table->transition_ball(std::to_integer<unsigned>(ram[0x79]),ram);
                require(original_ball.has_value(),"Missing ball for overlap checks");
                const std::array targets{std::pair{108,250},std::pair{25,280},std::pair{113,343}};
                for(unsigned target=0;target<targets.size();++target) {
                    auto overlap_ram=last_ram;std::copy(ram.begin(),ram.end(),overlap_ram.begin());
                    for(unsigned axis=0;axis<2;++axis) {
                        const unsigned p=axis?0x1680:0x15c0;
                        const int value=std::to_integer<int>(ram[p])+(std::to_integer<int>(ram[p+64])<<8)+
                            (axis?targets[target].second-original_ball->y:targets[target].first-original_ball->x);
                        overlap_ram[p]=static_cast<std::byte>(value&255);overlap_ram[p+64]=static_cast<std::byte>((value>>8)&255);
                    }
                    auto overlap_table=std::make_unique<render::TablePlayfield>(rom);
                    render::TableTransitionHold overlap_hold;
                    const auto overlap=overlap_table->cabinet_frame(video,&*previous,overlap_ram,&raster,overlap_hold);
                    const auto test_ball=overlap_table->transition_ball(std::to_integer<unsigned>(ram[0x79]),overlap_ram);
                    require(overlap && test_ball && overlap_hold.ball_updated(),"Overlap view did not replace delayed ball");
                    for(int y=0;y<16;++y) for(int x=0;x<16;++x) {
                        const auto color=test_ball->image.pixels[y*16+x];
                        if(color>>24) require(overlap->pixels[(test_ball->y+y)*256+test_ball->x+x]==color,"Reported table decoration still obscures ball");
                    }
                    preview(*overlap,"overlap-"+std::to_string(target));
                }
                auto paused_table=std::make_unique<render::TablePlayfield>(rom);
                std::array<std::byte,8192> paused_ram;
                std::copy(ram.begin(),ram.end(),paused_ram.begin());paused_ram[0xf6]|=std::byte{2};
                auto paused_video_storage=std::make_unique<diagnostics::ReferenceSnapshot>(video);
                auto paused_previous_storage=std::make_unique<diagnostics::ReferenceSnapshot>(video);
                auto& paused_video=*paused_video_storage;auto& paused_previous=*paused_previous_storage;
                --paused_previous.frame;
                render::TableTransitionHold paused_hold;
                std::optional<render::Image> paused_image;
                for(unsigned i=0;i<16;++i) {
                    // Without the frame's palette writes: they replay from its start.
                    const auto next=paused_table->cabinet_frame(paused_video,&paused_previous,paused_ram,nullptr,paused_hold);
                    require(next.has_value(),"Pause lost cabinet presentation");
                    if(paused_image) require(next->pixels==paused_image->pixels,"The picture changed during pause");
                    paused_image=next;paused_previous=paused_video;++paused_video.frame;
                }
                auto cold_table=std::make_unique<render::TablePlayfield>(rom);
                render::TableTransitionHold cold_hold;
                const auto cold=cold_table->cabinet_frame(video,nullptr,ram,nullptr,cold_hold);
                const auto reset=paused_table->cabinet_frame(video,nullptr,ram,nullptr,paused_hold);
                require(cold && reset && cold->pixels==reset->pixels,"Cold view retained stale history");
                // The real Run pause: the game's actors, both halves', stop.
                const auto checkpoint=runtime->save();
                auto real_pause_table=std::make_unique<render::TablePlayfield>(rom);
                render::TableTransitionHold real_pause_hold;
                auto real_previous_storage=std::make_unique<diagnostics::ReferenceSnapshot>(video);
                auto& real_previous=*real_previous_storage;
                std::optional<render::Image> frozen;
                std::array<std::byte,8192> frozen_ram{};
                for(unsigned tick=0;tick<40;++tick) {
                    runtime->advance({static_cast<std::uint16_t>(tick==0?Pad::run:0),0});
                    const auto paused=runtime->video_snapshot();const auto state=runtime->work_ram();
                    const render::TableRaster paused_raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
                    const auto rendered=real_pause_table->cabinet_frame(paused,&real_previous,state,&paused_raster,real_pause_hold);
                    require((state[0xf6]&std::byte{2})!=std::byte{},"Real Run did not pause");
                    require(rendered.has_value(),"Real pause lost stitched table");
                    if(tick>=4) {
                        if(frozen) {
                            require(std::equal(state.begin()+0x1500,state.begin()+0x1d00,frozen_ram.begin()+0x1500) &&
                                std::equal(state.begin()+0x204,state.begin()+0xa00,frozen_ram.begin()+0x204),
                                "Game actors animate during pause");
                            if(rendered->pixels!=frozen->pixels) {
                                preview(*frozen,"pause-before");preview(*rendered,"pause-after");
                                throw std::runtime_error("Table pixels changed during real pause at tick "+std::to_string(tick));
                            }
                        }
                        frozen=rendered;std::copy(state.begin(),state.end(),frozen_ram.begin());
                    }
                    real_previous=paused;
                }
                bool resumed_motion=false;
                for(unsigned tick=0;tick<60;++tick) {
                    runtime->advance({static_cast<std::uint16_t>(tick==0?Pad::run:0),0});
                    const auto resumed=runtime->video_snapshot();
                    const render::TableRaster resumed_raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
                    const auto rendered=real_pause_table->cabinet_frame(resumed,&real_previous,runtime->work_ram(),&resumed_raster,real_pause_hold);
                    if(rendered && rendered->pixels!=frozen->pixels) resumed_motion=true;
                    real_previous=resumed;
                }
                require(resumed_motion && (runtime->work_ram()[0xf6]&std::byte{2})==std::byte{},
                    "Unpause did not resume table animation");
                runtime->restore(checkpoint);
            }
            if(image && (n==399 || n==419 || n==999 || n==1019 || (n>=1399 && n<=1406))) {
                preview(*image,std::to_string(n+1));
            }
            // The stored half is drawn moving: its own actors, not a copy.
            const auto half=std::to_integer<unsigned>(ram[0x79]);
            if(n>360 && image && last && half<2 && ram[0x79]==last_ram[0x79]) {
                const unsigned start=half==0?0:240*256,end=half==0?216*256:456*256;
                if(!std::equal(image->pixels.begin()+start,image->pixels.begin()+end,last->pixels.begin()+start)) ++animated[1-half];
            }
            last=image;std::copy(ram.begin(),ram.end(),last_ram.begin());previous=video;
        }
        require(animated[0]>50 && animated[1]>50,"Both stored halves must be drawn moving");
        require(ball_pixels>10000,"Insufficient ball layering coverage");
        require(mouth_pixels>0,"Natural replay must exercise mouth occlusion");
        runtime->restore(initial);runtime->observe_display(false);
        for(unsigned n=0;n<1800;++n) {
            const auto frame=runtime->advance(recording::canonical_input(n));
            require(recording::fingerprint(frame,runtime->work_ram())==fingerprints[n],"Presentation changed RAM, video or audio replay");
        }
        std::cout<<"PASS: stored lower/upper halves drawn moving on "<<animated[0]<<'/'<<animated[1]
            <<" frames; "<<ball_pixels<<" unobscured ball pixels; 1800 exact RAM/video/audio frames; repeated rendering is idempotent\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
