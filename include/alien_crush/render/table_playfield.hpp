// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "alien_crush/render/animation_scene.hpp"
#include "alien_crush/assets/table_assets.hpp"
#include "alien_crush/assets/actor_assets.hpp"
#include "alien_crush/diagnostics/reference_snapshot.hpp"
#include "alien_crush/diagnostics/display_trace.hpp"
#include "alien_crush/render/pce_graphics.hpp"
#include <optional>
namespace alien_crush::render {
// Matching ROM side-wall tiles place the lower artwork at upper row 216.
// The game's ball handoff uses 224 instead; it is not an artwork measurement.
inline constexpr int table_half_offset=216;
inline constexpr int table_playfield_height=table_half_offset+240;
inline constexpr unsigned table_aspect_width=40,table_aspect_height=57;
// Read-only recognition of the original main-table RLE upload call. A host may
// omit its presentation delay during an already-confirmed transition hold.
bool table_transition_upload(const diagnostics::ReferenceSnapshot&,std::span<const std::byte> ram);
// Hide outgoing score-view graphics and the bonus loader's pre-fade frame.
// This affects cabinet presentation only; no runtime frames are skipped.
bool cabinet_viewport_blank(const diagnostics::ReferenceSnapshot& current,
    const diagnostics::ReferenceSnapshot* previous,std::span<const std::byte> ram);
// Collapse stationary handoff frames on a continuous presentation clock. The
// original core still executes every frame; this never changes game state.
class TableTransitionClock {
    std::uint64_t frame_{};
    unsigned x_{},y_{};
    unsigned transition_frames_{};
    bool valid_{},transition_{},upload_follows_{};
public:
    void reset() {valid_=transition_=upload_follows_=false;transition_frames_=0;}
    bool upload_follows() const {return upload_follows_;}
    bool omit(const diagnostics::ReferenceSnapshot&,std::span<const std::byte> ram,bool table_available);
};
// Upper artwork owns the overlap. Visible sprites from either half survive;
// the active half wins sprite conflicts. Masks describe the visible 256x240 area.
Image stitch_table_views(const Image& lower,const Image& upper,unsigned active_table,
    std::span<const std::uint8_t> lower_sprites={},std::span<const std::uint8_t> upper_sprites={});
struct TableRaster {
    std::span<const diagnostics::DisplayRow> rows;
    std::span<const diagnostics::PaletteWrite> palette;
    std::span<const RasterWrite> vram;
};
// Presentation-only bridge across a confirmed main-table change. An optional
// ball replaces the delayed SAT ball in both ready and held images, using only
// actual game coordinates. Reset on load or view change.
struct TableBallBackdrop { Image image; std::vector<std::uint8_t> sprites; };
class TableTransitionHold {
    std::optional<Image> image_;
    std::optional<TableBallBackdrop> backdrop_;
    std::uint64_t frame_{};
    unsigned table_{2},remaining_{};
    bool held_{},ball_updated_{};
public:
    void reset();
    bool held() const { return held_; }
    bool ball_updated() const { return ball_updated_; }
    const TableBallBackdrop* ball_backdrop() const {return ball_updated_ && backdrop_?&*backdrop_:nullptr;}
    std::optional<Image> present(std::optional<Image> live,std::uint64_t frame,
        unsigned table,unsigned camera,std::span<const std::byte> ram,
        const TableBallBackdrop* backdrop=nullptr,const Sprite* ball=nullptr);
};
// What the cabinet draws for both table halves: each actor bank as it is (the
// port runs the stored half's actors in cabinet play, gameplay::CabinetActors),
// held at the last coherent banks while the game exchanges them, with the
// stored half's flippers following the controls, and the colors of each half's
// running palette scripts.
class TableAnimation {
    std::array<std::array<std::byte,8192>,2> source_{},actors_{};
    std::array<std::array<std::uint16_t,512>,2> colors_{};
    std::array<std::array<bool,512>,2> color_owned_{};
    std::array<std::byte,16384> palette_rom_{};
    std::array<std::byte,256> palette_tables_{};
    std::array<std::byte,8> actor_offsets_{};
    bool valid_{};
    template<class Put> void upload(unsigned slot,unsigned frame,Put put) const;
public:
    explicit TableAnimation(std::span<const std::byte> rom);
    // The upper table's bumper colors for layout $A8, as 03:95B9 uploads them.
    void apply_bumper_colors(std::span<std::uint16_t> palette,unsigned layout) const;
    void reset() {valid_=false;}
    void update(const assets::ActorAssets&,std::span<const std::byte> ram);
    const auto& actors(unsigned half) const {return actors_[half];}
    void apply_palette(unsigned half,std::span<std::uint16_t> palette) const;
    bool owns_color(unsigned half,unsigned color) const {return color_owned_[half][color];}
};
class TablePlayfield {
    std::array<assets::TableBackground,2> tables_;
    std::array<assets::TableSprites,2> sprites_;
    assets::ActorAssets actors_;
    std::array<assets::TableIndicator,29> indicators_;
    assets::LowerCounterTiles counter_;
    std::array<std::vector<std::array<std::uint16_t,512>>,2> partial_fades_;
    mutable TableAnimation animation_;
    bool capture_animation_{};
    mutable std::optional<AnimationScene> animation_scene_;
    mutable std::uint64_t animation_frame_{};
    mutable unsigned animation_half_{2};
    mutable std::uint64_t cabinet_frame_number_{~std::uint64_t{0}};
    mutable std::optional<Image> cabinet_image_;
    std::optional<Image> cabinet_frame_impl(const diagnostics::ReferenceSnapshot& current,
        const diagnostics::ReferenceSnapshot* previous,std::span<const std::byte> ram,
        const TableRaster* raster,TableTransitionHold& transition) const;
    bool matches(const diagnostics::ReferenceSnapshot&,unsigned table) const;
    std::array<std::uint16_t,512> settled_palette(unsigned table,std::span<const std::byte> ram) const;
    std::optional<Image> compose_impl(const diagnostics::ReferenceSnapshot& current,
        const diagnostics::ReferenceSnapshot& previous,unsigned table,unsigned camera,
        std::span<const std::byte> ram,const TableRaster* raster,bool hide_ball,
        std::vector<std::uint8_t>* coverage,bool cabinet=false) const;
public:
    explicit TablePlayfield(std::span<const std::byte> rom);
    void capture_animation(bool value) {capture_animation_=value;}
    const AnimationScene* animation_scene() const {return animation_scene_?&*animation_scene_:nullptr;}
    // Full table throughout main-table loading, fades and score awards. Menus
    // and bonus rooms return empty. The diagnostic compose contract stays exact.
    std::optional<Image> cabinet_frame(const diagnostics::ReferenceSnapshot& current,
        const diagnostics::ReferenceSnapshot* previous,std::span<const std::byte> ram,
        const TableRaster* raster,TableTransitionHold& transition) const;
    // Experimental presentation. Unsupported scenes or incomplete raster inputs
    // return empty. Without raster observations, uses frozen video state.
    // Previous SAT preserves the established display epoch. Inactive actors
    // come from the game's stored bank; no gameplay or presentation cache advances.
    std::optional<Image> compose(const diagnostics::ReferenceSnapshot& current,
        const diagnostics::ReferenceSnapshot& previous,unsigned table,unsigned camera,
        std::span<const std::byte> ram,const TableRaster* raster=nullptr,
        std::optional<TableBallBackdrop>* backdrop=nullptr) const;
    std::optional<Sprite> transition_ball(unsigned table,std::span<const std::byte> ram,
        MotionPosition* subpixel=nullptr) const;
    Image inactive_view(unsigned table,std::span<const std::byte> ram,std::span<std::uint8_t> visible_sprites={},
        std::span<std::uint8_t> blockers={},std::span<const std::uint16_t> palette={},CompositionCapture* capture=nullptr) const;
};
}
