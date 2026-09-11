// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/diagnostics/reference_snapshot.hpp"
#include "alien_crush/render/pce_graphics.hpp"
#include "alien_crush/assets/table_assets.hpp"
#include "alien_crush/assets/actor_assets.hpp"
#include "alien_crush/diagnostics/alien_state.hpp"
#include "alien_crush/diagnostics/display_trace.hpp"
#include "alien_crush/render/table_playfield.hpp"
#include "alien_crush/render/score_panel.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <memory>
using namespace alien_crush;
namespace {
void require(bool test,const char* message) {if(!test) throw std::runtime_error(message);}
template<class F> void rejects(F function,const char* message) {
    bool rejected=false; try {function();} catch(const std::runtime_error&) {rejected=true;} require(rejected,message);
}
void put32(std::vector<std::byte>& bytes,std::size_t offset,std::uint32_t value) {
    for(unsigned i=0;i<4;++i) bytes.at(offset+i)=static_cast<std::byte>((value>>(8*i))&255);
}
}
int main() {
    try {
        {
            std::array<render::Image,10> digits;
            for(unsigned i=0;i<10;++i) digits[i]={16,16,std::vector<std::uint32_t>(256,0xff100000+i)};
            const render::ScorePanel panel(digits);
            diagnostics::AlienState state;
            state.score_points=4294967295U;state.high_scores[0]=4294967295U;state.ball_counter=255;
            const auto image=panel.compose(state);
            require(image.width==256 && image.height==96 && image.pixels.size()==256*96,"Score canvas bounds");
            const std::string maximum="4294967295";
            for(unsigned i=0;i<maximum.size();++i) {
                const auto color=0xff100000+static_cast<unsigned>(maximum[i]-'0');
                require(image.pixels[20*256+22+i*20]==color,"Maximum score retains every digit at fixed scale");
                require(image.pixels[74*256+35+i*10]==color,"Maximum high score fits reserved field");
            }
            require(image.pixels[74*256+208]==0xff100001,"Extra reserves cannot produce a negative ball number");
            for(unsigned remaining=0;remaining<=3;++remaining) {
                state.ball_counter=static_cast<std::uint8_t>(remaining);
                const auto ball=panel.compose(state);
                require(ball.pixels[74*256+208]==0xff100000+(remaining?4-remaining:3),"DMD counts balls up and retains ball three after final drain");
            }
            for(unsigned x=0;x<256;++x) require(image.pixels[x]==0xff000000 && image.pixels[95*256+x]==0xff000000,"Score outer black border");
            state={};const auto zero=panel.compose(state);
            require(zero.pixels[20*256+112]==0xff100000,"Zero score is visible");
            require(panel.compose(state).pixels==zero.pixels,"Score rendering is deterministic");
            state.title_menu=true;state.high_scores[0]=4294967295U;
            const auto menu=panel.compose(state);
            for(unsigned i=0;i<maximum.size();++i)
                require(menu.pixels[20*256+22+i*20]==0xff100000+static_cast<unsigned>(maximum[i]-'0'),"Menu high score fits at fixed scale");
            state.score_points=123456;state.ball_counter=255;
            require(panel.compose(state).pixels==menu.pixels,"Menu hides stale score and ball values");
            require(menu.pixels!=zero.pixels,"Menu presentation differs from gameplay");
            state.demo_play=true;
            require(panel.compose(state).pixels==menu.pixels,"Title takes precedence during demo-entry fade");
            state.title_menu=false;const auto demo=panel.compose(state);
            require(demo.pixels!=menu.pixels,"Demo has a visible label");
            state.score_points=999;state.ball_counter=1;
            require(panel.compose(state).pixels==demo.pixels,"Demo hides simulated score and ball values");
            state.demo_play=false;
            for(unsigned room=1;room<=4;++room) {
                state.bonus_room=static_cast<std::uint8_t>(room);
                const auto bonus=panel.compose(state);
                require(bonus.pixels[74*256+208]==0xff100000+room,"Bonus room number uses original digit slot");
                state.ball_counter=static_cast<std::uint8_t>(50+room);
                require(panel.compose(state).pixels==bonus.pixels,"Bonus room replaces ball counter");
            }
            const auto active_bonus=panel.compose(state);state.bonus_round_over=true;
            const auto ended_bonus=panel.compose(state);
            require(ended_bonus.pixels!=active_bonus.pixels,"Round-over label is visible");
            state.bonus_cleared=true;
            const auto cleared_bonus=panel.compose(state);
            require(cleared_bonus.pixels!=ended_bonus.pixels,"Clear result takes precedence over round-over label");
            require(std::equal(cleared_bonus.pixels.begin()+18*256,cleared_bonus.pixels.end(),active_bonus.pixels.begin()+18*256),"Clear result retains score and lower fields");
            state.title_menu=true;
            const auto clear_title=panel.compose(state);
            state.bonus_cleared=false;state.bonus_round_over=false;
            require(panel.compose(state).pixels==clear_title.pixels,"Title hides stale bonus outcome");
            state.title_menu=false;state.bonus_room=0;
            const auto playing=panel.compose(state);state.game_over=true;
            const auto game_over=panel.compose(state);
            require(game_over.pixels!=playing.pixels,"Game-over heading is visible");
            require(std::equal(game_over.pixels.begin()+18*256,game_over.pixels.end(),playing.pixels.begin()+18*256),"Game over retains final score and lower fields");
            state.match_visible=true;
            for(unsigned digit=0;digit<10;++digit) {
                state.match_digit=static_cast<std::uint8_t>(digit);
                const auto match=panel.compose(state);
                require(match.pixels[74*256+70]==0xff100000+digit &&
                    match.pixels[74*256+80]==0xff100000 && match.pixels[74*256+90]==0xff100000,
                    "DMD shows every match digit followed by two zeroes, including 000");
                require(std::equal(match.pixels.begin(),match.pixels.begin()+50*256,game_over.pixels.begin()),
                    "Match preserves final score and heading");
            }
            state.match_visible=false;
            state.title_menu=true;
            const auto game_over_title=panel.compose(state);state.game_over=false;
            require(panel.compose(state).pixels==game_over_title.pixels,"Title takes precedence over stale game-over state");
            require(std::equal(ended_bonus.pixels.begin()+18*256,ended_bonus.pixels.end(),active_bonus.pixels.begin()+18*256),"Round status retains score and lower fields");
            std::array<std::byte,8192> scene{};
            scene[0x151b]=std::byte{43};scene[0x155b]=std::byte{0xc4};scene[0x1c5b]=std::byte{7};
            for(unsigned action=0;action<6;++action) {
                scene[0x185b]=static_cast<std::byte>(action);
                const auto match=diagnostics::observe_alien_state(scene);
                require(match.match_visible==(action>=2 && action<=4),"Match visible only during spin, stop and result");
                if(match.match_visible) require(match.match_digit==7,"Match reads the live hundreds digit");
            }
            scene[0x185b]=std::byte{4};scene[0x46]=std::byte{1};
            require(diagnostics::observe_alien_state(scene).match_visible,"Winning extra ball retains match result");
            scene[0x1c5b]=std::byte{10};
            require(!diagnostics::observe_alien_state(scene).match_visible,"Uninitialized match digit rejected");
            scene={};
            state={};state.high_score_menu=true;
            state.high_scores={4294967295U,123456700,1000000,100,0};
            const auto ranks=panel.compose(state);
            for(unsigned rank=0;rank<5;++rank) {
                require(ranks.pixels[(18+rank*16)*256+30]==0xff100001+rank,"Every rank is visible");
                const auto text=std::to_string(state.high_scores[rank]);
                const auto start=160-((text.size()-1)*10+16)/2;
                for(unsigned digit=0;digit<text.size();++digit)
                    require(ranks.pixels[(18+rank*16)*256+start+digit*10]==0xff100000+static_cast<unsigned>(text[digit]-'0'),"Every ranked score fits without clipping");
            }
            state.score_points=999;state.ball_counter=2;state.game_over=true;
            require(panel.compose(state).pixels==ranks.pixels,"Score viewer hides stale gameplay fields");
            state.title_menu=true;const auto title_ranks=panel.compose(state);
            state.high_score_menu=false;
            require(panel.compose(state).pixels==title_ranks.pixels,"Title takes precedence over stale score viewer");
            constexpr std::array<std::byte,9> scores_signature{std::byte{0xfd},std::byte{0xeb},std::byte{3},
                std::byte{2},std::byte{1},std::byte{0xb0},std::byte{0x41},std::byte{0x8e},std::byte{0x40}};
            scene[0x7a]=std::byte{1};
            require(!diagnostics::observe_alien_state(scene).high_score_menu,"Viewer mode alone is insufficient");
            std::copy(scores_signature.begin(),scores_signature.end(),scene.begin()+0x1f7);
            require(diagnostics::observe_alien_state(scene).high_score_menu,"Complete paused score-viewer call signature");
            for(unsigned i=0;i<9;++i) {
                scene[0x1f7+i]^=std::byte{1};
                require(!diagnostics::observe_alien_state(scene).high_score_menu,"Partial score-viewer signature rejected");
                scene[0x1f7+i]^=std::byte{1};
            }
            scene[0x76]=std::byte{1};
            require(!diagnostics::observe_alien_state(scene).high_score_menu,"Demo excludes paused score viewer");
            scene={};
            auto practice_scores=scores_signature;
            practice_scores[5]=std::byte{0x8f};practice_scores[6]=std::byte{0x44};practice_scores[7]=std::byte{8};
            std::copy(practice_scores.begin(),practice_scores.end(),scene.begin()+0x1f7);
            scene[0x7a]=std::byte{2};scene[0x7c]=std::byte{1};
            require(diagnostics::observe_alien_state(scene).high_score_menu,"Practice game-end score call recognized");
            for(unsigned i=0;i<9;++i) {
                scene[0x1f7+i]^=std::byte{1};
                require(!diagnostics::observe_alien_state(scene).high_score_menu,"Partial practice score call rejected");
                scene[0x1f7+i]^=std::byte{1};
            }
            scene[0x7c]=std::byte{};
            require(!diagnostics::observe_alien_state(scene).high_score_menu,"Practice score call requires practice mode");
            scene={};scene[0x7a]=std::byte{2};
            constexpr std::array<std::byte,7> game_scores{std::byte{0xfd},std::byte{0xeb},std::byte{3},
                std::byte{2},std::byte{1},std::byte{0x7d},std::byte{0x41}};
            std::copy(game_scores.begin(),game_scores.end(),scene.begin()+0x1f9);
            require(diagnostics::observe_alien_state(scene).high_score_menu,"Normal game-end ranking recognized");
            for(unsigned i=0;i<7;++i) {
                scene[0x1f9+i]^=std::byte{1};
                require(!diagnostics::observe_alien_state(scene).high_score_menu,"Partial normal game-end signature rejected");
                scene[0x1f9+i]^=std::byte{1};
            }
            scene[0x76]=std::byte{1};
            require(!diagnostics::observe_alien_state(scene).high_score_menu,"Demo excludes normal game-end ranking");
            scene={};
            constexpr std::array<std::byte,9> signature{std::byte{0x0c},std::byte{0xeb},std::byte{0x18},
                std::byte{4},std::byte{3},std::byte{2},std::byte{1},std::byte{5},std::byte{0x40}};
            require(!diagnostics::observe_alien_state(scene).title_menu,"Zero RAM is not title evidence");
            require(!diagnostics::observe_alien_state(scene).demo_play,"Zero demo selector is player mode");
            for(unsigned table=0;table<6;++table) for(unsigned room=0;room<6;++room) {
                scene[0x79]=static_cast<std::byte>(table);scene[0x83]=static_cast<std::byte>(room);
                require(diagnostics::observe_alien_state(scene).bonus_room==(table==2 && room<4?room+1:0),"Bonus selector requires shared table code and known room");
                for(unsigned flags=0;flags<32;++flags) {
                    scene[0x78]=static_cast<std::byte>(flags);
                    require(diagnostics::observe_alien_state(scene).bonus_round_over==(table==2 && room<4 && (flags&1)),"Round over requires known bonus room and original end bit");
                    scene[0x85]=static_cast<std::byte>(flags);
                    require(diagnostics::observe_alien_state(scene).bonus_cleared==(table==2 && room<4 && (flags&1)),"Clear requires known room and results flag");
                }
            }
            for(const unsigned mode:{1u,2u,255u}) {
                scene[0x76]=static_cast<std::byte>(mode);
                require(diagnostics::observe_alien_state(scene).demo_play,"Demo selector follows original nonzero branch");
            }
            scene.fill(std::byte{});
            for(unsigned table=0;table<4;++table) for(unsigned balls=0;balls<4;++balls)
                for(unsigned flags=0;flags<32;++flags) for(unsigned mode=0;mode<4;++mode) {
                    scene[0x79]=static_cast<std::byte>(table);scene[0x46]=static_cast<std::byte>(balls);
                    scene[0x78]=static_cast<std::byte>(flags);scene[0x7c]=static_cast<std::byte>(mode&1);
                    scene[0x76]=static_cast<std::byte>(mode>>1);
                    require(diagnostics::observe_alien_state(scene).game_over==(table<2 && balls==0 && (flags&3)==3 && mode==0),"Game over requires exhausted ordinary balls and both original end bits");
                }
            std::copy(signature.begin(),signature.end(),scene.begin()+0x1f7);
            require(diagnostics::observe_alien_state(scene).title_menu,"Complete title call signature");
            for(unsigned i=0;i<9;++i) {
                scene[0x1f7+i]^=std::byte{1};
                require(!diagnostics::observe_alien_state(scene).title_menu,"Partial title signature is rejected");
                scene[0x1f7+i]^=std::byte{1};
            }
            digits[3].pixels.clear();rejects([&]{render::ScorePanel invalid(digits);},"Invalid digit artwork rejected");
        }
        {
            assets::LowerCounterTiles tiles{};
            for(unsigned value=0;value<7;++value) for(unsigned i=0;i<6;++i)
                tiles[value][i]=static_cast<std::uint16_t>(0xa000+value*16+i);
            std::array<std::uint16_t,1024> bat;bat.fill(0x1234);
            const auto original=bat;
            std::array<std::byte,8192> ram{};
            for(unsigned value=1;value<=7;++value) {
                bat=original;ram[0x7f]=static_cast<std::byte>(value);
                require(assets::restore_lower_counter(bat,tiles,0,ram),"Lower counter restored");
                for(unsigned address=0;address<1024;++address) {
                    const bool inside=(address>=0x22d && address<=0x22f) || (address>=0x24d && address<=0x24f);
                    const auto expected=inside?tiles[value-1][(address-0x22d)/32*3+(address-0x22d)%32]:original[address];
                    require(bat[address]==expected,"Counter rectangle and stride preserve unrelated BAT");
                }
            }
            for(const unsigned value:{0u,8u,255u}) {
                bat=original;ram[0x7f]=static_cast<std::byte>(value);
                require(!assets::restore_lower_counter(bat,tiles,0,ram) && bat==original,"Unknown counter leaves BAT unchanged");
            }
            ram[0x7f]=std::byte{7};bat=original;
            require(!assets::restore_lower_counter(bat,tiles,1,ram) && bat==original,"Upper table has no lower counter writes");
            rejects([&]{assets::restore_lower_counter(std::span(bat).first(1023),tiles,0,ram);},"Counter rejects short BAT");
            rejects([&]{assets::restore_lower_counter(bat,tiles,0,std::span(ram).first(8191));},"Counter rejects short RAM");
            rejects([&]{assets::restore_lower_counter(bat,tiles,2,ram);},"Counter rejects unknown table");
            rejects([&]{assets::decode_lower_counter_tiles({});},"Counter rejects unsupported ROM");
            require(bat==original,"Rejected counter inputs leave BAT unchanged");
        }
        render::TableTransitionHold transition;
        {
            render::TableTransitionClock clock;
            auto video=std::make_unique<diagnostics::ReferenceSnapshot>();
            std::array<std::byte,8192> ram{};ram[0x1500]=std::byte{1};ram[0x1540]=std::byte{0x80};
            ram[0x1680]=std::byte{10};ram[0x1fc]=std::byte{0xb5};ram[0x1fd]=std::byte{0x89};
            ram[0x1fe]=std::byte{0xa0};ram[0x1ff]=std::byte{0x40};video->pc=0xe2fc;video->frame=10;
            require(!clock.omit(*video,ram,true),"First clock sample cannot omit a frame");
            ram[0x79]=std::byte{1};ram[0x1680]=std::byte{234};video->frame=11;
            require(clock.omit(*video,ram,true),"Original 224-coordinate translation is stationary in world space");
            ram[0x1640]=std::byte{1};video->frame=12;
            require(!clock.omit(*video,ram,true),"Fractional ball movement retains its frame");
            video->pc=0x84d2;video->frame=13;
            require(!clock.omit(*video,ram,true),"Stationary physics routine is never compressed");
            video->pc=0xe2fc;ram[0x1fc]=std::byte{0xeb};video->frame=14;
            require(clock.omit(*video,ram,true),"Stationary transition wait is compressed");
            ram[0x1fc]=std::byte{0xee};ram[0x1fe]=std::byte{0x88};video->frame=15;
            require(clock.omit(*video,ram,true),"Single final frame wait follows confirmed transition");
            video->frame=16;require(!clock.omit(*video,ram,true),"Stale final wait cannot keep compressing frames");
            ram[0x1fe]=std::byte{0xa0};video->frame=18;
            require(!clock.omit(*video,ram,true),"Discontinuous frame numbers invalidate clock history");
            video->frame=19;require(!clock.omit(*video,ram,false),"Missing stitched table cannot compress frames");
            clock.reset();video->frame=20;
            require(!clock.omit(*video,ram,true),"View changes and state loads reset the clock");
            for(unsigned frame=21;frame<52;++frame) {video->frame=frame;require(clock.omit(*video,ram,true),"Bounded stationary transition is compressed");}
            video->frame=52;require(!clock.omit(*video,ram,true),"Stationary transition cannot compress indefinitely");
        }
        std::array<std::byte,8192> transition_ram{};
        {
            auto video=std::make_unique<diagnostics::ReferenceSnapshot>();video->pc=0xe40a;
            std::array<std::byte,8192> ram{};ram[0x1500]=std::byte{1};ram[0x1540]=std::byte{0x80};
            constexpr unsigned chain[]{0x35,0xeb,0x18,4,3,2,1,0x0c,0xe7,0xcf,0x89,0xa0,0x40};
            for(unsigned i=0;i<std::size(chain);++i) ram[0x1f3+i]=static_cast<std::byte>(chain[i]);
            ram[0x79]=std::byte{1};
            require(render::table_transition_upload(*video,ram),"Verified upper RLE upload recognized");
            for(unsigned i=0;i<std::size(chain);++i) {
                ram[0x1f3+i]^=std::byte{1};
                require(!render::table_transition_upload(*video,ram),"Partial upload call chain rejected");
                ram[0x1f3+i]^=std::byte{1};
            }
            for(const auto address:{0x76,0x7c,0x16,0x17}) {
                ram[address]=std::byte{1};require(!render::table_transition_upload(*video,ram),"Demo/practice/camera upload cannot change pacing");ram[address]=std::byte{};
            }
            for(const auto address:{0x53,0x54}) {
                ram[address]=std::byte{0x80};require(!render::table_transition_upload(*video,ram),"Unsupported scene cannot change pacing");ram[address]=std::byte{};
            }
            video->pc=0xe2fc;require(!render::table_transition_upload(*video,ram),"Stale upload stack at frame wait rejected");
            video->pc=0xe40a;video->mpr[7]=1;require(!render::table_transition_upload(*video,ram),"Wrong code bank rejected");video->mpr[7]=0;
            ram[0x79]=std::byte{};ram[0x1fc]=std::byte{0x2a};ram[0x1fd]=std::byte{0x8a};
            require(render::table_transition_upload(*video,ram),"Verified lower RLE upload recognized");
            rejects([&]{render::table_transition_upload(*video,std::span(ram).first(8191));},"Short upload RAM rejected");
        }
        render::Image complete{256,456,std::vector<std::uint32_t>(256*456,0xff112233)};
        require(transition.present(complete,10,0,0,transition_ram).has_value(),"Complete image seeds transition");
        for(unsigned frame=11;frame<=42;++frame) {
            const auto held=transition.present(std::nullopt,frame,1,0,transition_ram);
            require(held && held->pixels==complete.pixels && transition.held(),"Confirmed transition holds exact last image");
        }
        require(!transition.present(std::nullopt,43,1,0,transition_ram) && !transition.held(),"Hold expires after 32 frames");
        transition.present(complete,44,0,0,transition_ram);
        require(!transition.present(std::nullopt,45,0,0,transition_ram),"Same-table unavailable scene cannot arm hold");
        transition.present(complete,46,0,0,transition_ram);
        require(!transition.present(std::nullopt,48,1,0,transition_ram),"Skipped epoch clears image");
        transition.present(complete,49,0,0,transition_ram);
        transition.reset();
        require(!transition.present(std::nullopt,50,1,0,transition_ram),"Explicit load/reset clears transition");
        transition.present(complete,51,0,0,transition_ram);
        require(!transition.present(std::nullopt,52,1,1,transition_ram),"Camera change clears transition");
        transition.present(complete,53,0,0,transition_ram);
        transition_ram[0x53]=std::byte{0x80};
        require(!transition.present(std::nullopt,54,1,0,transition_ram),"Unsupported scene clears transition");
        transition_ram[0x53]=std::byte{};
        transition.present(complete,55,0,0,transition_ram);
        transition.present(std::nullopt,56,1,0,transition_ram);
        auto ready=complete; ready.pixels[0]=0xff445566;
        const auto resumed=transition.present(ready,57,1,0,transition_ram);
        require(resumed && resumed->pixels[0]==0xff445566 && !transition.held(),"Ready target replaces held image immediately");
        transition.present(std::nullopt,58,0,0,transition_ram);
        require(!transition.present(std::nullopt,59,1,0,transition_ram),"A second table change cannot extend a hold");
        render::TableBallBackdrop clean_ball{complete,std::vector<std::uint8_t>(256*456)};
        auto old_ball=complete; old_ball.pixels[0]=0xffffffff;
        render::Sprite moving_ball;
        moving_ball.x=10;moving_ball.y=20;moving_ball.width=16;moving_ball.height=16;
        moving_ball.image={16,16,std::vector<std::uint32_t>(256)};
        moving_ball.image.pixels[0]=0xffffffff;
        transition.present(old_ball,60,0,0,transition_ram,&clean_ball);
        const auto moved_ball=transition.present(std::nullopt,61,1,0,transition_ram,nullptr,&moving_ball);
        require(moved_ball && transition.ball_updated() && moved_ball->pixels[0]==complete.pixels[0] &&
            moved_ball->pixels[20*256+10]==0xffffffff,"Transition replaces old ball without leaving a duplicate");
        clean_ball.sprites[20*256+10]=1;
        moving_ball.image.pixels[1]=0xffffffff;
        transition.present(old_ball,62,0,0,transition_ram,&clean_ball);
        const auto blocked_ball=transition.present(std::nullopt,63,1,0,transition_ram,nullptr,&moving_ball);
        require(blocked_ball && transition.ball_updated() && blocked_ball->pixels[0]==complete.pixels[0] &&
            blocked_ball->pixels[20*256+10]==complete.pixels[20*256+10] &&
            blocked_ball->pixels[20*256+11]==0xffffffff,
            "Uncertain overlap masks one pixel without freezing or duplicating the old ball");
        moving_ball.x=30;
        const auto live_ball=transition.present(old_ball,64,1,0,transition_ram,&clean_ball,&moving_ball);
        require(live_ball && transition.ball_updated() && live_ball->pixels[0]==complete.pixels[0] &&
            live_ball->pixels[20*256+30]==0xffffffff,
            "Ready table uses current ball coordinates instead of reverting to old SAT position");
        transition.reset();
        require(!transition.present(std::nullopt,64,0,0,transition_ram,nullptr,&moving_ball),
            "Reset discards both the original image and ball-free backdrop");
        std::array<assets::TableIndicator,29> indicators;
        indicators.fill({1,0,1,1,0x9000});
        indicators[0]={0,31,2,2,0xd000};
        indicators[28]={0,1023,1,1,0xa000};
        std::array<std::byte,8192> indicator_ram{};
        indicator_ram[0xd6]=std::byte{3}; indicator_ram[0xd9]=std::byte{0x10};
        std::array<std::uint16_t,1024> indicator_bat;
        indicator_bat.fill(0xc345);
        assets::restore_table_indicators(indicator_bat,indicators,0,indicator_ram);
        require(indicator_bat[31]==0xd345 && indicator_bat[32]==0xd345 &&
            indicator_bat[63]==0xd345 && indicator_bat[64]==0xd345 && indicator_bat[1023]==0xa345,
            "Indicator rectangles preserve tile identity, row stride and final flag byte");
        require(indicator_bat[0]==0xc345 && indicator_bat[33]==0xc345,"Indicators isolate table halves and unset regions");
        const auto indicator_before=indicator_bat;
        indicators[28].width=2;
        rejects([&]{assets::restore_table_indicators(indicator_bat,indicators,0,indicator_ram);},"Indicator rectangle outside BAT rejected");
        require(indicator_bat==indicator_before,"Invalid indicator geometry cannot partially update the BAT");
        std::array<std::byte,8192> bank_fixture{};
        bank_fixture.fill(std::byte{0xee});
        bank_fixture[0x204]=std::byte{2}; bank_fixture[0x244]=std::byte{0x80};
        bank_fixture[0x9ff]=std::byte{0x37};
        const auto projected=assets::inactive_table_actor_ram(bank_fixture);
        require(projected[0x1504]==std::byte{2} && projected[0x1544]==std::byte{0x80} &&
            projected[0x1cff]==std::byte{0x37},"Inactive bank includes first and final actor fields");
        for(unsigned field=0;field<32;++field) for(unsigned slot=0;slot<4;++slot)
            require(projected[0x1500+field*64+slot]==std::byte{},"Inactive bank excludes ball and reserved slots in every field");
        require(projected[0x16]==std::byte{} && projected[0x17]==std::byte{} && projected[0x50]==std::byte{10},
            "Inactive presentation has local camera and original SAT reservation");
        require(bank_fixture[0x1504]==std::byte{0xee},"Projection never swaps runtime RAM");
        rejects([&]{(void)assets::inactive_table_actor_ram(std::span(bank_fixture).first(8191));},"Truncated inactive bank rejected");
        render::Image lower{256,256,std::vector<std::uint32_t>(256*256,0xff112233)};
        render::Image upper{256,240,std::vector<std::uint32_t>(256*240,0xff445566)};
        const auto lower_active=render::stitch_table_views(lower,upper,0);
        const auto upper_active=render::stitch_table_views(lower,upper,1);
        require(lower_active.width==256 && lower_active.height==456,"Stitch preserves the ROM's 216-pixel table offset");
        require(lower_active.pixels[215*256]==0xff445566 && lower_active.pixels[216*256]==0xff445566 &&
            lower_active.pixels.back()==0xff112233,"Fixed upper artwork spans overlap and clips unused BAT rows");
        require(lower_active.pixels==upper_active.pixels,"Background join does not change with active half");
        require(upper_active.pixels[239*256]==0xff445566 && upper_active.pixels[240*256]==0xff112233,
            "Upper viewport owns overlap at the artwork offset");
        std::vector<std::uint8_t> lower_mask(256*240),upper_mask(256*240);
        lower_mask[0]=1; lower_mask[1]=1; upper_mask[216*256+1]=1;
        const auto lower_sprite_join=render::stitch_table_views(lower,upper,0,lower_mask,upper_mask);
        const auto upper_sprite_join=render::stitch_table_views(lower,upper,1,lower_mask,upper_mask);
        require(lower_sprite_join.pixels[216*256]==lower.pixels[0] &&
            upper_sprite_join.pixels[216*256]==lower.pixels[0],"Lower sprite survives fixed artwork join on either half");
        require(lower_sprite_join.pixels[216*256+1]==lower.pixels[1] &&
            upper_sprite_join.pixels[216*256+1]==upper.pixels[216*256+1],"Active sprite wins an overlap conflict");
        rejects([&]{(void)render::stitch_table_views(lower,upper,0,lower_mask,{});},"Incomplete pair of sprite masks rejected");
        rejects([&]{(void)render::stitch_table_views(lower,upper,2);},"Unsupported table rejected");
        auto short_view=upper; short_view.pixels.pop_back();
        rejects([&]{(void)render::stitch_table_views(lower,short_view,0);},"Truncated table rejected");
        require(lower.pixels.size()==256*256 && upper.pixels.back()==0xff445566,"Stitch leaves source assets immutable");
        std::vector<std::byte> payload(77);
        std::memcpy(payload.data(),"MDFNSVST",8); put32(payload,20,77);
        std::memcpy(payload.data()+32,"CPU",3); put32(payload,64,9);
        payload[68]=std::byte{2}; std::memcpy(payload.data()+69,"PC",2); put32(payload,71,2);
        payload[75]=std::byte{0x34}; payload[76]=std::byte{0x12};
        const auto fields=diagnostics::parse_reference_fields(payload);
        require(fields.size()==1 && fields[0].section=="CPU" && fields[0].name=="PC" && fields[0].bytes[0]==std::byte{0x34},"Named little-endian field parsing");
        auto invalid=payload; invalid.pop_back();
        rejects([&]{(void)diagnostics::parse_reference_fields(invalid);},"Truncated payload rejected");
        invalid=payload; put32(invalid,71,999);
        rejects([&]{(void)diagnostics::parse_reference_fields(invalid);},"Field cannot cross section boundary");
        invalid=payload; invalid.insert(invalid.end(),payload.begin()+32,payload.end()); put32(invalid,20,static_cast<std::uint32_t>(invalid.size()));
        rejects([&]{(void)diagnostics::parse_reference_fields(invalid);},"Ambiguous duplicate field rejected");
        payload.resize(100); require(diagnostics::parse_reference_fields(payload).size()==1,"Libretro padding ignored");

        std::array<std::uint16_t,32768> vram{};
        std::array<std::uint16_t,512> palette{};
        std::array<std::uint16_t,256> sat{};
        palette[0]=7; palette[3*16+1]=7<<3; palette[3*16+2]=7<<6;
        vram[0]=0x3080; vram[128*16]=0x4080;
        const auto bg=render::background(vram,palette,32,32);
        require(bg.width==256 && bg.height==256,"BAT dimensions");
        require(bg.pixels[0]==0xffff0000 && bg.pixels[1]==0xff00ff00,"Background two low planes and palette selection");
        require(bg.pixels[2]==0xff0000ff,"Background zero uses universal color");
        vram[128*16+8]=0x4080; palette[3*16+5]=0x1ff; palette[3*16+10]=0;
        const auto planes=render::background(vram,palette,32,32);
        require(planes.pixels[0]==0xffffffff && planes.pixels[1]==0xff000000,"Background high planes");
        vram[0]=0x3fff;
        require(render::background(vram,palette,32,32).pixels[0]==0xff0000ff,"Dormant patterns beyond VRAM are transparent");
        rejects([&]{(void)render::background(vram,palette,33,32);},"Invalid map rejected");

        vram.fill(0); palette.fill(0);
        palette[256+8]=7; palette[256+1]=7<<3;
        sat[0]=64+12; sat[1]=32+9; sat[2]=64*2; sat[3]=0x80;
        vram[64*64+48]=0x8000;
        auto sprites=render::sprites(vram,palette,sat,0);
        require(sprites[0].x==9 && sprites[0].y==12 && sprites[0].foreground,"Sprite coordinates and priority");
        require(sprites[0].image.pixels[0]==0xff0000ff && sprites[0].image.pixels[1]==0,"Sprite plane 3 and transparency");
        sat[3]=0x8880;
        sprites=render::sprites(vram,palette,sat,0);
        require(sprites[0].image.pixels[255]==0xff0000ff && sprites[0].image.pixels[0]==0,"Both sprite flips");
        sat[2]=65*2; sat[3]=0x1180; vram[65*64]=0x8000; vram[66*64]=0x8000;
        sprites=render::sprites(vram,palette,sat,0);
        require(sprites[0].width==32 && sprites[0].height==32 && sprites[0].image.pixels[0]==0xff0000ff,"Wide sprite aligns odd pattern number");
        require(sprites[0].image.pixels[16]==0xffff0000 && sprites[0].image.pixels[16*32]==0xffff0000,"Wide/tall sprite tile addressing");
        sat[2]=64*2+1; sat[3]=0x80; vram[64*64+32]=0x8000;
        require(render::sprites(vram,palette,sat,4)[0].image.pixels[0]==render::pce_color(palette[256+3]),"Two-bit alternate plane selection");
        diagnostics::ReferenceSnapshot snapshot;
        {
            std::array<std::uint16_t,32768> memory{};
            std::array<std::uint16_t,512> colors{};
            std::array<std::uint16_t,256> attributes{};
            colors[0]=colors[1]=7; // Equal RGB values must not erase background opacity.
            colors[256]=7<<6; colors[257]=7<<3; colors[273]=7<<6;
            memory[0]=128; memory[128*16]=0x80;
            memory[64*64]=0xc000;
            attributes[0]=attributes[4]=64; attributes[1]=attributes[5]=32;
            attributes[2]=attributes[6]=128; attributes[7]=0x81;
            render::Viewport view{2,1};
            std::array<std::uint8_t,2> visible{};
            std::array<std::uint8_t,2> blocked{};
            auto composed=render::compose(memory,colors,attributes,view,visible,blocked);
            require(visible[0]==0 && visible[1]==1,"Sprite mask respects hidden early sprite and background transparency");
            require(blocked[0]==1 && blocked[1]==1,"Hidden sprite pixels still block insertion under SAT priority");
            memory[64*64]=0x8000;
            (void)render::compose(memory,colors,attributes,view,visible,blocked);
            require(blocked[0]==1 && blocked[1]==0,"Transparent sprite pixels do not block insertion");
            memory[64*64]=0xc000;
            require(composed.pixels[0]==render::pce_color(7),"Hidden earlier sprite blocks later foreground sprite");
            require(composed.pixels[1]==render::pce_color(7<<3),"Background color zero admits rear sprite");
            attributes[3]=0x80;
            colors[257]=colors[0];
            (void)render::compose(memory,colors,attributes,view,visible);
            require(visible[0]==1,"Visible sprite remains identifiable when its RGB equals the background");
            colors[257]=7<<3;
            const std::array<render::RasterWrite,1> hide_sprites{{{0,render::RasterTarget::control,0,0x80}}};
            (void)render::compose_palette_raster(memory,colors,attributes,view,hide_sprites,{},visible,blocked);
            require(visible[0]==0 && visible[1]==0,"Raster-disabled sprites clear previously visible mask pixels");
            require(blocked[0]==0 && blocked[1]==0,"Raster-disabled sprites clear insertion blockers");
            (void)render::compose_palette_raster(memory,colors,attributes,view,{}, {},visible);
            require(visible[0]==1 && visible[1]==1,"Palette raster preserves sprite coverage before color expansion");
            rejects([&]{(void)render::compose(memory,colors,attributes,view,std::span(visible).first(1));},
                "Truncated sprite coverage rejected");
            rejects([&]{(void)render::compose(memory,colors,attributes,view,{},std::span(blocked).first(1));},
                "Truncated insertion mask rejected");
            require(render::compose(memory,colors,attributes,view).pixels[0]==render::pce_color(7<<3),"Foreground sprite covers opaque background");
            memory[64*64]=0x4000; attributes[2]=130; memory[65*64]=0x8000;
            require(render::compose(memory,colors,attributes,view).pixels[1]==render::pce_color(7<<6),"Transparent earlier sprite admits later sprite");
            view.control=0x80; view.scroll_x=256; view.scroll_y=256;
            require(render::compose(memory,colors,attributes,view).pixels[0]==render::pce_color(7),"Scroll wraps at BAT dimensions");
            colors[1]=7<<3;
            require(render::compose(memory,colors,attributes,view).pixels[0]==render::pce_color(7<<3),"Wrapped background samples nonzero pixel");
            view.scroll_x=255;
            require(render::compose(memory,colors,attributes,view).pixels[1]==render::pce_color(7<<3),"Horizontal viewport crosses BAT seam");
            view.control=0x40; attributes.fill(0);
            require(render::compose(memory,colors,attributes,view).pixels[0]==render::pce_color(colors[256]),"Sprite-only backdrop selects palette 256");
            attributes[0]=64; attributes[1]=31; attributes[2]=128; attributes[3]=0x80;
            require(render::compose(memory,colors,attributes,view).pixels[0]==render::pce_color(colors[257]),"Partially clipped sprite samples visible column");
            view.control=0;
            require(render::compose(memory,colors,attributes,view).pixels[0]==render::pce_color(colors[0]),"Both layers disabled select universal backdrop");
            view.width=0;
            rejects([&]{(void)render::compose(memory,colors,attributes,view);},"Empty composition rejected");
            view.width=1025;
            rejects([&]{(void)render::compose(memory,colors,attributes,view);},"Oversized composition rejected");
            view.width=2;
            rejects([&]{(void)render::compose(memory,std::span(colors).first(256),attributes,view);},"Incomplete composition palette rejected");
        }
        {
            std::array<std::uint16_t,32768> memory{};
            std::array<std::uint16_t,512> colors{};
            std::array<std::uint16_t,256> attributes{};
            colors[257]=7; colors[258]=7<<3;
            // Fifteen transparent, horizontally offscreen pieces exhaust fifteen fetches.
            for(unsigned i=0;i<15;++i) {attributes[i*4]=64; attributes[i*4+1]=900;}
            attributes[60]=64; attributes[61]=32; attributes[62]=128; attributes[63]=0x180;
            for(unsigned y=0;y<16;++y) {memory[64*64+y]=0xffff; memory[65*64+16+y]=0xffff;}
            render::Viewport view{32,17,0,0,0,0x40};
            std::vector<std::uint8_t> blockers(32*17);
            auto limited=render::compose(memory,colors,attributes,view,{},blockers);
            require(blockers[31]==1 && blockers[16*32]==0,"Full fetch row blocks insertion even at transparent pixels and resets next row");
            require(limited.pixels[0]==render::pce_color(7) && limited.pixels[16]==render::pce_color(0),
                "Wide sprite at fetch sixteen retains only its left half");
            attributes[63]|=0x800;
            limited=render::compose(memory,colors,attributes,view);
            require(limited.pixels[0]==render::pce_color(7<<3) && limited.pixels[16]==render::pce_color(0),
                "Flipped wide sprite still fetches its screen-left half first");
            attributes[64]=64; attributes[65]=48; attributes[66]=128; attributes[67]=0x80;
            require(render::compose(memory,colors,attributes,view).pixels[16]==render::pce_color(0),
                "Later sprite cannot use exhausted row even after transparent fetches");
            const auto hidden_wide=render::compose(memory,colors,attributes,view,{},blockers,std::uint64_t{1}<<15);
            require(hidden_wide.pixels[0]==render::pce_color(0) && hidden_wide.pixels[16]==render::pce_color(0) && blockers[31],
                "Hidden wide sprite still exhausts fetch budget and blocks later sprites");
            attributes[64]=80;
            require(render::compose(memory,colors,attributes,view).pixels[16*32+16]==render::pce_color(7),
                "Fetch budget resets on each row and excludes vertically absent sprites");
            attributes[56]=63; // Only fourteen pieces intersect row fifteen now.
            require(render::compose(memory,colors,attributes,view).pixels[15*32+16]==render::pce_color(7),
                "Wide sprite consumes both remaining fetches at exact capacity");
            attributes.fill(0);attributes[252]=64;attributes[253]=32;attributes[254]=128;attributes[255]=0x80;
            require(render::compose(memory,colors,attributes,view).pixels[0]==render::pce_color(7),"Last SAT slot is visible without suppression");
            constexpr auto last_slot=std::uint64_t{1}<<63;
            std::vector<std::uint8_t> visible(32*17,1);
            const auto hidden=render::compose(memory,colors,attributes,view,visible,blockers,last_slot);
            require(hidden.pixels[0]==render::pce_color(0) && !visible[0] && !blockers[0],"Last SAT slot can be hidden without claiming pixels");
            const std::array<render::RasterWrite,2> bands{{{0,render::RasterTarget::control,0,0x40},{1,render::RasterTarget::control,0,0x40}}};
            require(render::compose_raster(memory,colors,attributes,view,bands,{}, {},last_slot).pixels==hidden.pixels,"Hidden slots survive raster band changes");
            require(render::compose_palette_raster(memory,colors,attributes,view,bands,{}, {}, {},last_slot).pixels==hidden.pixels,"Indexed palette raster preserves hidden slots");
        }
        {
            using Target=render::RasterTarget;
            using Write=render::RasterWrite;
            std::array<std::uint16_t,32768> memory{};
            std::array<std::uint16_t,512> colors{};
            std::array<std::uint16_t,256> attributes{};
            render::Viewport view{2,4,0,0,0,0};
            colors[0]=7; colors[256]=7<<3;
            const auto original_colors=colors;
            const std::array writes{Write{0,Target::palette,0,7<<6},Write{2,Target::palette,0,7<<3},
                Write{2,Target::palette,0,7},Write{3,Target::control,0,0x40}};
            auto result=render::compose_raster(memory,colors,attributes,view,writes);
            require(result.pixels==std::vector<std::uint32_t>{0xff00ff00,0xff00ff00,0xff00ff00,0xff00ff00,
                0xff0000ff,0xff0000ff,0xffff0000,0xffff0000},"Raster row-zero, same-row ordering, final row and backdrop control");
            require(colors==original_colors && view.control==0,"Raster writes preserve caller palette and registers");
            require(render::compose_raster(memory,colors,attributes,view,{}).pixels==render::compose(memory,colors,attributes,view).pixels,
                "No raster writes preserve frozen composition");
            colors[257]=7<<3; colors[1]=7<<6;
            memory[0]=128;
            for(unsigned y=0;y<8;++y) memory[128*16+y]=0xff;
            for(unsigned y=0;y<16;++y) memory[64*64+y]=0x8000;
            attributes[0]=64; attributes[1]=32; attributes[2]=128; attributes[3]=0x80;
            view={4,4};
            const auto original_memory=memory; const auto original_attributes=attributes;
            const std::array moving{Write{1,Target::sat,1,33},Write{2,Target::vram,64*64+2,0x4000}};
            result=render::compose_raster(memory,colors,attributes,view,moving);
            require(result.pixels[0]==0xffff0000 && result.pixels[4]==0xff00ff00 && result.pixels[5]==0xffff0000 &&
                result.pixels[9]==0xff00ff00 && result.pixels[10]==0xffff0000 && result.pixels[13]==0xffff0000,
                "Raster SAT and pattern writes retain absolute sprite rows and prior pixels");
            require(memory==original_memory && attributes==original_attributes,"Raster writes preserve caller VRAM and SAT");
            memory.fill(0); attributes.fill(0); memory[0]=128;
            for(unsigned y=0;y<8;++y) {
                const auto pixel=y+1;
                memory[128*16+y]=static_cast<std::uint16_t>(((pixel&1)?0x80:0)|((pixel&2)?0x8000:0));
                memory[128*16+y+8]=static_cast<std::uint16_t>(((pixel&4)?0x80:0)|((pixel&8)?0x8000:0));
                colors[pixel]=static_cast<std::uint16_t>(pixel);
            }
            view={1,4,0,0,0,0x80};
            const std::array scrolling{Write{2,Target::scroll_y,0,1}};
            result=render::compose_raster(memory,colors,attributes,view,scrolling);
            require(result.pixels==std::vector<std::uint32_t>{render::pce_color(1),render::pce_color(2),render::pce_color(4),render::pce_color(5)},
                "Raster scroll origin remains relative to full frame, not band start");
            memory.fill(0); memory[32]=128; memory[64]=129; memory[65]=130;
            for(unsigned y=0;y<8;++y) {memory[128*16+y]=0x80;memory[129*16+y]=0x8000;memory[130*16+y+8]=0x80;}
            view={1,10,0,0,0,0x80};
            const std::array mapping{Write{8,Target::memory_width,0,0x10},Write{9,Target::scroll_x,0,8}};
            result=render::compose_raster(memory,colors,attributes,view,mapping);
            require(result.pixels[8]==render::pce_color(2) && result.pixels[9]==render::pce_color(4),"Raster map width and horizontal scroll update at row boundary");
            const auto invalid_write=[&](Write write) {const std::array list{write};
                rejects([&]{(void)render::compose_raster(memory,colors,attributes,view,list);},"Invalid raster write rejected");};
            invalid_write({10,Target::control,0,0}); invalid_write({0,Target::vram,32768,0});
            invalid_write({0,Target::palette,512,0}); invalid_write({0,Target::palette,0,512});
            invalid_write({0,Target::sat,256,0}); invalid_write({0,Target::control,1,0});
            invalid_write({0,static_cast<Target>(99),0,0});
            const std::array unsorted{Write{2,Target::control,0,0},Write{1,Target::control,0,0}};
            rejects([&]{(void)render::compose_raster(memory,colors,attributes,view,unsorted);},"Backwards raster writes rejected");
            const std::vector<Write> excessive(65537);
            rejects([&]{(void)render::compose_raster(memory,colors,attributes,view,excessive);},"Raster event count bounded");
        }
        {
            const std::array rows{diagnostics::DisplayRow{0,0xc8,true},diagnostics::DisplayRow{1,0xc8,true},
                diagnostics::DisplayRow{2,0xc8,false},diagnostics::DisplayRow{3,0x88,false}};
            const auto schedule=diagnostics::display_control_schedule(rows,4);
            require(schedule.size()==4 && schedule[0].row==0 && schedule[0].value==0xc8 &&
                schedule[1].target==render::RasterTarget::burst_mode && schedule[1].value==1 &&
                schedule[2].row==2 && schedule[2].value==0 && schedule[3].row==3 && schedule[3].value==0x88,
                "Observed burst state remains explicit and consecutive equal controls coalesce");
            rejects([&]{(void)diagnostics::display_control_schedule(rows,5);},"Partial display row coverage rejected");
            auto invalid_rows=rows; invalid_rows[2].row=1;
            rejects([&]{(void)diagnostics::display_control_schedule(invalid_rows,4);},"Duplicate or unordered display rows rejected");
            rejects([&]{(void)diagnostics::display_control_schedule({},0);},"Empty display trace rejected");
        }
        {
            std::array<std::uint16_t,32768> memory{};
            std::array<std::uint16_t,512> colors{};
            std::array<std::uint16_t,256> attributes{};
            colors[0]=colors[1]=colors[257]=7; colors[256]=7<<3;
            memory[0]=memory[1]=memory[2]=128; memory[128*16]=0xff; memory[64*64]=0xffff;
            attributes[0]=64; attributes[1]=32; attributes[2]=128; attributes[3]=0x80;
            render::Viewport view{20,1};
            const std::array writes{render::PalettePixelWrite{2,257,7<<6}};
            auto result=render::compose_palette_raster(memory,colors,attributes,view,{},writes);
            require(result.pixels[0]==0xff0000ff && result.pixels[1]==0xff0000ff && result.pixels[2]==0xff00ff00 &&
                result.pixels[15]==0xff00ff00 && result.pixels[16]==0xff0000ff,
                "Pixel palette timing preserves palette identity when background and sprite RGB initially match");
            require(colors[257]==7,"Pixel palette timing preserves input colors");
            view.burst_mode=true;
            const std::array border{render::PalettePixelWrite{2,256,7<<6},render::PalettePixelWrite{2,256,7}};
            result=render::compose_palette_raster(memory,colors,attributes,view,{},border);
            require(result.pixels[0]==0xffff0000 && result.pixels[2]==0xff0000ff,"Burst mode uses palette 256 and ordered same-pixel updates");
            require(render::compose(memory,colors,attributes,view).pixels[0]==0xffff0000,"Frozen burst mode suppresses enabled sprites and background");
            const std::array mixed{render::RasterWrite{0,render::RasterTarget::palette,0,0}};
            rejects([&]{(void)render::compose_palette_raster(memory,colors,attributes,view,mixed,writes);},"Ambiguous palette timelines rejected");
            const std::array past_end{render::PalettePixelWrite{20,0,0}};
            rejects([&]{(void)render::compose_palette_raster(memory,colors,attributes,view,{},past_end);},"Pixel timing bounds enforced");
            const std::array backwards{render::PalettePixelWrite{2,0,0},render::PalettePixelWrite{1,0,0}};
            rejects([&]{(void)render::compose_palette_raster(memory,colors,attributes,view,{},backwards);},"Pixel timing order enforced");
            std::array<std::uint16_t,512> initial{},final_colors{}; final_colors[0]=4;
            const std::array trace{diagnostics::PaletteWrite{240,0,0,0,1,false},diagnostics::PaletteWrite{0,2,0,1,2,false},
                diagnostics::PaletteWrite{0,5,0,2,3,false},diagnostics::PaletteWrite{240,0,0,3,4,true}};
            const auto pixels=diagnostics::palette_pixel_schedule(initial,final_colors,trace,4,2);
            require(pixels.size()==3 && pixels[0].pixel==0 && pixels[1].pixel==2 && pixels[2].pixel==4,
                "Palette schedule distinguishes preceding blank, visible pixels, right border and completed frame");
            auto broken=trace; broken[1].before=0;
            rejects([&]{(void)diagnostics::palette_pixel_schedule(initial,final_colors,broken,4,2);},"Incomplete palette write chain rejected");
            final_colors[1]=1;
            rejects([&]{(void)diagnostics::palette_pixel_schedule(initial,final_colors,trace,4,2);},"Full final palette verification rejects unobserved changes");
        }
        snapshot.memory_width=0x70;
        require(snapshot.bat_width()==128 && snapshot.bat_height()==64,"MWR map dimensions");
        const std::array<std::byte,10> packed{std::byte{2},std::byte{0x34},std::byte{0x12},std::byte{0x78},std::byte{0x56},std::byte{0x83},std::byte{0xbc},std::byte{0x9a},std::byte{0},std::byte{0xff}};
        const auto decoded=assets::decode_word_rle(packed,5);
        require(decoded.words==std::vector<std::uint16_t>{0x1234,0x5678,0x9abc,0x9abc,0x9abc} && decoded.consumed==9,"RLE literals, repeated words, and terminator");
        rejects([&]{(void)assets::decode_word_rle(packed,4);},"RLE output bounded");
        rejects([&]{(void)assets::decode_word_rle(std::span(packed).first(7),5);},"Truncated RLE word rejected");
        const std::array<std::byte,4> wrapping{std::byte{0x80},std::byte{0x34},std::byte{0x12},std::byte{0}};
        require(assets::decode_word_rle(wrapping,256).words.size()==256,"RLE repeat zero count preserves CPU wrap");
        std::array<std::byte,8192> ram{};
        assets::PalettePulseState pulse{0x81,0,0,0};
        for(unsigned step=1;step<=14;++step) {
            const auto next=assets::advance_palette_pulse(pulse);
            const unsigned level=step<=7?step:14-step;
            require(next.color_zero==level*73 && next.state.green==level && next.state.red==level && next.state.blue==level,"Pulse rise/fall colors");
            require(next.state.flags==(step<8?0x81:(step<14?0xc0:0)),"Pulse direction and completion flags");
            pulse=next.state;
        }
        require(!assets::advance_palette_pulse(pulse).color_zero,"Completed pulse emits no further write");
        const auto inactive=assets::advance_palette_pulse({0x40,9,8,7});
        require(!inactive.color_zero && inactive.state.flags==0x40 && inactive.state.green==9,"Inactive pulse preserves state");
        const auto wrapped=assets::advance_palette_pulse({0xc1,0,0,0});
        require(wrapped.state.green==255 && wrapped.state.flags==0xc1 && wrapped.color_zero==511,"Pulse byte underflow and VCE packing");
        std::array<std::uint16_t,512> fade_current{},fade_target{};
        fade_target[0]=511; fade_target[1]=65; fade_target[2]=1;
        auto fade=assets::advance_palette_fade(fade_current,fade_target,21);
        require(fade[0]==8 && fade[1]==0 && fade[2]==0,"Fade brightness threshold and red tie priority");
        fade=assets::advance_palette_fade(fade_current,fade_target,1);
        require(fade[1]==64 && fade[2]==1,"Fade green tie priority and blue-only step");
        for(unsigned t=21;t>0;--t) fade_current=assets::advance_palette_fade(fade_current,fade_target,t);
        require(fade_current==fade_target,"Complete 21-step fade reaches target");
        rejects([&]{(void)assets::advance_palette_fade(fade_current,fade_target,0);},"Invalid fade threshold rejected");
        fade_current[2]=2;
        rejects([&]{(void)assets::advance_palette_fade(fade_current,fade_target,1);},"Non-monotonic fade input rejected");
        for(unsigned i=0;i<512;++i) fade_current[i]=static_cast<std::uint16_t>(i);
        const auto faded=assets::advance_palette_fade_out(fade_current);
        require(faded.already_black==1 && faded.colors[511]==438 && faded.colors[73]==0 && faded.colors[1]==0,
            "Fade-out decrements every nonzero channel and counts input black");
        for(unsigned step=0;step<7;++step) fade_current=assets::advance_palette_fade_out(fade_current).colors;
        require(std::all_of(fade_current.begin(),fade_current.end(),[](auto v){return v==0;}),"All 512 PCE colors fade to black in seven steps");
        require(assets::advance_palette_fade_out(fade_current).already_black==512,"Fade-out requires final already-black completion sweep");
        fade_current[0]=512;
        rejects([&]{(void)assets::advance_palette_fade_out(fade_current);},"Invalid fade-out color rejected");
        rejects([&]{(void)assets::advance_palette_fade_out({});},"Invalid fade-out buffer rejected");
        const auto hud=assets::assemble_table_hud(123456700,10);
        const std::array<unsigned,9> hud_digits{1,2,3,4,5,6,7,0,0};
        for(unsigned i=0;i<9;++i) require(hud[i].slot==i && hud[i].words[2]==(20+hud_digits[i])*2,"HUD decimal order and trailing hundreds");
        require(hud[9].words==std::array<std::uint16_t,4>{96,264,58,143},"HUD ball counter decrement and placement");
        require(assets::assemble_table_hud(0,1)[0].words[2]==40 && assets::assemble_table_hud(999999900,1)[6].words[2]==58,"HUD zero and maximum score");
        rejects([&]{(void)assets::assemble_table_hud(1000000000,1);},"HUD score overflow rejected");
        rejects([&]{(void)assets::assemble_table_hud(101,1);},"HUD fractional hundreds rejected");
        rejects([&]{(void)assets::assemble_table_hud(0,0);},"Invalid HUD counter rejected");
        const std::array<std::byte,14> background_bytes{std::byte{0xff},std::byte{0xff},std::byte{2},std::byte{2},
            std::byte{0xff},std::byte{0xff},std::byte{2},std::byte{0},std::byte{0},std::byte{0},
            std::byte{1},std::byte{0},std::byte{0xff},std::byte{1}};
        const auto updates=assets::decode_actor_background(background_bytes,0x80);
        require(updates.size()==4 && updates[0].address==65535 && updates[1].address==0 &&
            updates[2].address==31 && updates[3].address==32,"Background row stride and address wrapping");
        require(updates[0].value==0x8001 && updates[1].value==0xffff && updates[2].value==0x8000 &&
            updates[3].value==0x81fe,"Background base carry, wrapping and high-byte OR");
        rejects([&]{(void)assets::decode_actor_background({});},"Empty background header rejected");
        rejects([&]{(void)assets::decode_actor_background(std::span(background_bytes).first(13));},"Truncated background tiles rejected");
        std::array<std::byte,258> background_bounds{};
        rejects([&]{(void)assets::decode_actor_background(background_bounds);},"Zero background dimensions rejected");
        background_bounds[2]=std::byte{125}; background_bounds[3]=std::byte{1};
        require(assets::decode_actor_background(background_bounds).size()==125,"Largest non-wrapping background descriptor accepted");
        background_bounds[2]=std::byte{126};
        rejects([&]{(void)assets::decode_actor_background(background_bounds);},"Wrapping background byte index rejected");
        ram[0x99]=std::byte{3};
        auto overlay=assets::assemble_table_overlay(ram,59);
        require(overlay.size()==4 && overlay[0].slot==59 && overlay[3].slot==62 &&
            overlay[0].words==std::array<std::uint16_t,4>{191,132,262,143} &&
            overlay[3].words==std::array<std::uint16_t,4>{191,156,256,143},"Lower table overlay and strict capacity boundary");
        require(assets::assemble_table_overlay(ram,60).empty() && assets::assemble_table_overlay(ram,64).empty(),"Overlay capacity guard");
        ram[0x79]=std::byte{1}; ram[0x9a]=std::byte{0xff};
        ram[0x16]=std::byte{0xff}; ram[0x17]=std::byte{0xff};
        ram[0x2e]=std::byte{0xff}; ram[0x2f]=std::byte{0xff};
        overlay=assets::assemble_table_overlay(ram,0);
        require(overlay[0].words==std::array<std::uint16_t,4>{246,136,509,143} &&
            overlay[1].words==std::array<std::uint16_t,4>{246,144,0,143},"Upper overlay coordinate wrap, discarded shift carry and pattern arithmetic");
        ram[0x53]=std::byte{0x80};
        require(assets::assemble_table_overlay(ram,0).empty(),"Overlay first sign guard");
        ram[0x53]=std::byte{0}; ram[0x54]=std::byte{0x80};
        require(assets::assemble_table_overlay(ram,0).empty(),"Overlay second sign guard");
        ram[0x54]=std::byte{0}; ram[0x79]=std::byte{2};
        require(assets::assemble_table_overlay(ram,0).empty(),"Non-table overlay guard");
        rejects([&]{(void)assets::assemble_table_overlay(ram,65);},"Invalid overlay slot rejected");
        rejects([&]{(void)assets::assemble_table_overlay(std::span(ram).first(8191),0);},"Truncated overlay RAM rejected");
        ram.fill(std::byte{0});
        const std::array<std::byte,8> piece_bytes{std::byte{1},std::byte{0x80},std::byte{0xff},
            std::byte{0x34},std::byte{0x12},std::byte{0x8f},std::byte{0x89},std::byte{0xff}};
        const auto pieces=assets::decode_actor_pieces(piece_bytes);
        require(pieces.size()==1 && pieces[0].y==-128 && pieces[0].x==-1 && pieces[0].pattern==0x1234 && pieces[0].flags==0x898f,
            "Actor signed offsets, little-endian words and trailing descriptor data");
        rejects([&]{(void)assets::decode_actor_pieces({});},"Empty actor descriptor rejected");
        rejects([&]{(void)assets::decode_actor_pieces(std::span(piece_bytes).first(6));},"Truncated actor piece rejected");
        std::array<std::byte,259> large_descriptor{};
        rejects([&]{(void)assets::decode_actor_pieces(large_descriptor);},"Zero piece count rejected");
        large_descriptor[0]=std::byte{42};
        require(assets::decode_actor_pieces(large_descriptor).size()==42,"Largest non-wrapping descriptor accepted");
        large_descriptor[0]=std::byte{43};
        rejects([&]{(void)assets::decode_actor_pieces(large_descriptor);},"Wrapping descriptor index rejected");
        ram[0x121e]=std::byte{0x66}; ram[0x1224]=std::byte{1}; ram[0x122a]=std::byte{2};
        ram[0x1219]=std::byte{0x10}; ram[0x121f]=std::byte{0x27}; ram[0x46]=std::byte{3}; ram[0x68]=std::byte{4};
        const auto observed=diagnostics::observe_alien_state(ram);
        require(observed.score_points==13'143'000 && observed.high_scores[0]==1'000'000 && observed.ball_counter==3 && observed.score_update_pending,"Strided 24-bit score and HUD update state");
        std::cout<<"PASS: bounded named-state parsing, background planes/palettes, sprite addressing/flips/transparency\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
