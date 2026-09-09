// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <memory>
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
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected ROM path");
        const auto path=core::path_from_utf8(argv[1]);
        const auto rom=core::read_file(path);
        assets::ActorAssets actors(rom);
        // Type 33 initializes six frames at +$0020 (01:5B15..5B25).
        // Validate both FD35 reflection endpoints and the recovered sequence.
        std::array<std::byte,8192> projected{};
        const unsigned slot=4,type=33;
        const auto sequence=std::to_integer<unsigned>(rom[0x9df7+type-1])|
            (std::to_integer<unsigned>(rom[0x9e51+type-1])<<8);
        projected[0x1500+slot]=std::byte{type};projected[0x1540+slot]=std::byte{0x88};
        projected[0x1840+slot]=std::byte{1};projected[0x1a40+slot]=std::byte{6};
        projected[0x1b80+slot]=std::byte{0x20};projected[0x1800+slot]=rom[sequence-0x2000];
        for(unsigned i=0;i<8;++i) actors.advance_animation(projected);
        require(projected[0x1800+slot]==rom[sequence-0x2000+1],"ROM animation did not advance at its original rate");
        for(unsigned i=8;i<48;++i) actors.advance_animation(projected);
        require(projected[0x1c40+slot]==std::byte{5} && projected[0x1bc0+slot]==std::byte{255},"Upper animation reflection differs from FD35");
        for(unsigned i=0;i<40;++i) actors.advance_animation(projected);
        require(projected[0x1c40+slot]==std::byte{0} && projected[0x1bc0+slot]==std::byte{0},"Lower animation reflection differs from FD35");
        projected[0x1840+slot]=std::byte{0};const auto idle=projected;
        actors.advance_animation(projected);require(projected==idle,"Animation woke a collision-triggered idle actor");
        for(unsigned orb_type:{70u,71u}) {
            std::array<std::byte,8192> orb{};
            orb[0x1504]=static_cast<std::byte>(orb_type);orb[0x1544]=std::byte{0xe0};
            orb[0x1884]=std::byte{120};orb[0x1684]=std::byte{160};
            orb[0x1b44]=std::byte{0xe0};orb[0x19c4]=std::byte{1};orb[0x1804]=std::byte{42};
            for(unsigned i=0;i<119;++i) actors.advance_animation(orb);
            require((orb[0x1544]&std::byte{0x20})!=std::byte{},"Orb respawn timer ended early");
            actors.advance_animation(orb);
            require(orb[0x1844]==std::byte{1} && orb[0x1544]==std::byte{0x80},"Hidden orb never respawned");
            unsigned moved=0;
            for(unsigned i=0;i<600;++i) {
                actors.advance_animation(orb);
                const auto y=std::to_integer<unsigned>(orb[0x1684]);
                require(y>=150 && y<=160 && orb[0x16c4]==std::byte{},"Cosmetic orb drifted out of its bobbing range");
                if(y!=160) ++moved;
            }
            require(moved>100,"Respawned orb did not bob");
        }
        std::array<std::byte,8192> brain_orb{};
        brain_orb[0x1504]=std::byte{19};brain_orb[0x1544]=std::byte{0x80};brain_orb[0x1a04]=std::byte{0x16};
        brain_orb[0x1884]=std::byte{1};brain_orb[0x15c4]=std::byte{50};brain_orb[0x1684]=std::byte{100};
        unsigned brain_moves=0;
        for(unsigned i=0;i<600;++i) {
            const auto old=brain_orb;actors.advance_animation(brain_orb);
            if(old[0x1584]!=brain_orb[0x1584]) ++brain_moves;
            const auto x=std::to_integer<unsigned>(brain_orb[0x15c4]);
            require(x>=48 && x<=52 && brain_orb[0x1604]==std::byte{} && brain_orb[0x1684]==std::byte{100},
                "Brain orb wobble drifted away from its anchor");
        }
        require(brain_moves>30 && brain_moves<300,"Brain orb must alternate wobble bursts with substantial rests");
        brain_orb[0x1844]=std::byte{2};brain_orb[0x1a04]=std::byte{0x5f};
        brain_orb[0x1b44]=std::byte{0xe0};brain_orb[0x19c4]=std::byte{1};
        for(unsigned i=0;i<32;++i) actors.advance_animation(brain_orb);
        require(brain_orb[0x1844]==std::byte{} && brain_orb[0x1684]==std::byte{100} &&
            brain_orb[0x1a04]==std::byte{0x16},"Saved brain orb hop did not land and resume horizontal motion");
        std::array<std::byte,8192> orange{};
        orange[0x1504]=std::byte{15};orange[0x1544]=std::byte{0xc0};orange[0x1844]=std::byte{1};
        orange[0x1884]=std::byte{1};orange[0x15c4]=std::byte{210};orange[0x1684]=std::byte{100};
        actors.advance_animation(orange);
        for(unsigned i=0;i<32;++i) actors.advance_animation(orange);
        require(orange[0x15c4]==std::byte{202} && orange[0x1684]==std::byte{108},"Orange alien did not emerge diagonally");
        for(unsigned i=0;i<212;++i) actors.advance_animation(orange);
        require(orange[0x15c4]==std::byte{210} && orange[0x1684]==std::byte{100} && orange[0x1844]==std::byte{1},"Orange alien did not return to its starting position");
        std::array<std::byte,8192> explosion{};
        explosion[0x1504]=std::byte{60};explosion[0x1544]=std::byte{0xc4};
        explosion[0x1a44]=std::byte{3};explosion[0x1b84]=std::byte{0x40};explosion[0x1804]=std::byte{158};
        for(unsigned i=0;i<20;++i) actors.advance_animation(explosion);
        require(explosion[0x1504]==std::byte{} && explosion[0x1544]==std::byte{},"Saved orb explosion did not finish");
        for(unsigned half=0;half<2;++half) for(unsigned flipper=2;flipper<=3;++flipper) for(unsigned pose=0;pose<4;++pose) {
            std::array<std::byte,8192> live{},other{};
            live[0x79]=static_cast<std::byte>(half);
            live[0x1504]=other[0x1504]=static_cast<std::byte>(flipper);
            live[0x1804]=rom[0x9eaf+(flipper-2)*8+half*4+pose];
            actors.mirror_flippers(other,live,1-half);
            require(other[0x1804]==rom[0x9eaf+(flipper-2)*8+(1-half)*4+pose],"Inactive flipper used the wrong table pose");
        }

        auto table=std::make_unique<render::TablePlayfield>(rom);
        auto runtime=make_reference_runtime(HandoffMode::continuous);runtime->load(path);runtime->observe_display(true);
        const auto initial=runtime->save();
        std::vector<std::string> fingerprints;
        std::optional<diagnostics::ReferenceSnapshot> previous;
        std::optional<render::Image> last;
        std::array<std::byte,8192> last_ram{};
        std::array<unsigned,2> animated{};
        render::TableTransitionHold hold;
        unsigned ball_pixels=0,orb_frames=0,brain_frames=0;
        for(unsigned n=0;n<1800;++n) {
            const auto frame=runtime->advance(recording::canonical_input(n));
            fingerprints.push_back(recording::fingerprint(frame,runtime->work_ram()));
            const auto video=runtime->video_snapshot();const auto ram=runtime->work_ram();
            const auto before=runtime->save();
            if(n==399) {
                auto dormant=assets::inactive_table_actor_ram(ram);
                const auto original=dormant;
                // A visible skull saved when the ball falls to the lower half
                // must consume the pending reset visually, before revisiting it.
                auto skull=original;
                unsigned skull_slot=64;
                for(unsigned a=4;a<64;++a) if(skull[0x1500+a]==std::byte{14}) skull_slot=a;
                require(skull_slot<64,"Upper table fixture lacks skull guard");
                skull[0x1540+skull_slot]=std::byte{0x88};
                skull[0x1840+skull_slot]=std::byte{2};
                auto event_ram=last_ram;std::copy(ram.begin(),ram.end(),event_ram.begin());
                event_ram[0x70]&=std::byte{0xfd};
                const auto visible_skull=skull;
                const auto visible_pieces=assets::assemble_table_actors(actors,skull);
                require(std::any_of(visible_pieces.begin(),visible_pieces.end(),[&](const auto& p){return p.actor==skull_slot;}),
                    "Skull reset fixture never displayed the skull");
                assets::apply_inactive_table_events(skull,event_ram,1);
                require(skull==visible_skull,"Skull disappeared without a drain reset");
                event_ram[0x70]|=std::byte{2};const auto event_before=event_ram;
                assets::apply_inactive_table_events(skull,event_ram,0);
                require(skull==visible_skull,"Upper skull reset affected lower projection");
                assets::apply_inactive_table_events(skull,event_ram,1);
                require(skull[0x1840+skull_slot]==std::byte{} && skull[0x1540+skull_slot]==std::byte{0xe8},
                    "Drained skull remained visible in dormant upper bank");
                require(event_ram==event_before,"Presentation consumed real pending skull reset");
                const auto pieces=assets::assemble_table_actors(actors,skull);
                require(std::none_of(pieces.begin(),pieces.end(),[&](const auto& p){return p.actor==skull_slot;}),
                    "Reset skull still emits sprites");
                for(unsigned field=0;field<32;++field)
                    std::copy_n(visible_skull.begin()+0x1504+field*64,60,event_ram.begin()+0x204+field*64);
                event_ram[0x70]&=std::byte{0xfd};
                const auto before_drain=table->inactive_view(1,event_ram);
                event_ram[0x70]|=std::byte{2};
                const auto after_drain=table->inactive_view(1,event_ram);
                require(before_drain.pixels!=after_drain.pixels,"Inactive skull reset did not reach rendered table");
                require(ram[0x79]==std::byte{},"Initial orb check requires lower half");
                require(std::count(dormant.begin()+0x1504,dormant.begin()+0x1540,std::byte{19})==0,"Initial fixture already has brain orbs");
                actors.seed_brain_orbs(dormant,0);require(dormant==original,"Lower half seeded upper orbs");
                actors.seed_brain_orbs(dormant,1);
                unsigned count=0;
                for(unsigned a=4;a<64;++a) if(dormant[0x1500+a]==std::byte{19}) {
                    constexpr unsigned coordinates[]{0x15c0,0x1600,0x1680,0x16c0};
                    for(unsigned f=0;f<4;++f) require(dormant[coordinates[f]+a]==rom[0x56b6+count*4+f],"Initial brain orb is not at its ROM position");
                    require(dormant[0x1540+a]==std::byte{0x80},"Initial brain orb is invisible");
                    ++count;
                }
                require(count==5,"Initial cabinet view lacks the five brain orbs");
                const auto seeded=dormant;actors.seed_brain_orbs(dormant,1);
                require(dormant==seeded,"Initial brain orbs were duplicated");
                unsigned mixed_frames=0,rest_frames=0;
                for(unsigned tick=0;tick<1200;++tick) {
                    actors.advance_animation(dormant);
                    unsigned moving=0;
                    for(unsigned a=4;a<64;++a) if(dormant[0x1500+a]==std::byte{19} && dormant[0x1840+a]!=std::byte{}) ++moving;
                    if(moving>0 && moving<5) ++mixed_frames;
                    if(!moving) ++rest_frames;
                }
                require(mixed_frames>300 && rest_frames>20,"Brain orb group shakes continuously instead of taking separate rests");
                auto waiting=original;
                for(unsigned a=4;a<64;++a) if(waiting[0x1500+a]==std::byte{66}) {
                    waiting[0x1880+a]=std::byte{44};waiting[0x18c0+a]=std::byte{1};
                }
                const auto respawning=waiting;actors.seed_brain_orbs(waiting,1);
                require(waiting==respawning,"Later cleared orb group was recreated early");
            }
            const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
            const auto image=table->cabinet_frame(video,previous?&*previous:nullptr,ram,&raster,hold);
            const auto repeated=table->cabinet_frame(video,previous?&*previous:nullptr,ram,&raster,hold);
            require(!image || (repeated && repeated->pixels==image->pixels),"Rendering twice advanced inactive animation");
            require(runtime->save().core_state==before.core_state,"Presentation changed runtime state");
            if(image && hold.ball_updated()) {
                const auto ball=table->transition_ball(std::to_integer<unsigned>(ram[0x79]),ram);
                require(ball.has_value(),"Updated ball is missing");
                for(int y=0;y<16;++y) for(int x=0;x<16;++x) {
                    const int dx=ball->x+x,dy=ball->y+y;
                    const auto color=ball->image.pixels[y*16+x];
                    if(!(color>>24) || dx<0 || dx>=256 || dy<0 || dy>=456) continue;
                    require(image->pixels[dy*256+dx]==color,"Cabinet decoration or sprite fetch limit obscured the ball");
                    ++ball_pixels;
                }
            }
            if(n>360 && ram[0x79]==last_ram[0x79]) {
                auto predicted=last_ram;actors.advance_animation(predicted);
                for(unsigned a=4;a<64;++a) {
                    const auto t=last_ram[0x1500+a];
                    if(t==std::byte{19} && ram[0x1500+a]==t && last_ram[0x1840+a]!=std::byte{} &&
                        (last_ram[0x1ac0+a]&std::byte{0x80})==std::byte{} &&
                        (last_ram[0x1580+a]!=ram[0x1580+a] || last_ram[0x1640+a]!=ram[0x1640+a])) {
                        for(unsigned field:{0x1580u,0x15c0u,0x1600u,0x1640u,0x1680u,0x16c0u,0x1700u,0x1740u,0x1780u,0x17c0u})
                            require(predicted[field+a]==ram[field+a],"Brain orb movement differs from original actor update");
                        ++brain_frames;
                    }
                    if((t!=std::byte{70} && t!=std::byte{71}) || ram[0x1500+a]!=t ||
                        last_ram[0x1840+a]!=std::byte{1} || (last_ram[0x1ac0+a]&std::byte{0x80})!=std::byte{} ||
                        ram[0x1840+a]==std::byte{}) continue;
                    if(last_ram[0x1640+a]==ram[0x1640+a] && last_ram[0x1680+a]==ram[0x1680+a] &&
                        last_ram[0x1780+a]==ram[0x1780+a] && last_ram[0x17c0+a]==ram[0x17c0+a]) continue;
                    for(unsigned field:{0x1640u,0x1680u,0x16c0u,0x1780u,0x17c0u,0x1b40u,0x1800u,0x1840u})
                        if(predicted[field+a]!=ram[field+a]) throw std::runtime_error("Orb visual motion differs: frame "+std::to_string(n+1)+
                            " slot "+std::to_string(a)+" field "+std::to_string(field)+" predicted "+std::to_string(std::to_integer<unsigned>(predicted[field+a]))+
                            " actual "+std::to_string(std::to_integer<unsigned>(ram[field+a])));
                    ++orb_frames;
                }
            }
            if(n==999) {
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
                auto paused_video=video,paused_previous=video;--paused_previous.frame;
                render::TableTransitionHold paused_hold;
                std::optional<render::Image> paused_image;
                for(unsigned i=0;i<16;++i) {
                    const auto next=paused_table->cabinet_frame(paused_video,&paused_previous,paused_ram,&raster,paused_hold);
                    require(next.has_value(),"Pause lost cabinet presentation");
                    if(paused_image) require(next->pixels==paused_image->pixels,"Inactive animation advanced during pause");
                    paused_image=next;paused_previous=paused_video;++paused_video.frame;
                }
                auto cold_table=std::make_unique<render::TablePlayfield>(rom);
                render::TableTransitionHold cold_hold;
                const auto cold=cold_table->cabinet_frame(video,nullptr,ram,&raster,cold_hold);
                const auto reset=paused_table->cabinet_frame(video,nullptr,ram,&raster,paused_hold);
                require(cold && reset && cold->pixels==reset->pixels,"Cold view retained stale animation history");
                // Exercise the real Run pause path, including original active
                // actor scheduling, rather than relying only on a synthetic flag.
                const auto checkpoint=runtime->save();
                auto real_pause_table=std::make_unique<render::TablePlayfield>(rom);
                render::TableTransitionHold real_pause_hold;
                auto real_previous=video;
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
                            require(std::equal(state.begin()+0x1500,state.begin()+0x1d00,frozen_ram.begin()+0x1500),
                                "Original game actors animate during pause");
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
            const auto half=std::to_integer<unsigned>(ram[0x79]);
            if(n>360 && image && last && half<2 && ram[0x79]==last_ram[0x79] &&
                std::equal(ram.begin()+0x204,ram.begin()+0xa00,last_ram.begin()+0x204)) {
                const unsigned start=half==0?0:240*256,end=half==0?216*256:456*256;
                if(!std::equal(image->pixels.begin()+start,image->pixels.begin()+end,last->pixels.begin()+start)) ++animated[1-half];
            }
            last=image;std::copy(ram.begin(),ram.end(),last_ram.begin());previous=video;
        }
        require(animated[0]>50 && animated[1]>50,"Both dormant halves must animate while their game-state bank stays frozen");
        require(ball_pixels>10000 && orb_frames>100 && brain_frames>100,"Insufficient ball layering or original orb-motion coverage");
        runtime->restore(initial);runtime->observe_display(false);
        for(unsigned n=0;n<1800;++n) {
            const auto frame=runtime->advance(recording::canonical_input(n));
            require(recording::fingerprint(frame,runtime->work_ram())==fingerprints[n],"Animation changed RAM, video or audio replay");
        }
        std::cout<<"PASS: inactive lower/upper changed on "<<animated[0]<<'/'<<animated[1]
            <<" frozen-bank frames; "<<ball_pixels<<" unobscured ball pixels; "<<orb_frames
            <<" original orb-motion comparisons; "<<brain_frames<<" brain orb comparisons; 1800 exact RAM/video/audio frames; repeated rendering is idempotent\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
