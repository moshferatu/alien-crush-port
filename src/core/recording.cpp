// SPDX-License-Identifier: GPL-3.0-or-later
#include "alien_crush/core/recording.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/rom_image.hpp"
#include <cstring>
#include <stdexcept>
namespace alien_crush::recording {
namespace {
void validate(PadInput pad, std::uint8_t command) {
    if(((pad.player_one|pad.player_two)&0xff00U)!=0) throw std::runtime_error("Invalid controller bits");
    if(command>101) throw std::runtime_error("Invalid plunger command");
    if(command==101 && (pad.player_one&3)!=0) throw std::runtime_error("Plunger cancellation requires released I/II");
}
}
Frame advance(ReferenceRuntime& runtime, PadInput pad, std::uint8_t command) {
    validate(pad,command);
    const bool accepted=command==0 || (command==101?runtime.cancel_plunger_draw():runtime.apply_plunger_draw(command));
    if(!accepted) throw std::runtime_error("Recorded plunger command rejected before frame "+std::to_string(runtime.save().frame_number+1));
    return runtime.advance(pad);
}
void write(const std::filesystem::path& path, const Recording& recording) {
    const bool extended=!recording.plunger.empty();
    if(extended && recording.plunger.size()!=recording.inputs.size()) throw std::runtime_error("Plunger timeline length mismatch");
    const auto initial = state::encode(recording.initial);
    std::vector<std::byte> bytes;
    for (char c : std::string_view(extended?"ACREC002":"ACREC001")) bytes.push_back(static_cast<std::byte>(c));
    auto u32 = [&](std::uint32_t v) { for(unsigned s=0;s<32;s+=8) bytes.push_back(static_cast<std::byte>((v>>s)&255)); };
    if (recording.inputs.size() > 10'000'000U) throw std::runtime_error("Recording too long");
    u32(static_cast<std::uint32_t>(initial.size()));
    u32(static_cast<std::uint32_t>(recording.inputs.size()));
    bytes.insert(bytes.end(), initial.begin(), initial.end());
    for (std::size_t i=0;i<recording.inputs.size();++i) {
        const auto input=recording.inputs[i];
        const auto command=recording.plunger_at(i);
        validate(input,command);
        u32(input.player_one | (static_cast<std::uint32_t>(input.player_two)<<16));
        if(extended) bytes.push_back(static_cast<std::byte>(command));
    }
    const auto hash = rom::compute_digests(bytes).sha256;
    for (char c : hash) bytes.push_back(static_cast<std::byte>(c));
    core::write_file_atomic(path, bytes);
}
Recording read(const std::filesystem::path& path) {
    const auto bytes = core::read_file(path, 128U*1024U*1024U);
    if(bytes.size()<80) throw std::runtime_error("Invalid recording header");
    const bool extended=std::memcmp(bytes.data(),"ACREC002",8)==0;
    if(!extended && std::memcmp(bytes.data(),"ACREC001",8)!=0) throw std::runtime_error("Invalid recording header");
    const auto payload = std::span(bytes).first(bytes.size()-64);
    const auto hash = rom::compute_digests(payload).sha256;
    if(std::memcmp(hash.data(),bytes.data()+payload.size(),64)!=0) throw std::runtime_error("Recording checksum mismatch");
    auto u32 = [&](std::size_t i) { std::uint32_t v=0; for(unsigned s=0;s<32;s+=8) v |= std::to_integer<std::uint32_t>(bytes[i++])<<s; return v; };
    const auto state_size = u32(8), frames = u32(12);
    const std::size_t stride=extended?5:4;
    if(frames>10'000'000U || 16ULL+state_size+stride*frames != payload.size()) throw std::runtime_error("Invalid recording size");
    auto decoded = state::decode(payload.subspan(16,state_size));
    if(!decoded.decoded) throw std::runtime_error(decoded.message);
    Recording result{std::move(decoded.container),{}};
    result.inputs.reserve(frames);
    for(std::size_t i=0;i<frames;++i) {
        const auto offset=16+state_size+stride*i;
        const auto packed=u32(offset);
        if((packed & 0xff00ff00U)!=0) throw std::runtime_error("Invalid controller bits");
        result.inputs.push_back({static_cast<std::uint16_t>(packed&255),static_cast<std::uint16_t>((packed>>16)&255)});
        const auto command=extended?std::to_integer<std::uint8_t>(bytes[offset+4]):std::uint8_t{0};
        validate(result.inputs.back(),command);
        if(extended) result.plunger.push_back(command);
    }
    return result;
}
PadInput canonical_input(std::uint64_t frame) {
    PadInput input;
    if((frame>=180 && frame<185) || (frame>=240 && frame<245) || (frame>=300 && frame<305)) input.player_one=Pad::run;
    if(frame>=360) {
        if(frame%240<100) input.player_one |= Pad::button_i;
        if(frame%47<19) input.player_one |= Pad::left;
        if(frame%181==100) input.player_one |= Pad::button_ii;
    }
    return input;
}
std::string fingerprint(const Frame& frame, std::span<const std::byte> ram) {
    return std::to_string(frame.number)+","+std::to_string(frame.width)+","+std::to_string(frame.height)+","+
        rom::compute_digests(ram).sha256+","+rom::compute_digests(std::as_bytes(frame.pixels)).sha256+","+
        std::to_string(frame.stereo.size())+","+rom::compute_digests(std::as_bytes(frame.stereo)).sha256;
}
}
