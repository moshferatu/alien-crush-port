# SPDX-License-Identifier: GPL-3.0-or-later
# Read-only getters on generated copies; no emulated reads, synchronization or writes.
function(ac_prepare_reference_snapshot cpu_input vce_input output_root)
    file(READ "${cpu_input}" cpu)
    set(anchor "HuC6280::HuC6280()\n{")
    string(FIND "${cpu}" "${anchor}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Pinned CPU snapshot constructor anchor missing")
    endif()
    set(hook [=[
    ac_cpu_instance = this;
    ac_cpu_copy = [](void* object, unsigned short* pc, unsigned char* mpr) {
        auto* self = static_cast<HuC6280*>(object);
        *pc = static_cast<unsigned short>(self->PC);
        for(unsigned i = 0; i < 8; ++i) mpr[i] = self->MPR[i];
    };
]=])
    string(REPLACE "${anchor}" "${anchor}\n${hook}" cpu "${cpu}")
    set(prefix [=[
static void* ac_cpu_instance = nullptr;
static void (*ac_cpu_copy)(void*, unsigned short*, unsigned char*) = nullptr;
extern "C" bool ac_copy_cpu_snapshot(unsigned short* pc, unsigned char* mpr) {
    if(!ac_cpu_instance || !ac_cpu_copy) return false;
    ac_cpu_copy(ac_cpu_instance, pc, mpr); return true;
}
]=])
    file(WRITE "${output_root}/huc6280.cpp" "${prefix}${cpu}")
    file(READ "${vce_input}" vce)
    set(anchor "VCE::VCE(const bool want_sgfx, const uint32 vram_size)\n{")
    string(FIND "${vce}" "${anchor}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Pinned VCE snapshot constructor anchor missing")
    endif()
    set(hook [=[
    ac_video_instance = this;
    ac_video_copy = [](void* object, unsigned short* vram, unsigned short* palette,
        unsigned short* sat, unsigned short* registers) {
        auto* self = static_cast<VCE*>(object);
        for(unsigned i = 0; i < 32768; ++i) vram[i] = self->vdc[0].PeekVRAM(static_cast<uint16>(i));
        for(unsigned i = 0; i < 512; ++i) palette[i] = self->color_table[i];
        for(unsigned i = 0; i < 256; ++i) sat[i] = self->vdc[0].PeekSAT(static_cast<uint8>(i));
        registers[0] = self->vdc[0].GetRegister(VDC::GSREG_CR);
        registers[1] = self->vdc[0].GetRegister(VDC::GSREG_MWR);
        registers[2] = self->vdc[0].GetRegister(VDC::GSREG_BXR);
        registers[3] = self->vdc[0].GetRegister(VDC::GSREG_BYR);
    };
]=])
    string(REPLACE "${anchor}" "${anchor}\n${hook}" vce "${vce}")
    set(anchor "VCE::~VCE()\n{")
    string(FIND "${vce}" "${anchor}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Pinned VCE snapshot destructor anchor missing")
    endif()
    string(REPLACE "${anchor}" "${anchor}\n    if(ac_video_instance == this) { ac_video_instance = nullptr; ac_video_copy = nullptr; }" vce "${vce}")
    set(prefix [=[
static void* ac_video_instance = nullptr;
static void (*ac_video_copy)(void*, unsigned short*, unsigned short*, unsigned short*, unsigned short*) = nullptr;
extern "C" bool ac_copy_video_snapshot(unsigned short* vram, unsigned short* palette,
    unsigned short* sat, unsigned short* registers) {
    if(!ac_video_instance || !ac_video_copy) return false;
    ac_video_copy(ac_video_instance, vram, palette, sat, registers); return true;
}
]=])
    file(WRITE "${output_root}/vce.cpp" "${prefix}${vce}")
endfunction()
