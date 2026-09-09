// SPDX-License-Identifier: GPL-3.0-or-later
#include "backglass.hpp"
#include "alien_crush/core/files.hpp"
#include <iostream>
#include <stdexcept>
using namespace alien_crush;
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
void put(std::vector<std::byte>& bytes,std::size_t p,std::uint32_t value) {
    for(unsigned i=0;i<4;++i) bytes.at(p+i)=static_cast<std::byte>((value>>(8*i))&255);
}
std::vector<std::byte> bmp(bool top,bool wide) {
    // Three columns exercise 24-bit row padding; rows and corners are distinct.
    const unsigned stride=12;
    std::vector<std::byte> bytes(54+stride*2);
    bytes[0]=std::byte{'B'}; bytes[1]=std::byte{'M'};
    put(bytes,2,static_cast<std::uint32_t>(bytes.size())); put(bytes,10,54);
    put(bytes,14,40); put(bytes,18,3); put(bytes,22,top?0xFFFFFFFEU:2U);
    put(bytes,26,1U|((wide?32U:24U)<<16));
    for(unsigned y=0;y<2;++y) for(unsigned x=0;x<3;++x) {
        const auto p=54+(top?y:1-y)*stride+x*(wide?4:3);
        bytes[p]=static_cast<std::byte>(x+1);
        bytes[p+1]=static_cast<std::byte>(y+1);
        bytes[p+2]=std::byte{0x80};
        if(wide) bytes[p+3]=static_cast<std::byte>(x*100); // Reserved byte is ignored.
    }
    return bytes;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected fixture path");
        const std::filesystem::path path=argv[1];
        require(!host::load_backglass(path.string()+".missing"),"Missing artwork must be optional");
        for(bool top:{false,true}) for(bool wide:{false,true}) {
            const auto data=bmp(top,wide); core::write_file_atomic(path,data);
            const auto image=host::load_backglass(path);
            require(image && image->width==3 && image->height==2,"Wrong dimensions");
            require(image->pixels==std::vector<std::uint32_t>{0xFF800101,0xFF800102,0xFF800103,
                0xFF800201,0xFF800202,0xFF800203},"BMP orientation, padding or channels differ");
        }
        for(unsigned case_id=0;case_id<13;++case_id) {
            auto data=bmp(false,false);
            switch(case_id) {
            case 0:data.resize(53);break;
            case 1:data[0]=std::byte{'X'};break;
            case 2:put(data,14,0xFFFFFFFF);break;
            case 3:put(data,10,0xFFFFFFFF);break;
            case 4:put(data,10,40);break;
            case 5:put(data,18,4097);break;
            case 6:put(data,22,0x80000000);break;
            case 7:put(data,22,0);break;
            case 8:put(data,26,1|(8<<16));break;
            case 9:put(data,30,1);break;
            case 10:data.pop_back();break;
            case 11:put(data,18,0);break;
            case 12:put(data,26,2|(24<<16));break;
            }
            core::write_file_atomic(path,data);
            bool rejected=false;
            try {(void)host::load_backglass(path);} catch(const std::runtime_error&) {rejected=true;}
            require(rejected,"Malformed artwork was accepted");
            require(core::read_file(path)==data,"Rejected artwork was modified");
        }
        core::write_file_atomic(path,bmp(false,false));
        std::cout<<"PASS: BMP orientation, padding, opaque colors, missing and invalid artwork\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
