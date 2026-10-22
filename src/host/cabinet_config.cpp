// SPDX-License-Identifier: GPL-3.0-or-later
#include "cabinet_config.hpp"
#include "cabinet_plunger.hpp"
#include "cabinet_settings.hpp"
#include "alien_crush/core/files.hpp"
#include "alien_crush/core/data_files.hpp"
#include "alien_crush/input/nudge_multiplier.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace alien_crush::host {
namespace {
constexpr std::size_t kMaximumConfigBytes=64U*1024U;
using Fields=std::map<std::string,std::string,std::less<>>;
[[noreturn]] void fail(const std::string& why) {throw std::runtime_error("alien-crush.ini: "+why);}

// Letters, digits and the punctuation of paths and names stay as they are, so
// "Left Shift,Left" and "D:\Games\Alien Crush.pce" read back verbatim; every
// other byte is %XX.
bool is_unescaped(unsigned char value) {
    return std::isalnum(value)!=0 || value=='-' || value=='_' || value=='.' || value=='/' ||
        value=='\\' || value==':' || value==' ';
}
std::string escape_value(std::string_view value) {
    constexpr char hex[]="0123456789ABCDEF";
    std::string escaped;
    for(const unsigned char byte:value) {
        if(is_unescaped(byte)) escaped.push_back(static_cast<char>(byte));
        else {escaped.push_back('%');escaped.push_back(hex[byte>>4]);escaped.push_back(hex[byte&15]);}
    }
    return escaped;
}
int hex_value(char value) {
    if(value>='0' && value<='9') return value-'0';
    if(value>='A' && value<='F') return value-'A'+10;
    if(value>='a' && value<='f') return value-'a'+10;
    return -1;
}
std::string unescape_value(std::string_view value,std::string_view key) {
    std::string out;
    for(std::size_t i=0;i<value.size();++i) {
        if(value[i]!='%') {out.push_back(value[i]);continue;}
        const int high=i+2<value.size()?hex_value(value[i+1]):-1,low=i+2<value.size()?hex_value(value[i+2]):-1;
        if(high<0 || low<0) fail("bad %-escape in "+std::string(key));
        out.push_back(static_cast<char>(high<<4|low));
        i+=2;
    }
    return out;
}
std::string_view trim(std::string_view text) {
    while(!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while(!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    return text;
}
template<typename Integer> Integer integer(std::string_view text,std::string_view key) {
    text=trim(text);
    Integer value{};
    const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(text.empty() || error!=std::errc{} || end!=text.data()+text.size()) fail("invalid number for "+std::string(key));
    return value;
}
bool flag(std::string_view text,std::string_view key) {
    const auto value=integer<int>(text,key);
    if(value!=0 && value!=1) fail(std::string(key)+" must be 0 or 1");
    return value!=0;
}
const std::string* field(const Fields& fields,std::string_view key) {
    const auto found=fields.find(key);
    return found==fields.end()?nullptr:&found->second;
}
bool quarter_turn(int degrees) {return degrees>=0 && degrees<=270 && degrees%90==0;}

void append_placement(std::string& out,std::string_view prefix,const WindowPlacement& p) {
    const auto append=[&](std::string_view key,const std::string& value) {
        out+=prefix;out+='.';out+=key;out+='=';out+=value;out+='\n';
    };
    append("display",escape_value(p.display_identity));
    append("x",std::to_string(p.geometry.x));
    append("y",std::to_string(p.geometry.y));
    append("width",std::to_string(p.geometry.width));
    append("height",std::to_string(p.geometry.height));
    append("rotation",std::to_string(static_cast<unsigned>(p.rotation)));
    append("fullscreen",p.fullscreen?"1":"0");
}
// A window is all seven lines or none: none is a window never placed.
void parse_placement(const Fields& fields,std::string_view prefix,WindowPlacement& p) {
    const auto get=[&](std::string_view suffix) {
        return field(fields,std::string(prefix)+'.'+std::string(suffix));
    };
    constexpr std::array suffixes{"display","x","y","width","height","rotation","fullscreen"};
    const auto present=std::ranges::count_if(suffixes,[&](const char* s){return get(s)!=nullptr;});
    if(present==0) return;
    if(present!=static_cast<long long>(suffixes.size())) fail(std::string(prefix)+" needs all of display, x, y, width, height, rotation and fullscreen");
    const auto key=[&](std::string_view suffix){return std::string(prefix)+'.'+std::string(suffix);};
    p.display_identity=unescape_value(*get("display"),key("display"));
    p.geometry={integer<int>(*get("x"),key("x")),integer<int>(*get("y"),key("y")),
        integer<int>(*get("width"),key("width")),integer<int>(*get("height"),key("height"))};
    const auto rotation=integer<unsigned>(*get("rotation"),key("rotation"));
    if(rotation>static_cast<unsigned>(DisplayRotation::portrait_flipped)) fail(key("rotation")+" must be 0 to 4");
    p.rotation=static_cast<DisplayRotation>(rotation);
    p.fullscreen=flag(*get("fullscreen"),key("fullscreen"));
    if(p.geometry.width<0 || p.geometry.height<0 || p.display_identity.size()>1024) fail(std::string(prefix)+" has an impossible size or name");
}

std::string join_keys(const std::vector<std::string>& keys) {
    std::string out;
    for(const auto& key:keys) {if(!out.empty()) out+=',';out+=escape_value(key);}
    return out;
}
std::vector<std::string> parse_keys(std::string_view text,std::string_view key) {
    std::vector<std::string> keys;
    for(std::size_t start=0;start<=text.size();) {
        const auto comma=text.find(',',start);
        const auto piece=trim(text.substr(start,comma==text.npos?text.npos:comma-start));
        if(!piece.empty()) {
            auto name=unescape_value(piece,key);
            // A key bound twice to one input is the same key.
            if(std::ranges::find(keys,name)==keys.end()) keys.push_back(std::move(name));
        }
        if(comma==text.npos) break;
        start=comma+1;
    }
    return keys;
}

// Written as plain numbers and a name, because the numbers are what the
// probes print for them and are typed straight back in here.
void append_bindings(std::string& out,const InputBindings& input) {
    for(std::size_t i=0;i<kPadButtonCount;++i) {
        const auto button=static_cast<PadButton>(i);
        out+="input.";out+=pad_button_field(button);out+='=';out+=join_keys(input[button]);out+='\n';
    }
    out+="input.smoothing="+join_keys(input.smoothing_keys)+'\n';
    out+="input.next_window="+join_keys(input.next_window_keys)+'\n';
    out+="input.fullscreen="+join_keys(input.fullscreen_keys)+'\n';
    out+="input.next_monitor="+join_keys(input.next_monitor_keys)+'\n';
    const auto& n=input.nudge;
    out+="input.nudge.device="+escape_value(n.device)+'\n';
    out+="input.nudge.enabled="+std::to_string(n.enabled)+'\n';
    out+="input.nudge.axis_x="+std::to_string(n.axis_x)+'\n';
    out+="input.nudge.axis_y="+std::to_string(n.axis_y)+'\n';
    out+="input.nudge.threshold="+std::to_string(n.threshold)+'\n';
    out+="input.nudge.release="+std::to_string(n.release)+'\n';
    out+="input.nudge.cooldown_ms="+std::to_string(n.cooldown_ms)+'\n';
    const auto& p=input.plunger;
    out+="input.plunger.device="+escape_value(p.device)+'\n';
    out+="input.plunger.axis="+std::to_string(p.axis)+'\n';
    out+="input.plunger.rest="+std::to_string(p.calibration.rest)+'\n';
    out+="input.plunger.drawn="+std::to_string(p.calibration.drawn)+'\n';
    out+="input.plunger.deadzone="+std::to_string(static_cast<unsigned>(p.calibration.deadzone))+'\n';
    out+="input.plunger.release="+std::to_string(static_cast<unsigned>(p.calibration.release))+'\n';
}
void parse_bindings(const Fields& fields,InputBindings& input) {
    input=default_input_bindings();
    for(std::size_t i=0;i<kPadButtonCount;++i) {
        const auto button=static_cast<PadButton>(i);
        const auto key="input."+std::string(pad_button_field(button));
        if(const auto* text=field(fields,key)) input[button]=parse_keys(*text,key);
    }
    if(const auto* text=field(fields,"input.smoothing")) input.smoothing_keys=parse_keys(*text,"input.smoothing");
    if(const auto* text=field(fields,"input.next_window")) input.next_window_keys=parse_keys(*text,"input.next_window");
    if(const auto* text=field(fields,"input.fullscreen")) input.fullscreen_keys=parse_keys(*text,"input.fullscreen");
    if(const auto* text=field(fields,"input.next_monitor")) input.next_monitor_keys=parse_keys(*text,"input.next_monitor");

    auto& n=input.nudge;
    for(const auto& [key,value]:fields) {
        if(!key.starts_with("input.nudge.")) continue;
        if(key=="input.nudge.device") n.device=unescape_value(trim(value),key);
        else if(key=="input.nudge.enabled") n.enabled=flag(value,key);
        else if(key=="input.nudge.axis_x") n.axis_x=integer<int>(value,key);
        else if(key=="input.nudge.axis_y") n.axis_y=integer<int>(value,key);
        else if(key=="input.nudge.threshold") n.threshold=integer<int>(value,key);
        else if(key=="input.nudge.release") n.release=integer<int>(value,key);
        else if(key=="input.nudge.cooldown_ms") n.cooldown_ms=integer<int>(value,key);
        else fail("unknown setting "+key);
    }
    try {validate_cabinet_nudge(n);} catch(const std::runtime_error& e) {fail(e.what());}

    // A percentage is range-checked and an axis value is not: the percentages
    // are the port's own units, the endpoints are whatever the board reports.
    auto& p=input.plunger;
    for(const auto& [key,value]:fields) {
        if(!key.starts_with("input.plunger.")) continue;
        if(key=="input.plunger.device") p.device=unescape_value(trim(value),key);
        else if(key=="input.plunger.axis") p.axis=integer<std::int32_t>(value,key);
        else if(key=="input.plunger.rest") p.calibration.rest=integer<std::int32_t>(value,key);
        else if(key=="input.plunger.drawn") p.calibration.drawn=integer<std::int32_t>(value,key);
        else if(key=="input.plunger.deadzone" || key=="input.plunger.release") {
            const auto percent=integer<unsigned>(value,key);
            if(percent>100) fail(key+" must be 0 to 100");
            (key=="input.plunger.deadzone"?p.calibration.deadzone:p.calibration.release)=static_cast<std::uint8_t>(percent);
        } else fail("unknown setting "+key);
    }
    if(p.device.find_first_of(std::string_view("\r\n\0",3))!=std::string::npos) fail("input.plunger.device cannot hold a line break");
}

// The multiplier as the shortest text that reads back as the same double, so
// a 2.5 typed in stays 2.5 rather than growing digits on every save.
bool append_play_settings(std::string& out,const PlaySettings& play) {
    if(!std::isfinite(play.nudge_multiplier) || play.nudge_multiplier<0 || play.nudge_multiplier>16) return false;
    if(play.bonus_stage>4) return false;
    std::array<char,32> multiplier{};
    const auto [end,error]=std::to_chars(multiplier.data(),multiplier.data()+multiplier.size(),play.nudge_multiplier);
    if(error!=std::errc{}) return false;
    out+="game.rom="+escape_value(play.rom)+'\n';
    out+="game.cabinet="+std::to_string(play.cabinet?1:0)+'\n';
    out+="game.bonus_stage="+std::to_string(play.bonus_stage)+'\n';
    out+="game.near_max_score="+std::to_string(play.near_max_score?1:0)+'\n';
    out+="game.drag_ball="+std::to_string(play.drag_ball?1:0)+'\n';
    out+="audio.music_voice_overlay="+std::to_string(play.music_voice_overlay?1:0)+'\n';
    out+="video.smoothing="+std::to_string(play.smoothing?1:0)+'\n';
    out+="input.nudge_multiplier=";out.append(multiplier.data(),end);out+='\n';
    return true;
}
// Each is optional, so a file from before them plays with the defaults.
void parse_play_settings(const Fields& fields,PlaySettings& play) {
    play={};
    if(const auto* rom=field(fields,"game.rom")) play.rom=unescape_value(trim(*rom),"game.rom");
    if(const auto* text=field(fields,"game.cabinet")) play.cabinet=flag(*text,"game.cabinet");
    if(const auto* text=field(fields,"game.bonus_stage")) {
        const auto stage=trim(*text);
        if(stage.size()!=1 || stage[0]<'0' || stage[0]>'4') fail("game.bonus_stage must be a number from 0 to 4");
        play.bonus_stage=static_cast<std::uint8_t>(stage[0]-'0');
    }
    if(const auto* text=field(fields,"game.near_max_score")) play.near_max_score=flag(*text,"game.near_max_score");
    if(const auto* text=field(fields,"game.drag_ball")) play.drag_ball=flag(*text,"game.drag_ball");
    if(const auto* text=field(fields,"audio.music_voice_overlay")) play.music_voice_overlay=flag(*text,"audio.music_voice_overlay");
    if(const auto* text=field(fields,"video.smoothing")) play.smoothing=flag(*text,"video.smoothing");
    if(const auto* text=field(fields,"input.nudge_multiplier")) {
        const auto value=input::parse_nudge_multiplier(trim(*text));
        if(!value) fail("input.nudge_multiplier must be a number from 0 to 16");
        play.nudge_multiplier=*value;
    }
}

// The older format, as the game read it before this one: a lenient
// key=value file with `window.<role>` in its own encoding, and whatever it
// lacked from the separate files beside it.
std::array<WindowPlacement,kCabinetWindowCount> parse_legacy_windows(const std::string& text) {
    // That file listed the backglass before the score window.
    constexpr std::array<const char*,kCabinetWindowCount> roles{"playfield","backglass","score"};
    constexpr std::array<CabinetWindow,kCabinetWindowCount> slots{playfield_window,backglass_window,score_window};
    std::istringstream stream(text);
    std::string magic;int version{};
    if(!(stream>>magic>>version) || magic!="ACWINDOWS" || version!=1) fail("unsupported window placement");
    std::array<WindowPlacement,kCabinetWindowCount> out;
    for(std::size_t i=0;i<out.size();++i) {
        std::string role;int x{},y{},w{},h{},fullscreen{},rotation{};
        auto& p=out[slots[i]];
        if(!(stream>>role>>std::quoted(p.display_identity)>>x>>y>>w>>h>>fullscreen>>rotation) || role!=roles[i] ||
            p.display_identity.size()>1024 || w<64 || h<64 || (fullscreen!=0 && fullscreen!=1) || !quarter_turn(rotation))
            fail("invalid window placement");
        p.geometry={x,y,w,h};p.fullscreen=fullscreen!=0;p.artwork_rotation=rotation;
    }
    stream>>std::ws;
    if(!stream.eof()) fail("unexpected window placement fields");
    return out;
}
CabinetConfig load_legacy(const CabinetSettings& settings,const std::filesystem::path& data) {
    CabinetConfig config;
    auto& play=config.play;
    if(const auto found=settings.find("game.rom");found!=settings.end()) play.rom=found->second;
    const auto legacy_flag=[&](const char* key,bool& value) {
        if(const auto found=settings.find(key);found!=settings.end()) {
            if(found->second!="0" && found->second!="1") fail(std::string(key)+" must be 0 or 1");
            value=found->second=="1";
        }
    };
    legacy_flag("game.cabinet",play.cabinet);
    legacy_flag("video.smoothing",play.smoothing);
    if(const auto found=settings.find("input.nudge_multiplier");found!=settings.end()) {
        const auto value=input::parse_nudge_multiplier(found->second);
        if(!value) fail("input.nudge_multiplier must be a number from 0 to 16");
        play.nudge_multiplier=*value;
    }
    config.input.nudge=parse_cabinet_nudge(settings);
    const bool plunger_here=std::ranges::any_of(settings,[](const auto& f){return f.first.starts_with("input.plunger.");});
    config.input.plunger=load_cabinet_plunger(plunger_here?data/core::kConfigFileName:data/"cabinet-plunger.txt");
    std::error_code error;
    if(settings.contains("window.playfield")) {
        std::string text="ACWINDOWS 1\n";
        for(const auto* role:{"playfield","backglass","score"}) {
            const auto found=settings.find(std::string("window.")+role);
            if(found==settings.end()) fail(std::string("window.")+role+" is missing");
            text+=std::string(role)+' '+found->second+'\n';
        }
        config.windows=parse_legacy_windows(text);
    } else if(std::filesystem::exists(data/"cabinet-windows.txt",error)) {
        const auto bytes=core::read_file(data/"cabinet-windows.txt",16384);
        config.windows=parse_legacy_windows({reinterpret_cast<const char*>(bytes.data()),bytes.size()});
    }
    return config;
}
}

DisplayRotation display_rotation(SDL_DisplayOrientation orientation) {
    switch(orientation) {
    case SDL_ORIENTATION_LANDSCAPE: return DisplayRotation::landscape;
    case SDL_ORIENTATION_LANDSCAPE_FLIPPED: return DisplayRotation::landscape_flipped;
    case SDL_ORIENTATION_PORTRAIT: return DisplayRotation::portrait;
    case SDL_ORIENTATION_PORTRAIT_FLIPPED: return DisplayRotation::portrait_flipped;
    default: return DisplayRotation::unknown;
    }
}

InputBindings default_input_bindings() {
    InputBindings bindings;
    // Return is I and Run together: it confirms and advances an initial as I,
    // starts and pauses as Run, and as I sends I alone from the shooter.
    // Neither flipper sends I, so no flipper press can launch.
    bindings[PadButton::one]={"Return"};
    bindings[PadButton::two]={"Space"};
    bindings[PadButton::select]={"Backspace"};
    bindings[PadButton::run]={"Return"};
    bindings[PadButton::up]={"Up"};
    bindings[PadButton::right]={"Right Shift","Right"};
    bindings[PadButton::down]={"Down"};
    bindings[PadButton::left]={"Left Shift","Left"};
    bindings.smoothing_keys={"F6"};
    bindings.next_window_keys={"F10"};
    bindings.fullscreen_keys={"F11"};
    bindings.next_monitor_keys={"F12"};
    return bindings;
}

std::vector<std::byte> encode_cabinet_config(const CabinetConfig& config) {
    if(config.format_version!=kCabinetConfigVersion) return {};
    for(const auto& window:config.windows) if(!quarter_turn(window.artwork_rotation)) return {};
    std::string text="version="+std::to_string(config.format_version)+'\n';
    for(std::size_t index=0;index<kCabinetWindowCount;++index)
        append_placement(text,kCabinetWindowFields[index],config.windows[index]);
    append_bindings(text,config.input);
    for(std::size_t index=0;index<kCabinetWindowCount;++index)
        text+=std::string(kCabinetWindowFields[index])+".artwork_rotation="+std::to_string(config.windows[index].artwork_rotation)+'\n';
    if(!append_play_settings(text,config.play)) return {};
    const auto bytes=std::as_bytes(std::span(text));
    return {bytes.begin(),bytes.end()};
}

CabinetConfig decode_cabinet_config(std::span<const std::byte> bytes) {
    if(bytes.empty() || bytes.size()>kMaximumConfigBytes) fail("the file is empty or larger than 64 KiB");
    std::string_view body(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    // Several Windows editors add a byte-order mark on save.
    if(body.starts_with("\xEF\xBB\xBF")) body.remove_prefix(3);
    Fields fields;
    for(std::size_t start=0;start<body.size();) {
        const auto end=body.find('\n',start);
        auto line=body.substr(start,end==body.npos?body.npos:end-start);
        if(!line.empty() && line.back()=='\r') line.remove_suffix(1);
        if(!line.empty()) {
            const auto separator=line.find('=');
            if(separator==line.npos || separator==0) fail("expected key=value, found \""+std::string(line)+'"');
            if(!fields.emplace(std::string(line.substr(0,separator)),std::string(line.substr(separator+1))).second)
                fail(std::string(line.substr(0,separator))+" appears twice");
        }
        if(end==body.npos) break;
        start=end+1;
    }
    CabinetConfig config;
    const auto* version=field(fields,"version");
    if(!version) fail("no version");
    config.format_version=integer<std::uint32_t>(*version,"version");
    if(config.format_version!=kCabinetConfigVersion) fail("version "+*version+" is not one this game reads");
    for(std::size_t i=0;i<kCabinetWindowCount;++i) {
        auto& window=config.windows[i];
        parse_placement(fields,kCabinetWindowFields[i],window);
        const auto key=std::string(kCabinetWindowFields[i])+".artwork_rotation";
        if(const auto* text=field(fields,key)) {
            window.artwork_rotation=integer<int>(*text,key);
            if(!quarter_turn(window.artwork_rotation)) fail(key+" must be 0, 90, 180 or 270");
        }
    }
    parse_bindings(fields,config.input);
    parse_play_settings(fields,config.play);
    return config;
}

ConfigLoadResult load_cabinet_config(const std::filesystem::path& data) {
    const auto path=data/core::kConfigFileName;
    ConfigLoadResult result;
    try {
        std::error_code error;
        const bool exists=std::filesystem::exists(path,error);
        if(error) return {ConfigLoadStatus::io_error,{},"Could not inspect "+path.string()+": "+error.message()};
        std::vector<std::byte> bytes;
        if(exists) {
            try {bytes=core::read_file(path,kMaximumConfigBytes);}
            catch(const std::exception& e) {return {ConfigLoadStatus::io_error,{},e.what()};}
        }
        // A file with a version is this format; one without is the older one.
        std::string_view text(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        if(text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
        const bool current=text.starts_with("version=") || text.find("\nversion=")!=text.npos;
        if(current) {
            result.config=decode_cabinet_config(bytes);
            result.status=ConfigLoadStatus::loaded;
            result.message="Cabinet configuration loaded.";
            return result;
        }
        const bool older_files=std::ranges::any_of(std::array{"cabinet-windows.txt","cabinet-inputs.txt","cabinet-plunger.txt"},
            [&](const char* name){return std::filesystem::exists(data/name,error);});
        if(!exists && !older_files) return {ConfigLoadStatus::not_found,{},"No cabinet configuration exists."};
        result.config=load_legacy(exists?read_cabinet_settings(path):CabinetSettings{},data);
        result.status=ConfigLoadStatus::loaded;
        result.legacy=true;
        result.message="Cabinet configuration loaded from the older format.";
        return result;
    } catch(const std::exception& e) {
        return {ConfigLoadStatus::invalid,{},e.what()};
    }
}

void save_cabinet_config(const std::filesystem::path& path,const CabinetConfig& config) {
    const auto bytes=encode_cabinet_config(config);
    if(bytes.empty()) throw std::runtime_error("The cabinet configuration could not be encoded");
    if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    core::write_file_atomic(path,bytes);
}
}
