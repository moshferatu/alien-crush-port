// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/render/flipper_swing.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace alien_crush::render {
namespace {
// Fewer drawn pixels than this has no reliable long axis.
constexpr double minimum_pixels=32;
// Steps smaller than this are already as smooth as they can be drawn; larger
// ones are not a flipper's next pose.
constexpr double smallest_turn=std::numbers::pi/180;
constexpr double largest_turn=largest_flipper_swing*std::numbers::pi/180;
// A flipper is long and thin: its pixels spread several times as far along
// its long axis as across it. A rounder shape has no dependable axis.
constexpr double least_elongation=3;
// A flipper's poses are one picture turned: most of the previous pose's
// pixels, turned, land on the current pose's.
constexpr double least_fit=.8;
// The pivot lies inside the flipper's round end, on its own drawn pixels. A
// shape that only shifted measures a pivot well outside itself.
constexpr int pivot_reach=2;
struct Shape { double x{},y{},angle{}; };
template<class F> void each_pixel(std::span<const AnimationLayer> layers,std::uint64_t group,F&& visit) {
    for(const auto& layer:layers) {
        if(layer.actor_group!=group) continue;
        for(int row=0;row<layer.image.height;++row) for(int column=0;column<layer.image.width;++column)
            if(layer.image.pixels[static_cast<std::size_t>(row)*layer.image.width+column]>>24)
                visit(layer.position.x+column+.5,layer.position.y+row+.5);
    }
}
// The centroid and long axis of every drawn pixel the actor's pieces cover,
// the axis measured from +x toward +y.
std::optional<Shape> shape_of(std::span<const AnimationLayer> layers,std::uint64_t group) {
    double count=0,sum_x=0,sum_y=0,sum_xx=0,sum_yy=0,sum_xy=0;
    each_pixel(layers,group,[&](double x,double y) {
        ++count;sum_x+=x;sum_y+=y;sum_xx+=x*x;sum_yy+=y*y;sum_xy+=x*y;
    });
    if(count<minimum_pixels) return std::nullopt;
    const double x=sum_x/count,y=sum_y/count;
    const double xx=sum_xx/count-x*x,yy=sum_yy/count-y*y,xy=sum_xy/count-x*y;
    const double spread=std::hypot(xx-yy,2*xy);
    if(xx+yy+spread<least_elongation*(xx+yy-spread)) return std::nullopt;
    return Shape{x,y,.5*std::atan2(2*xy,xx-yy)};
}
bool drawn_near(std::span<const AnimationLayer> layers,std::uint64_t group,double x,double y,int reach) {
    const int px=static_cast<int>(std::floor(x)),py=static_cast<int>(std::floor(y));
    for(const auto& layer:layers) {
        if(layer.actor_group!=group) continue;
        const int ox=static_cast<int>(std::floor(layer.position.x)),oy=static_cast<int>(std::floor(layer.position.y));
        for(int dy=-reach;dy<=reach;++dy) for(int dx=-reach;dx<=reach;++dx) {
            const int column=px+dx-ox,row=py+dy-oy;
            if(column>=0 && row>=0 && column<layer.image.width && row<layer.image.height &&
                (layer.image.pixels[static_cast<std::size_t>(row)*layer.image.width+column]>>24)) return true;
        }
    }
    return false;
}
MotionPosition anchor(const AnimationLayer& layer) {
    return {layer.position.x-layer.pose_offset.x,layer.position.y-layer.pose_offset.y};
}
}
std::optional<FlipperSwing> flipper_swing(std::span<const AnimationLayer> previous,
    std::span<const AnimationLayer> current,std::uint64_t group) {
    const auto of_group=[group](const AnimationLayer& layer) {return layer.actor_group==group;};
    const auto now=std::find_if(current.begin(),current.end(),of_group);
    const auto then=std::find_if(previous.begin(),previous.end(),of_group);
    if(!group || now==current.end() || then==previous.end() || !now->swings) return std::nullopt;
    const auto here=anchor(*now),there=anchor(*then);
    if(here.x!=there.x || here.y!=there.y) return std::nullopt;
    const auto from=shape_of(previous,group),to=shape_of(current,group);
    if(!from || !to) return std::nullopt;
    double turn=from->angle-to->angle;
    if(turn>std::numbers::pi/2) turn-=std::numbers::pi;
    if(turn<=-std::numbers::pi/2) turn+=std::numbers::pi;
    if(std::abs(turn)<smallest_turn || std::abs(turn)>largest_turn) return std::nullopt;
    // The previous pose turned by -turn about the pivot lands on the current
    // one: (I - R) p = c_now - R c_then.
    const double c=std::cos(-turn),s=std::sin(-turn);
    const double right_x=to->x-(c*from->x-s*from->y),right_y=to->y-(s*from->x+c*from->y);
    const double a=1-c,b=s,determinant=a*a+b*b;
    const double pivot_x=(a*right_x-b*right_y)/determinant,pivot_y=(b*right_x+a*right_y)/determinant;
    if(!drawn_near(current,group,pivot_x,pivot_y,pivot_reach)) return std::nullopt;
    double drawn=0,landed=0;
    each_pixel(previous,group,[&](double x,double y) {
        x-=pivot_x;y-=pivot_y;++drawn;
        if(drawn_near(current,group,pivot_x+c*x-s*y,pivot_y+s*x+c*y,0)) ++landed;
    });
    if(landed<least_fit*drawn) return std::nullopt;
    return FlipperSwing{pivot_x,pivot_y,turn*180/std::numbers::pi};
}
}
