// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/render/table_playfield.hpp"
#include <algorithm>
#include <stdexcept>
namespace alien_crush::render {
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
    if(live) {image_=live;backdrop_=backdrop?std::optional<TableBallBackdrop>(*backdrop):std::nullopt;remaining_=0;return live;}
    // A table-code change immediately after a complete image arms the hold.
    // Same-table unsupported scenes cannot start or indefinitely extend it.
    if(changed && image_ && !was_held) remaining_=32;
    else if(changed) remaining_=0;
    if(remaining_ && image_) {
        --remaining_;held_=true;
        if(backdrop_ && ball) {
            auto result=backdrop_->image;
            // Keep the original complete hold if another sprite could affect
            // ordering at the new position. Never infer priority from RGB.
            for(int y=0;y<ball->height;++y) for(int x=0;x<ball->width;++x) {
                const int dx=ball->x+x,dy=ball->y+y;
                const auto color=ball->image.pixels[y*ball->width+x];
                if(!(color>>24) || dx<0 || dx>=result.width || dy<0 || dy>=result.height) continue;
                const auto p=static_cast<std::size_t>(dy)*result.width+dx;
                if(backdrop_->sprites[p]) return image_;
                result.pixels[p]=color;
            }
            ball_updated_=true;
            return result;
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
TablePlayfield::TablePlayfield(std::span<const std::byte> rom)
    :tables_(assets::decode_table_backgrounds(rom)),sprites_(assets::decode_table_sprites(rom)),actors_(rom),
    indicators_(assets::decode_table_indicators(rom)),counter_(assets::decode_lower_counter_tiles(rom)) {
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
    }
}
Image TablePlayfield::inactive_view(unsigned table,std::span<const std::byte> ram,std::span<std::uint8_t> visible,
    std::span<std::uint8_t> blockers) const {
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
    return render::compose(vram,tables_[table].palette,sat,{256,240,0,0,0,0xc0},visible,blockers);
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
std::optional<Sprite> TablePlayfield::transition_ball(unsigned table,std::span<const std::byte> ram) const {
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
    if(table==0) ball.y+=table_half_offset;
    return ball;
}
std::optional<Image> TablePlayfield::compose_impl(const diagnostics::ReferenceSnapshot& current,
    const diagnostics::ReferenceSnapshot& previous,unsigned table,unsigned camera,std::span<const std::byte> ram,
    const TableRaster* raster,bool hide_ball,std::vector<std::uint8_t>* coverage) const {
    if(table>1 || camera!=0 || current.frame!=previous.frame+1 ||
        !matches(current,table) || !matches(previous,table)) return std::nullopt;
    const auto& sat=previous.sat;
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
        if(found!=1) return std::nullopt;
    }
    // Recognize the recovered standard HUD by position, attributes and digit
    // pattern range. Suppress pixels without freeing their hardware fetch slots.
    for(unsigned i=0;i<64;++i) {
        auto* s=sat.data()+i*4;
        const bool digit=(s[2]>=40 && s[2]<=58 && s[2]%2==0);
        const bool score=s[0]==80 && s[1]>=48 && s[1]<=128 && (s[1]-48)%10==0;
        const bool ball=s[0]==96 && s[1]==264;
        if(s[3]==0x8f && digit && (score || ball)) hidden|=std::uint64_t{1}<<i;
    }
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
        const auto palette=diagnostics::palette_pixel_schedule(previous.palette,current.palette,raster->palette,256,240);
        live=render::compose_palette_raster(current.vram,previous.palette,sat,{256,240,0,0,0,0xc0},writes,palette,live_sprites,active_blocks,hidden);
    } else live=render::compose(current.vram,current.palette,sat,{256,240,0,0,0,0xc0},live_sprites,active_blocks,hidden);
    std::vector<std::uint8_t> inactive_blocks(coverage?256*240:0);
    const auto inactive=inactive_view(1-table,ram,inactive_sprites,inactive_blocks);
    if(coverage) {
        coverage->assign(256*table_playfield_height,0);
        const auto& lower=table==0?active_blocks:inactive_blocks;
        const auto& upper=table==0?inactive_blocks:active_blocks;
        std::copy(lower.begin(),lower.end(),coverage->begin()+256*table_half_offset);
        for(unsigned p=0;p<upper.size();++p) (*coverage)[p]|=upper[p];
    }
    return table==0?stitch_table_views(live,inactive,0,live_sprites,inactive_sprites):
        stitch_table_views(inactive,live,1,inactive_sprites,live_sprites);
}
}
