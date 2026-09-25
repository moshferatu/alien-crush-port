# SPDX-License-Identifier: GPL-3.0-or-later
# A second PSG for the music voice overlay (src/runtime/music_voice_overlay.hpp),
# added to a generated copy of pce.cpp. It is idle unless the host enables it,
# and then only adds to the output; the machine, its state and the physical
# PSG are untouched. The sound driver's steps reach it through the CPU hook
# (src/runtime/handoff_hook.inc).
function(ac_prepare_music_voice_overlay machine_source output_root)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/runtime/music_voice_overlay.hpp")
    file(MAKE_DIRECTORY "${output_root}")
    file(READ "${machine_source}" machine)
    # The pinned sources have CRLF lines; the anchors below are written with LF.
    string(REPLACE "\r\n" "\n" machine "${machine}")
    function(ac_overlay_patch anchor replacement)
        string(FIND "${machine}" "${anchor}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Pinned music voice overlay anchor missing: ${anchor}")
        endif()
        string(REPLACE "${anchor}" "${replacement}" machine "${machine}")
        set(machine "${machine}" PARENT_SCOPE)
    endfunction()
    ac_overlay_patch("static OwlResampler* HRRes = NULL;" [=[static OwlResampler* HRRes = NULL;

#include "music_voice_overlay.hpp"
static PCE_PSG *ac_overlay_psg = NULL;
static alien_crush::audio::MusicVoiceOverlay *ac_overlay_model = NULL;
static int ac_psg_revision = PCE_PSG::REVISION_HUC6280;
extern "C" { void (*ac_music_overlay_step)(unsigned,unsigned,unsigned,unsigned) = NULL; }
static void ac_overlay_step(unsigned pc,unsigned a,unsigned x,unsigned y)
{
	const int32 ts = HuCPU.Timestamp() / 3;
	for(const auto& w : ac_overlay_model->step((uint16)pc, (uint8)a, (uint8)x, (uint8)y))
		ac_overlay_psg->Write(ts, w.address, w.value);
}
// Silent from now, knowing nothing, with the physical PSG's main volume; the
// next push programs it again.
static void ac_overlay_restart(void)
{
	if(!ac_overlay_psg)
		return;
	const int32 ts = HuCPU.Timestamp() / 3;
	ac_overlay_psg->ResetTS(ts);
	ac_overlay_psg->Power(ts);
	ac_overlay_psg->Write(ts, 0x01, psg ? (uint8)psg->GetRegister(PSG_GSREG_GBALANCE) : 0);
	ac_overlay_model->reset();
}
static void ac_overlay_free(void)
{
	ac_music_overlay_step = NULL;
	delete ac_overlay_psg;
	ac_overlay_psg = NULL;
	delete ac_overlay_model;
	ac_overlay_model = NULL;
}
extern "C" bool ac_set_music_voice_overlay(bool enabled, const unsigned char* waveforms, unsigned size)
{
	ac_overlay_free();
	if(!enabled)
		return true;
	if(!psg || !HRBufs[0] || !HRBufs[1] || !waveforms)
		return false;
	ac_overlay_psg = new PCE_PSG(HRBufs[0]->Buf(), HRBufs[1]->Buf(), ac_psg_revision);
	ac_overlay_psg->SetVolume(1.0);
	ac_overlay_model = new alien_crush::audio::MusicVoiceOverlay(
		std::span<const std::byte>(reinterpret_cast<const std::byte*>(waveforms), size));
	ac_overlay_restart();
	ac_music_overlay_step = ac_overlay_step;
	return true;
}]=])
    ac_overlay_patch("psg->Write(HuCPU.Timestamp() / 3, A & 0x1FFF, V);" [=[psg->Write(HuCPU.Timestamp() / 3, A & 0x1FFF, V);
			if(ac_overlay_psg && (A & 0x0F) == 0x01)
				ac_overlay_psg->Write(HuCPU.Timestamp() / 3, 0x01, V);]=])
    ac_overlay_patch("	psg = new PCE_PSG(HRBufs[0]->Buf(), HRBufs[1]->Buf(), psgrevision);" [=[	ac_psg_revision = psgrevision;
	psg = new PCE_PSG(HRBufs[0]->Buf(), HRBufs[1]->Buf(), psgrevision);]=])
    ac_overlay_patch("	if(psg)\n	{\n		delete psg;" "	ac_overlay_free();\n	if(psg)\n	{\n		delete psg;")
    ac_overlay_patch("		psg->ResetTS(end_timestamp_mod12 / 3);" [=[		psg->ResetTS(end_timestamp_mod12 / 3);
		if(ac_overlay_psg)
		{
			ac_overlay_psg->Update(end_timestamp / 3);
			ac_overlay_psg->ResetTS(end_timestamp_mod12 / 3);
		}]=])
    ac_overlay_patch("	ret &= HuC_StateAction(sm, load, data_only);" [=[	ret &= HuC_StateAction(sm, load, data_only);
	if(load)
		ac_overlay_restart();]=])
    ac_overlay_patch("	psg->Power(timestamp / 3);" "	psg->Power(timestamp / 3);\n	ac_overlay_restart();")
    file(WRITE "${output_root}/pce.cpp" "${machine}")
endfunction()
