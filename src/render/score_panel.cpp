// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/render/score_panel.hpp"
#include "alien_crush/assets/table_assets.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
namespace alien_crush::render {
namespace {
// Five-bit rows. Labels are host UI, not claimed as cartridge font assets.
constexpr std::string_view letters="SCOREHIGBALPUNDMVT";
constexpr std::array<std::array<unsigned,7>,18> labels{{
    {15,16,16,14,1,1,30},{14,17,16,16,16,17,14},
    {14,17,17,17,17,17,14},{30,17,17,30,20,18,17},
    {31,16,16,30,16,16,31},{17,17,17,31,17,17,17},
    {31,4,4,4,4,4,31},{14,17,16,23,17,17,15},
    {30,17,17,30,17,17,30},{14,17,17,31,17,17,17},
    {16,16,16,16,16,16,31},{30,17,17,30,16,16,16},
    {17,17,17,17,17,17,14},{17,25,25,21,19,19,17},
    {30,17,17,17,17,17,30},{17,27,21,21,17,17,17},{17,17,17,17,17,10,4},
    {31,4,4,4,4,4,4}}};
void label(Image& image,std::string_view text,int x,int y) {
    for(const char c:text) {
        if(c==' ') {x+=6;continue;}
        const auto& glyph=labels.at(letters.find(c));
        for(int row=0;row<7;++row) for(int col=0;col<5;++col)
            if(glyph[static_cast<std::size_t>(row)]&(1u<<(4-col)))
                image.pixels[static_cast<std::size_t>((y+row)*image.width+x+col)]=0xffddd4e8;
        x+=6;
    }
}
}
ScorePanel::ScorePanel(std::array<Image,10> digits):digits_(std::move(digits)) {
    for(const auto& digit:digits_) if(digit.width!=16 || digit.height!=16 || digit.pixels.size()!=256)
        throw std::runtime_error("Score panel requires ten 16x16 HUD digits");
}
ScorePanel ScorePanel::from_rom(std::span<const std::byte> rom) {
    const auto hud=assets::decode_hud_assets(rom);
    const auto palette=assets::decode_table_palette(rom,0);
    std::array<std::uint16_t,32768> vram{};
    std::copy(hud.patterns.begin(),hud.patterns.end(),vram.begin()+0x500);
    std::array<std::uint16_t,256> sat{};
    for(unsigned i=0;i<10;++i) {sat[i*4+2]=static_cast<std::uint16_t>((20+i)*2);sat[i*4+3]=0x8f;}
    const auto decoded=sprites(vram,palette,sat,0);
    std::array<Image,10> digits;
    for(unsigned i=0;i<10;++i) digits[i]=decoded[i].image;
    return ScorePanel(std::move(digits));
}
Image ScorePanel::compose(const diagnostics::AlienState& state) const {
    Image result{256,96,std::vector<std::uint32_t>(256*96,0xff000000)};
    auto number=[&](std::uint32_t value,int center,int y,int scale,unsigned places=0) {
        auto text=std::to_string(value);
        if(text.size()<places) text.insert(0,places-text.size(),'0');
        const auto width=(static_cast<int>(text.size())-1)*10*scale+16*scale;
        int x=center-width/2;
        for(const char c:text) {
            const auto& digit=digits_[static_cast<std::size_t>(c-'0')];
            for(int row=0;row<16*scale;++row) for(int col=0;col<16*scale;++col) {
                const auto color=digit.pixels[static_cast<std::size_t>((row/scale)*16+col/scale)];
                if(color>>24) result.pixels[static_cast<std::size_t>((y+row)*256+x+col)]=color;
            }
            x+=10*scale;
        }
    };
    if(state.title_transition) return result;
    if(state.title_menu || (state.demo_play && !state.high_score_menu)) {
        label(result,"HIGH SCORE",98,6);
        number(state.high_scores[0],128,18,2);
        if(state.demo_play && !state.title_menu) label(result,"DEMO",116,56);
        label(result,"PUSH RUN BUTTON",83,72);
        return result;
    }
    if(state.high_score_menu) {
        label(result,"HIGH SCORES",95,2);
        for(unsigned rank=0;rank<state.high_scores.size();++rank) {
            number(rank+1,38,16+static_cast<int>(rank)*16,1);
            number(state.high_scores[rank],160,16+static_cast<int>(rank)*16,1);
        }
        return result;
    }
    const auto heading=state.game_over?"GAME OVER":state.bonus_cleared?"BONUS CLEAR":state.bonus_round_over?"ROUND OVER":"SCORE";
    label(result,heading,state.game_over?101:state.bonus_cleared?95:state.bonus_round_over?98:113,6);
    number(state.score_points,128,18,2);
    if(state.match_visible) {
        label(result,"MATCH",74,61);number(state.match_digit*100u,88,73,1,3);
    } else {
        label(result,"HIGH",77,61);number(state.high_scores[0],88,73,1);
    }
    label(result,state.bonus_room?"BONUS":"BALL",state.bonus_room?202:205,61);
    // Three-ball game: 3/2/1 remaining becomes BALL 1/2/3. Keep BALL 3
    // during the final drain/results, and bound extra-ball reserves at BALL 1.
    const auto ball=4-std::clamp(static_cast<unsigned>(state.ball_counter),1u,3u);
    number(state.bonus_room?state.bonus_room:ball,216,73,1);
    return result;
}
}
