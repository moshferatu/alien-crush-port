// SPDX-License-Identifier: GPL-3.0-or-later
#include "presentation.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/recording.hpp"
#include "alien_crush/render/viewport_actors.hpp"
#include "alien_crush/render/flipper_swing.hpp"
#include <set>
#include <map>
#include <iostream>
#include <memory>
using namespace alien_crush;
namespace {
void require(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
render::Image flatten(const render::AnimationScene& scene) {
    auto result=scene.background;
    for(const auto& layer:scene.layers) for(int y=0;y<layer.image.height;++y) for(int x=0;x<layer.image.width;++x) {
        const int dx=static_cast<int>(std::floor(layer.position.x))+x,dy=static_cast<int>(std::floor(layer.position.y))+y;
        const auto color=layer.image.pixels[y*layer.image.width+x];
        if((color>>24) && dx>=0 && dx<result.width && dy>=0 && dy<result.height)
            result.pixels[dy*result.width+dx]=color;
    }
    return result;
}
void bitmap(const std::filesystem::path& path,const render::Image& image) {
    auto* surface=SDL_CreateSurfaceFrom(image.width,image.height,SDL_PIXELFORMAT_ARGB8888,
        const_cast<std::uint32_t*>(image.pixels.data()),image.width*4);
    require(surface!=nullptr,"Create screenshot surface");
    const bool saved=SDL_SaveBMP(surface,path.string().c_str());SDL_DestroySurface(surface);
    require(saved,"Save screenshot");
}
// How many flippers changed pose by a measured turn between two scenes.
unsigned swings(const render::AnimationScene& before,const render::AnimationScene& now) {
    std::set<std::uint64_t> groups;
    for(const auto& layer:now.layers) if(layer.swings) groups.insert(layer.actor_group);
    unsigned result=0;
    for(const auto group:groups) if(render::flipper_swing(before.layers,now.layers,group)) ++result;
    return result;
}
// A round-ended bar from a pivot at (68.5, 220.5), as one flipper piece.
render::AnimationLayer bar(double angle) {
    render::AnimationLayer layer{500,{60,200},{40,40,std::vector<std::uint32_t>(1600)},false,{},500,true};
    const double px=8.5,py=20.5,ex=px+26*std::cos(angle),ey=py+26*std::sin(angle);
    for(int y=0;y<40;++y) for(int x=0;x<40;++x) {
        const double dx=x+.5-px,dy=y+.5-py;
        const double along=std::clamp((dx*(ex-px)+dy*(ey-py))/(26*26),0.0,1.0);
        if(std::hypot(dx-along*(ex-px),dy-along*(ey-py))<=2.5 || std::hypot(dx,dy)<=4) layer.image.pixels[y*40+x]=0xffffffff;
    }
    return layer;
}
void renderer_checks() {
    render::MotionSmoothing motion;
    motion.update(1,{10,20});motion.update(2,{11,21});
    require(motion.position(.25).x==10.25 && motion.position(.75).y==20.75,"Fractional interpolation");
    motion.update(3,{100,20});require(motion.position(0).x==100,"Teleport snaps");
    motion.update(5,{102,20});require(motion.position(0).x==102,"Missing frame snaps");
    motion.reset();motion.update(6,{103,20});require(motion.position(0).x==103,"Load/focus reset snaps");
    for(const double hz:{120.,144.,165.,240.}) {
        motion.update(7,{104,20});
        const double fraction=59.8261/hz;
        require(std::abs(motion.position(fraction).x-(103+fraction))<1e-9,"Noninteger display ratio");
        motion.reset();motion.update(6,{103,20});
    }
    host::Presentation display("Smoothing renderer checks",1024,1824,true);
    render::TableBallBackdrop backdrop{render::Image{256,456,std::vector<std::uint32_t>(256*456,0xff123456)},
        std::vector<std::uint8_t>(256*456)};
    render::Sprite ball{};ball.width=ball.height=16;ball.image={16,16,std::vector<std::uint32_t>(256)};
    ball.image.pixels[0]=0xffffffff;
    const auto show=[&](unsigned frame,double x) {
        display.show({frame,256,456,backdrop.image.pixels,{}},256,456,true,{},&backdrop,&ball,{x,100},0);
    };
    show(1,50);show(2,51);
    for(unsigned quarter=0;quarter<=4;++quarter) {
        display.repeat(quarter/4.0);
        const auto output=display.capture();
        for(unsigned x=0;x<1024;++x) {
            const bool white=x>=200+quarter && x<204+quarter;
            require(output.pixels[400*1024+x]==(white?0xffffffffu:0xff123456u),"Each 240-Hz subframe moves by a target pixel, without ghosts");
        }
    }
    // A mouth occludes the moving ball, even at an interpolated position.
    backdrop.sprites[100*256+50]=1;backdrop.image.pixels[100*256+50]=0xffff0000;
    show(3,51);display.repeat(0);
    const auto masked=display.capture();
    require(masked.pixels[400*1024+200]==0xffff0000,"Mouth foreground survives smoothing");
    display.reset_smoothing();
    show(4,50);display.repeat(.5);
    require(display.capture().pixels[400*1024+200]==0xffff0000,"Mouth covers ball after reset");
    // A normal menu frame must discard both layers and motion history.
    display.show({5,256,456,backdrop.image.pixels,{}},256,456,true);
    display.repeat(.5);
    require(display.capture().pixels[400*1024+204]==0xff123456,"Fallback removes the ball layer");

    // Two independent actors, a changing pose/palette, and a stationary
    // occluder: movement must not dissolve between two screen positions.
    render::AnimationScene scene{backdrop.image,{},77};
    scene.background.pixels.assign(256*456,0xff123456);
    scene.layers.push_back({100,{50,100},{1,1,{0xffffffff}}});
    scene.layers.push_back({200,{80,100},{1,1,{0xff00ff00}}});
    const auto animated=[&](unsigned n,double fraction) {
        display.show({n,256,456,scene.background.pixels,{}},256,456,true,{},nullptr,nullptr,{},fraction,&scene);
    };
    animated(10,1);scene.layers[0].position.x=51;scene.layers[1].position.x=79;
    animated(11,0);
    for(unsigned quarter=0;quarter<=4;++quarter) {
        display.repeat(quarter/4.0);const auto output=display.capture();
        for(unsigned x=0;x<1024;++x) {
            const auto expected=x>=200+quarter && x<204+quarter?0xffffffffu:
                x>=320-quarter && x<324-quarter?0xff00ff00u:0xff123456u;
            const auto actual=output.pixels[400*1024+x];
            for(unsigned shift:{0u,8u,16u})
                require(std::abs(int((actual>>shift)&255)-int((expected>>shift)&255))<=(quarter==0 || quarter==4?0:1),
                    "All actors interpolate independently without trails (one color level blend rounding)");
        }
    }
    scene.layers[0].image.pixels[0]=0xffff0000;animated(12,.5);
    const auto mixed=display.capture().pixels[400*1024+204];
    require(((mixed>>16)&255)==255 && ((mixed>>8)&255)>100 && ((mixed>>8)&255)<150,"Pose colors blend without dimming alpha");
    scene.key=78;scene.layers[0].position.x=20;animated(13,0);
    require(display.capture().pixels[400*1024+80]==0xffff0000,"Scene entry has no outgoing frame flash");
    scene.layers.clear();animated(14,0);
    require(display.capture().pixels[400*1024+80]==0xff123456,"Removed entities do not linger");
    scene.key=80;
    scene.layers={{300,{50,100},{1,1,{0xffffffff}}},{301,{50,100},{1,1,{0xffff0000}},true}};
    animated(20,1);animated(21,.5);
    require((display.capture().pixels[400*1024+200]&0x00ff0000)==0x00ff0000 &&
        (display.capture().pixels[400*1024+200]&0x0000ffff)==0,"Mouth foreground occludes ball in both blended scenes");
    display.reset_smoothing();scene.layers[0].position.x=70;animated(22,0);
    require(display.capture().pixels[400*1024+280]==0xffffffff,"Pause/load reset discards all actor history");
    scene.layers.clear();

    // A stationary launcher can change sprite origins AND sprite count as it
    // changes pose. The visible pixels stay put throughout the whole blend.
    scene.key=81;
    scene.layers={{400,{50,100},{1,1,{0xffffffff}},false,{},400},
                  {401,{51,100},{1,1,{0xffffffff}},false,{1,0},400}};
    animated(30,1);
    scene.layers={{400,{49,100},{3,1,{0,0xffffffff,0xffffffff}},false,{-1,0},400}};
    animated(31,0);
    for(unsigned quarter=0;quarter<=4;++quarter) {
        display.repeat(quarter/4.0);const auto pixels=display.capture();
        for(unsigned x=190;x<215;++x) {
            const auto expected=x>=200 && x<208?255:0x56;
            require(std::abs(int(pixels.pixels[400*1024+x]&255)-expected)<=1,
                "Changing launcher layout preserves complete poses without flashing or sliding");
        }
    }
    const auto smooth=display.capture();const auto plain=flatten(scene);
    display.show({32,256,456,plain.pixels,{}},256,456,true);
    require(display.capture().pixels==smooth.pixels,"Toggling smoothing preserves stationary actor alignment");
    scene.layers.clear();

    // A flipper's pose change is its current pose turning about its pivot,
    // not a crossfade: halfway through, it stands at the angle halfway
    // between, where neither pose draws anything, over a fading trail.
    scene.key=82;scene.layers={bar(.3)};
    animated(40,1);
    const auto raised=scene;
    scene.layers={bar(-.1)};
    const auto turn=render::flipper_swing(raised.layers,scene.layers,500);
    require(turn && std::abs(turn->degrees-.4*180/3.14159265)<2 &&
        std::abs(turn->pivot_x-68.5)<2 && std::abs(turn->pivot_y-220.5)<2,"A flipper's turn and pivot are measured from its pixels");
    scene.layers[0].position.x=61;
    require(!render::flipper_swing(raised.layers,scene.layers,500),"A flipper that moved is not swung");
    scene.layers[0].position.x=60;scene.layers[0].swings=false;
    require(!render::flipper_swing(raised.layers,scene.layers,500),"Only flippers swing");
    scene.layers[0].swings=true;
    animated(41,.5);
    const auto swung=display.capture();
    const auto at=[&](double angle,double radius) {
        const int x=static_cast<int>(68.5+radius*std::cos(angle)),y=static_cast<int>(220.5+radius*std::sin(angle));
        return swung.pixels[static_cast<std::size_t>(y*4+2)*1024+x*4+2];
    };
    require(at(.1,22)==0xffffffff,"A flipper halfway through a pose change stands halfway between the poses");
    require((at(.3,24)&255)>0x56 && (at(.3,24)&255)<255,"A swinging flipper leaves a fading trail from its previous picture");
    require(std::abs(int(at(-.1,24)&255)-0x56)<=1,"A swinging flipper is not drawn ahead of its turn");
    display.repeat(1);
    const auto landed=display.capture();const auto lowered=flatten(scene);
    for(int y=0;y<456;++y) for(int x=0;x<256;++x) if(lowered.pixels[y*256+x]==0xffffffff)
        require(landed.pixels[static_cast<std::size_t>(y*4+2)*1024+x*4+2]==0xffffffff,"A flipper's pose is exact on its own tick");
    animated(42,1);animated(43,.5);
    require(display.capture().pixels==[&]{display.show({44,256,456,lowered.pixels,{}},256,456,true);return display.capture().pixels;}(),
        "A flipper holding its pose is drawn as it is");
    scene.layers.clear();

    // Scrolling background uses texture coordinates, rather than dissolving
    // two displaced patterns. Crossing the repeat boundary is one pixel.
    scene.key=79;scene.scroll_period=96;scene.scroll={95,0};
    scene.background.pixels.assign(256*456,0xff000000);
    for(int y=0;y<456;++y) for(int x=50;x<256;x+=96) scene.background.pixels[y*256+x]=0xffffffff;
    animated(15,1);
    scene.scroll.x=0;scene.background.pixels.assign(256*456,0xff000000);
    for(int y=0;y<456;++y) for(int x=49;x<256;x+=96) scene.background.pixels[y*256+x]=0xffffffff;
    animated(16,0);
    for(unsigned quarter=0;quarter<=4;++quarter) {
        display.repeat(quarter/4.0);const auto output=display.capture();
        for(unsigned x=190;x<210;++x) {
            const auto expected=x>=200-quarter && x<204-quarter?255:0;
            require(std::abs(int(output.pixels[400*1024+x]&255)-expected)<=1,"Title scroll interpolates across wrap without a double image");
        }
    }
}
void viewport_checks(const std::filesystem::path& rom,const std::filesystem::path& output) {
    assets::ActorAssets bonus_assets(core::read_file(rom));
    auto runtime=make_reference_runtime();
    host::Presentation display("Bonus animation checks",768,1368,true);
    for(unsigned room=0;room<4;++room) {
        runtime->load(rom);std::optional<diagnostics::ReferenceSnapshot> previous;
        unsigned layered=0;std::size_t actors=0;
        std::array<std::byte,8192> previous_ram{};
        std::map<std::uint64_t,unsigned> previous_slots;
        unsigned identified_samples=0,reordered_samples=0,unidentified_samples=0,swung=0;
        render::AnimationScene last_scene;
        for(unsigned n=0;n<600;++n) {
            auto input=recording::canonical_input(n);
            if(n>=100 && n<140) {
                const auto press=(n-100)/2;
                input.player_one=n%2?0:static_cast<std::uint16_t>(press<3?Pad::up:press<9?Pad::down:press<17?Pad::left:Pad::right);
            }
            if(n>=144 && n<144+room*2) input.player_one=n%2?0:Pad::button_i;
            const auto frame=runtime->advance(input);
            const render::Image original{frame.width,frame.height,{frame.pixels.begin(),frame.pixels.end()}};
            const auto snapshot=runtime->video_snapshot();
            const auto& sat=previous?previous->sat:snapshot.sat;
            auto scene=render::viewport_animation(original,snapshot.vram,snapshot.palette,sat,
                {frame.width,frame.height,snapshot.scroll_x,snapshot.scroll_y,snapshot.memory_width,snapshot.control},10+room);
            if(n>400) {
                std::vector<unsigned> slots;
                for(const auto& layer:scene.layers) slots.push_back(static_cast<unsigned>(layer.id));
                identified_samples+=render::identify_viewport_actors(scene,bonus_assets,previous_ram,sat,previous->frame);
                std::map<std::uint64_t,unsigned> current_slots;
                for(std::size_t i=0;i<scene.layers.size();++i) {
                    const auto& layer=scene.layers[i];
                    if(slots[i]<10) continue;
                    if(!layer.actor_group) { ++unidentified_samples; continue; }
                    require(current_slots.emplace(layer.id,slots[i]).second,"Bonus sprite identities are unique");
                    const auto old=previous_slots.find(layer.id);
                    if(old!=previous_slots.end() && old->second!=slots[i]) ++reordered_samples;
                }
                previous_slots=std::move(current_slots);
                swung+=swings(last_scene,scene);
                last_scene=scene;
            }
            require(flatten(scene).pixels==original.pixels,"Bonus/title split preserves every source pixel");
            if(room==0 && (n==98 || n==99)) {
                auto title=scene;title.key=2;title.scroll_period=96;
                title.scroll={double(snapshot.scroll_x%96),double(snapshot.scroll_y%96)};
                const auto tile=render::compose(snapshot.vram,snapshot.palette,snapshot.sat,
                    {96,96,snapshot.scroll_x,snapshot.scroll_y,snapshot.memory_width,static_cast<std::uint16_t>(snapshot.control&0x80)});
                display.show({snapshot.frame,original.width,original.height,original.pixels,{}},4,3,false,
                    {snapshot.frame,tile.width,tile.height,tile.pixels,{}},nullptr,nullptr,{},0,&title);
                if(n==99) {display.repeat(.5);bitmap(output/"title.bmp",display.capture());}
            }
            if(n>400 && !scene.layers.empty()) {++layered;actors+=scene.layers.size();}
            if(n==498 || n==499) {
                display.show({snapshot.frame,original.width,original.height,original.pixels,{}},4,3,true,{},nullptr,nullptr,{},0,&scene);
                if(n==499) {
                    display.repeat(.5);bitmap(output/("bonus-"+std::to_string(room+1)+".bmp"),display.capture());
                }
            }
            previous=snapshot;
            std::copy(runtime->work_ram().begin(),runtime->work_ram().end(),previous_ram.begin());
        }
        require(layered>100 && actors>500,"All four bonus rooms expose moving sprite layers");
        require(identified_samples>500 && reordered_samples>0,"Bonus identity survives real sprite-slot changes during flipper use");
        std::cout<<"Bonus "<<room+1<<": "<<identified_samples<<" identified pieces, "<<unidentified_samples<<" conservative fallbacks, "<<reordered_samples<<" stable identities across slot changes\n";
        std::cout<<"Bonus "<<room+1<<": "<<layered<<" layered frames, "<<actors<<" sprite samples\n";
        require(swung>20,"Bonus room flippers swing between their poses");
        std::cout<<"Bonus "<<room+1<<": "<<swung<<" flipper swings\n";
    }
}
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"Expected ROM and screenshot directory");
        require(SDL_Init(SDL_INIT_VIDEO),"SDL video");
        renderer_checks();
        const auto path=core::path_from_utf8(argv[1]);
        const auto output=core::path_from_utf8(argv[2]);std::filesystem::create_directories(output);
        viewport_checks(path,output);
        auto runtime=make_reference_runtime(HandoffMode::continuous);runtime->load(path);runtime->observe_display(true);
        auto table=std::make_unique<render::TablePlayfield>(core::read_file(path));
        table->capture_animation(true);
        render::TableTransitionHold transition;
        std::optional<diagnostics::ReferenceSnapshot> previous;
        host::Presentation display("Alien Crush smoothing",768,1368,true);
        host::Presentation alignment_display("Cabinet alignment checks",1024,1824,true);
        unsigned checked=0,crossings=0,last_half=2;
        std::array<std::optional<render::MotionPosition>,128> overlay_positions;
        unsigned overlay_samples=0,swung=0;
        render::AnimationScene last_scene;
        render::MotionSmoothing motion;
        for(unsigned n=0;n<1000;++n) {
            (void)runtime->advance(recording::canonical_input(n));
            const auto snapshot=runtime->video_snapshot();
            const auto ram=runtime->work_ram();
            const std::vector<std::byte> before(ram.begin(),ram.end());
            const render::TableRaster raster{runtime->display_rows(),runtime->palette_writes(),runtime->vram_rows()};
            const auto image=table->cabinet_frame(snapshot,previous?&*previous:nullptr,ram,&raster,transition);
            const auto half=std::to_integer<unsigned>(ram[0x79]);
            render::MotionPosition position;
            const auto ball=table->transition_ball(half,ram,&position);
            const auto* backdrop=transition.ball_backdrop();
            if(image && ball && backdrop) {
                require(position.x==ball->x && position.y==ball->y,"Ball uses identical smoothed and original raster endpoints");
                if(const auto* animation=table->animation_scene()) {
                    if(n>=480 && n<=650 && (n%17==0 || (n>=578 && n<=583))) {
                        auto full=*animation;
                        full.layers.push_back({~std::uint64_t{0},position,ball->image});
                        std::stable_partition(full.layers.begin(),full.layers.end(),[](const auto& layer){return !layer.foreground;});
                        alignment_display.show({snapshot.frame,256,456,image->pixels,{}},256,456,true,{},nullptr,nullptr,{},1,&full);
                        const auto smoothed=alignment_display.capture();
                        alignment_display.show({snapshot.frame,256,456,image->pixels,{}},256,456,true);
                        require(alignment_display.capture().pixels==smoothed.pixels,
                            "Real table GPU endpoints match smoothing off during launcher and flipper operation");
                    }
                    swung+=swings(last_scene,*animation);
                    last_scene=*animation;
                    auto rebuilt=animation->background;
                    for(const auto& layer:animation->layers) {
                        require(layer.position.x==std::floor(layer.position.x) && layer.position.y==std::floor(layer.position.y),
                            "Stationary actors have no smoothing-only fractional displacement");
                        if(layer.id>=0x100000 && layer.id<0x100080) {
                            const auto i=layer.id-0x100000;
                            require((i%64)<4,"Bonus overlay identity is independent of changing SAT slots");
                            if(overlay_positions[i]) require(overlay_positions[i]->x==layer.position.x &&
                                overlay_positions[i]->y==layer.position.y,"Flipper poses never relocate bonus digits");
                            overlay_positions[i]=layer.position;++overlay_samples;
                        }
                        for(int y=0;y<layer.image.height;++y) for(int x=0;x<layer.image.width;++x) {
                            const int dx=static_cast<int>(std::floor(layer.position.x))+x,dy=static_cast<int>(std::floor(layer.position.y))+y;
                            const auto color=layer.image.pixels[y*layer.image.width+x];
                            if((color>>24) && dx>=0 && dx<rebuilt.width && dy>=0 && dy<rebuilt.height)
                                rebuilt.pixels[dy*rebuilt.width+dx]=color;
                        }
                    }
                    if(rebuilt.pixels!=backdrop->image.pixels) {
                        bitmap(output/"layers-actual.bmp",rebuilt);bitmap(output/"layers-expected.bmp",backdrop->image);
                        throw std::runtime_error("Full animation decomposition differs at frame "+std::to_string(n));
                    }
                }
                // At the original integer coordinates, separated layers must
                // reproduce the complete cabinet image pixel for pixel.
                auto composed=backdrop->image;
                for(int y=0;y<16;++y) for(int x=0;x<16;++x) {
                    const int dx=ball->x+x,dy=ball->y+y;
                    const auto color=ball->image.pixels[y*16+x];
                    if(!(color>>24) || dx<0 || dx>=256 || dy<0 || dy>=456) continue;
                    const auto at=dy*256+dx;if(!backdrop->sprites[at]) composed.pixels[at]=color;
                }
                require(composed.pixels==image->pixels,"Extracted GPU layers preserve original table and mouth occlusion");
                ++checked;
                motion.update(snapshot.frame,position);
                if(last_half<2 && half!=last_half) {
                    ++crossings;
                    require(std::abs(position.y-motion.position(0).y)<32,"Seam remains in one world coordinate system");
                }
                last_half=half;
                if(n>=598 && n<=599) {
                    display.show({snapshot.frame,256,456,image->pixels,{}},40,57,true,{},backdrop,&*ball,position,0);
                    if(n==599) for(unsigned part=0;part<=4;++part) {
                        display.repeat(part/4.0);bitmap(output/("table-"+std::to_string(part)+".bmp"),display.capture());
                    }
                    if(table->animation_scene()) {
                        auto animation=*table->animation_scene();
                        animation.layers.push_back({~std::uint64_t{0},position,ball->image});
                        std::stable_partition(animation.layers.begin(),animation.layers.end(),[](const auto& layer){return !layer.foreground;});
                        display.show({snapshot.frame,256,456,image->pixels,{}},40,57,true,{},nullptr,nullptr,{},0,&animation);
                        if(n==599) for(unsigned part=0;part<=4;++part) {
                            display.repeat(part/4.0);bitmap(output/("all-animation-"+std::to_string(part)+".bmp"),display.capture());
                        }
                    }
                }
            } else {motion.reset();last_half=2;}
            require(std::equal(before.begin(),before.end(),runtime->work_ram().begin()),"Smoothing never writes game state");
            previous=snapshot;
        }
        require(checked>100 && crossings>0,"ROM route covers layered table play and seam crossings");
        require(overlay_samples>100,"ROM route verifies bonus overlays while operating flippers");
        // Both halves' flippers answer the controls, so each press swings two.
        require(swung>100,"Table flippers swing between their poses in both halves");
        std::cout<<checked<<" exact layered frames, "<<crossings<<" seam crossings and "<<swung<<" flipper swings verified\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
