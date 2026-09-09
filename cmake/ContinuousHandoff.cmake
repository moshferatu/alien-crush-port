# SPDX-License-Identifier: GPL-3.0-or-later
# Patches generated copies only. The default reference execution is unchanged.
function(ac_prepare_continuous_handoff root)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/runtime/handoff_hook.inc")
    file(READ "${root}/huc6280.cpp" cpu)
    set(anchor "\t\tlastop = RdOp(PC);")
    string(FIND "${cpu}" "${anchor}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Pinned handoff CPU anchor missing")
    endif()
    file(READ "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/runtime/handoff_hook.inc" hook)
    string(REPLACE "${anchor}" "${hook}\n${anchor}" cpu "${cpu}")
    set(prefix [=[
#include "handoff_packets.hpp"
#include "handoff_audit.hpp"
static alien_crush::handoff::Audit ac_handoff_audit;
static void (*ac_handoff_boundary)(unsigned,const unsigned char*) = nullptr;
extern "C" void ac_set_handoff_boundary(void (*callback)(unsigned,const unsigned char*)) { ac_handoff_boundary=callback; }
extern "C" void ac_set_handoff_audit(bool enabled) { ac_handoff_audit={};ac_handoff_audit.enabled=enabled; }
extern "C" bool ac_handoff_audit_result(unsigned long long* checked) {
    *checked=ac_handoff_audit.checked;return !ac_handoff_audit.failed && !ac_handoff_audit.pending;
}
static bool ac_continuous_handoff = false;
static unsigned long long ac_handoff_packets = 0, ac_handoff_waits = 0;
extern "C" void ac_set_continuous_handoff(bool enabled) {
    ac_continuous_handoff=enabled; ac_handoff_packets=ac_handoff_waits=0;
}
extern "C" void ac_handoff_counts(unsigned long long* packets,unsigned long long* waits) {
    *packets=ac_handoff_packets; *waits=ac_handoff_waits;
}
extern "C" bool ac_handoff_video(const unsigned short*,unsigned);
]=])
    file(WRITE "${root}/huc6280.cpp" "${prefix}${cpu}")
    file(READ "${root}/vdc.cpp" vdc)
    set(anchor "VDC::VDC()\n{")
    string(FIND "${vdc}" "${anchor}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Pinned handoff VDC anchor missing")
    endif()
    set(hook [=[
    ac_handoff_video_peek_device = [](void* object,unsigned index) -> unsigned {
        auto* self=static_cast<VDC*>(object);
        if(index==32768) return self->MAWR;
        if(index==32769) return self->CR;
        // The original CPU can return before the last queued bus write commits.
        if(self->pending_write && self->pending_write_addr==index) return self->pending_write_latch;
        return self->PeekVRAM(static_cast<uint16>(index));
    };
    ac_handoff_video_write = [](void* object,const unsigned short* words,unsigned count) {
        auto* self=static_cast<VDC*>(object);
        if(self->select!=2 || !count || count>256 || self->DMARunning || self->sat_dma_counter>0) return false;
        // Finish a preceding bus write before installing the resident packet.
        if(self->pending_write) self->PokeVRAM(self->pending_write_addr,self->pending_write_latch);
        self->pending_write=false;
        for(unsigned i=0;i<count;++i) {
            self->pending_write_addr=self->MAWR;
            self->pending_write_latch=words[i];
            self->write_latch=static_cast<uint8>(words[i]);
            self->PokeVRAM(self->MAWR,words[i]);
            self->MAWR+=vram_inc_tab[(self->CR>>11)&3];
        }
        return true;
    };
]=])
    string(REPLACE "${anchor}" "${anchor}\n${hook}" vdc "${vdc}")
    set(prefix [=[
bool (*ac_handoff_video_write)(void*,const unsigned short*,unsigned) = nullptr;
unsigned (*ac_handoff_video_peek_device)(void*,unsigned) = nullptr;
]=])
    file(WRITE "${root}/vdc.cpp" "${prefix}${vdc}")
    file(READ "${root}/vce.cpp" vce)
    set(anchor "    ac_video_instance = this;")
    string(FIND "${vce}" "${anchor}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Pinned handoff VCE anchor missing")
    endif()
    set(hook [=[
    ac_handoff_video_dispatch = [](void* object,const unsigned short* words,unsigned count) {
        auto* self=static_cast<VCE*>(object);
        return ac_handoff_video_write && ac_handoff_video_write(&self->vdc[0],words,count);
    };
    ac_handoff_peek_dispatch = [](void* object,unsigned index) -> unsigned {
        auto* self=static_cast<VCE*>(object);
        return ac_handoff_video_peek_device(&self->vdc[0],index);
    };
]=])
    string(REPLACE "${anchor}" "${anchor}\n${hook}" vce "${vce}")
    set(prefix [=[
extern bool (*ac_handoff_video_write)(void*,const unsigned short*,unsigned);
extern unsigned (*ac_handoff_video_peek_device)(void*,unsigned);
static bool (*ac_handoff_video_dispatch)(void*,const unsigned short*,unsigned) = nullptr;
static unsigned (*ac_handoff_peek_dispatch)(void*,unsigned) = nullptr;
]=])
    set(suffix [=[
extern "C" bool ac_handoff_video(const unsigned short* words,unsigned count) {
    return ac_video_instance && ac_handoff_video_dispatch && ac_handoff_video_dispatch(ac_video_instance,words,count);
}
extern "C" unsigned ac_handoff_video_peek(unsigned index) {
    return ac_handoff_peek_dispatch(ac_video_instance,index);
}
]=])
    file(WRITE "${root}/vce.cpp" "${prefix}${vce}\n${suffix}")
endfunction()
