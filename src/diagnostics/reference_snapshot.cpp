// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/diagnostics/reference_snapshot.hpp"
#include "alien_crush/core/rom_image.hpp"
#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>
namespace alien_crush::diagnostics {
namespace {
std::uint32_t little(std::span<const std::byte> bytes) {
    std::uint32_t result=0;
    if(bytes.size()>4) throw std::runtime_error("Oversized integer field");
    for(std::size_t i=0;i<bytes.size();++i) result|=std::to_integer<std::uint32_t>(bytes[i])<<(8*i);
    return result;
}
}
std::vector<StateField> parse_reference_fields(std::span<const std::byte> payload) {
    if(payload.size()<32 || payload.size()>64U*1024U*1024U || std::memcmp(payload.data(),"MDFNSVST",8)!=0)
        throw std::runtime_error("Invalid pinned reference payload header");
    const auto end=little(payload.subspan(20,4));
    if(end<32 || end>payload.size()) throw std::runtime_error("Invalid reference payload length");
    // Header stores logical size; libretro's caller-owned buffer may have spare padding.
    payload=payload.first(end);
    std::vector<StateField> fields;
    std::set<std::pair<std::string,std::string>> names;
    std::size_t pos=32;
    while(pos<payload.size()) {
        if(payload.size()-pos<36) throw std::runtime_error("Truncated reference section");
        const auto* text=reinterpret_cast<const char*>(payload.data()+pos);
        std::string section(text,std::find(text,text+32,'\0'));
        const auto size=little(payload.subspan(pos+32,4)); pos+=36;
        if(size>payload.size()-pos) throw std::runtime_error("Reference section exceeds payload");
        const auto section_end=pos+size;
        while(pos<section_end) {
            const auto name_size=std::to_integer<unsigned>(payload[pos++]);
            if(name_size==0 || name_size+4>section_end-pos) throw std::runtime_error("Truncated reference field");
            std::string name(reinterpret_cast<const char*>(payload.data()+pos),name_size); pos+=name_size;
            const auto bytes=little(payload.subspan(pos,4)); pos+=4;
            if(bytes>section_end-pos) throw std::runtime_error("Reference field exceeds section");
            if(!names.emplace(section,name).second) throw std::runtime_error("Duplicate reference field");
            fields.push_back({section,std::move(name),payload.subspan(pos,bytes)}); pos+=bytes;
        }
    }
    return fields;
}
ReferenceSnapshot decode_reference_snapshot(const state::SaveStateContainer& saved) {
    if(saved.rom_sha256!=rom::kSupportedSha256 ||
       (saved.core_revision!="beetle-ae99235c2139c176c1a8d0fde2957bf701d3cab0-state1" &&
        saved.core_revision!="beetle-ae99235c2139c176c1a8d0fde2957bf701d3cab0"))
        throw std::runtime_error("Unsupported diagnostic snapshot schema or ROM");
    const auto fields=parse_reference_fields(saved.core_state);
    auto field=[&](std::string_view section,std::string_view name,std::size_t size) {
        for(const auto& f:fields) if(f.section==section && f.name==name) {
            if(f.bytes.size()!=size) throw std::runtime_error("Reference field size mismatch: "+f.name);
            return f.bytes;
        }
        throw std::runtime_error("Missing reference field: "+std::string(section)+"/"+std::string(name));
    };
    ReferenceSnapshot result; result.frame=saved.frame_number;
    auto words=[&](auto& dest,std::string_view section,std::string_view name) {
        const auto bytes=field(section,name,dest.size()*2);
        for(std::size_t i=0;i<dest.size();++i) dest[i]=static_cast<std::uint16_t>(little(bytes.subspan(i*2,2)));
    };
    words(result.vram,"VDC","VRAM"); words(result.sat,"VDC","SAT"); words(result.palette,"VCE","color_table");
    result.pc=static_cast<std::uint16_t>(little(field("CPU","PC",2)));
    const auto mpr=field("CPU","MPR",8);
    for(std::size_t i=0;i<8;++i) result.mpr[i]=std::to_integer<std::uint8_t>(mpr[i]);
    result.control=static_cast<std::uint16_t>(little(field("VDC","CR",2)));
    result.memory_width=static_cast<std::uint16_t>(little(field("VDC","MWR",2)));
    result.scroll_x=static_cast<std::uint16_t>(little(field("VDC","BXR",2)));
    result.scroll_y=static_cast<std::uint16_t>(little(field("VDC","BYR",2)));
    return result;
}
}
