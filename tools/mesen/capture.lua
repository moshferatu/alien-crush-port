-- SPDX-License-Identifier: GPL-3.0-or-later
-- Read-only capture and real controller stimulus; never patch game RAM or ROM.
local directory = assert(os.getenv('AC_MESEN_OUTPUT'))
local frames = tonumber(os.getenv('AC_MESEN_FRAMES')) or 1800
local ram = assert(io.open(directory .. '/work-ram.bin', 'wb'))
local pixels = assert(io.open(directory .. '/pixels.rgb', 'wb'))
local cpu = assert(io.open(directory .. '/cpu.csv', 'w'))
local frame_count = 0
local trace_frames=tonumber(os.getenv('AC_MESEN_TRACE_VIDEO')) or 0
local video_trace=nil
local actor_frames=tonumber(os.getenv('AC_MESEN_TRACE_ACTORS')) or 0
local actor_trace,actor_phases=nil,nil
local overlay_trace,overlay_pending=nil,nil
local overlay_pairs,overlay_orphans=0,0
local actor_pending=nil
local actor_pairs,actor_orphans=0,0
local background_trace,background_pending=nil,nil
local background_pairs,background_orphans=0,0
local pattern_trace,pattern_pending=nil,nil
local pattern_pairs=0
local fade_trace,fade_pending=nil,nil
local fade_pairs,fade_orphans=0,0
local fade_out_trace,fade_out_pending=nil,nil
local fade_out_pairs,fade_out_orphans=0,0
local pulse_trace,pulse_pending=nil,nil
local pulse_pairs,pulse_orphans=0,0
if actor_frames>0 then
  actor_trace=assert(io.open(directory..'/actor-passes.bin','wb'))
  actor_trace:write('ACAT0001')
  overlay_trace=assert(io.open(directory..'/overlay-passes.bin','wb'))
  overlay_trace:write('ACOV0001')
  background_trace=assert(io.open(directory..'/background-passes.bin','wb'))
  background_trace:write('ACBG0001')
  pattern_trace=assert(io.open(directory..'/pattern-passes.bin','wb'))
  pattern_trace:write('ACPT0001')
  fade_trace=assert(io.open(directory..'/palette-fades.bin','wb'))
  fade_trace:write('ACPF0001')
  fade_out_trace=assert(io.open(directory..'/palette-fade-outs.bin','wb'))
  fade_out_trace:write('ACFD0001')
  pulse_trace=assert(io.open(directory..'/palette-pulses.bin','wb'))
  pulse_trace:write('ACPZ0001')
  actor_phases=assert(io.open(directory..'/actor-phases.csv','w'))
  actor_phases:write('frame,address,phase,table,slot,clock\n')
  local function memory_bytes(kind,start,count)
    local bytes={}
    for i=start,start+count-1 do bytes[#bytes+1]=string.char(emu.read(i,kind)) end
    return table.concat(bytes)
  end
  local points={[0x45a8]='update-entry',[0x45ce]='ball-update',[0x45e2]='render-entry',
    [0x4666]='main-pass-end',[0x46ac]='render-return'}
  for address,phase in pairs(points) do
    emu.addMemoryCallback(function()
      if frame_count<frames-actor_frames then return end
      local state=emu.getState()
      if state['memoryManager.mpr[2]']~=1 then return end
      local slot=emu.read(0x2d,emu.memType.pceWorkRam)
      actor_phases:write(string.format('%d,%04X,%s,%d,%d,%s\n',frame_count+1,address,phase,
        emu.read(0x79,emu.memType.pceWorkRam),slot,tostring(state.masterClock)))
      if address==0x45e2 then
        assert(not actor_pending,'Nested actor render pass')
        actor_pending={frame=frame_count+1,ram=memory_bytes(emu.memType.pceWorkRam,0,8192)}
      elseif address==0x4666 then
        if actor_pending then
          actor_trace:write(string.pack('<I4I4',actor_pending.frame,frame_count+1),actor_pending.ram,
            memory_bytes(emu.memType.pceVideoRam,0x800,512),string.char(slot))
          actor_pairs=actor_pairs+1
          actor_pending=nil
        else actor_orphans=actor_orphans+1 end
        assert(not overlay_pending,'Nested table overlay pass')
        overlay_pending={frame=frame_count+1,ram=memory_bytes(emu.memType.pceWorkRam,0,8192)}
      elseif address==0x46ac then
        if overlay_pending then
          overlay_trace:write(string.pack('<I4I4',overlay_pending.frame,frame_count+1),overlay_pending.ram,
            memory_bytes(emu.memType.pceVideoRam,0x800,512),string.char(slot))
          overlay_pairs=overlay_pairs+1
          overlay_pending=nil
        else overlay_orphans=overlay_orphans+1 end
      end
      -- Observe instruction boundaries without replacing values or writing memory.
    end,emu.callbackType.exec,address,address,emu.cpuType.pce,emu.memType.pceMemory)
  end
  for _,address in ipairs({0xa1c3,0xa24a}) do
    emu.addMemoryCallback(function()
      if frame_count<frames-actor_frames then return end
      local state=emu.getState()
      if state['memoryManager.mpr[5]']~=0x16 then return end
      if address==0xa1c3 then
        assert(not background_pending,'Nested actor background update')
        background_pending={frame=frame_count+1,actor=state['cpu.x'],
          ram=memory_bytes(emu.memType.pceWorkRam,0,8192),
          vram=memory_bytes(emu.memType.pceVideoRam,0,65536)}
      elseif background_pending then
        local actor=background_pending.actor
        background_trace:write(string.pack('<I4I4',background_pending.frame,frame_count+1),
          string.char(actor),background_pending.ram,background_pending.vram,
          memory_bytes(emu.memType.pceVideoRam,0,65536),
          string.char(emu.read(0x1540+actor,emu.memType.pceWorkRam)))
        background_pending=nil; background_pairs=background_pairs+1
      else background_orphans=background_orphans+1 end
    end,emu.callbackType.exec,address,address,emu.cpuType.pce,emu.memType.pceMemory)
  end
  for _,address in ipairs({0xa0df,0xebb1}) do
    emu.addMemoryCallback(function()
      if frame_count<frames-actor_frames then return end
      local state=emu.getState()
      if address==0xa0df then
        if state['memoryManager.mpr[5]']~=0x16 then return end
        assert(not pattern_pending,'Nested dynamic actor pattern transfer')
        pattern_pending={frame=frame_count+1,actor=state['cpu.x'],
          ram=memory_bytes(emu.memType.pceWorkRam,0,8192),vram=memory_bytes(emu.memType.pceVideoRam,0,65536)}
      elseif pattern_pending and state['memoryManager.mpr[7]']==0 then
        local actor=pattern_pending.actor
        pattern_trace:write(string.pack('<I4I4',pattern_pending.frame,frame_count+1),string.char(actor),
          pattern_pending.ram,pattern_pending.vram,memory_bytes(emu.memType.pceVideoRam,0,65536),
          string.char(emu.read(0x1540+actor,emu.memType.pceWorkRam)))
        pattern_pending=nil; pattern_pairs=pattern_pairs+1
      end
    end,emu.callbackType.exec,address,address,emu.cpuType.pce,emu.memType.pceMemory)
  end
  for _,address in ipairs({0xe4b7,0xe5cc}) do
    emu.addMemoryCallback(function()
      if frame_count<frames-actor_frames then return end
      local state=emu.getState()
      if state['memoryManager.mpr[7]']~=0 then return end
      if address==0xe4b7 then
        assert(not fade_pending,'Nested palette fade step')
        local source=emu.read(0x0a,emu.memType.pceWorkRam)+256*emu.read(0x0b,emu.memType.pceWorkRam)
        assert(source>=0x4000 and source+1024<=0x10000,'Palette target outside ROM window')
        fade_pending={frame=frame_count+1,threshold=emu.read(0x0c,emu.memType.pceWorkRam),
          current=memory_bytes(emu.memType.pceWorkRam,0xa00,1024),
          target=memory_bytes(emu.memType.pceMemory,source,1024)}
      elseif fade_pending then
        fade_trace:write(string.pack('<I4I4',fade_pending.frame,frame_count+1),string.char(fade_pending.threshold),
          fade_pending.current,fade_pending.target,memory_bytes(emu.memType.pceWorkRam,0xa00,1024))
        fade_pending=nil; fade_pairs=fade_pairs+1
      else fade_orphans=fade_orphans+1 end
    end,emu.callbackType.exec,address,address,emu.cpuType.pce,emu.memType.pceMemory)
  end
  for _,address in ipairs({0xe605,0xe6aa}) do
    emu.addMemoryCallback(function()
      if frame_count<frames-actor_frames then return end
      local state=emu.getState()
      if state['memoryManager.mpr[7]']~=0 then return end
      if address==0xe605 then
        assert(not fade_out_pending,'Nested palette fade-out step')
        fade_out_pending={frame=frame_count+1,current=memory_bytes(emu.memType.pceWorkRam,0xa00,1024)}
      elseif fade_out_pending then
        local black=emu.read(6,emu.memType.pceWorkRam)+256*emu.read(7,emu.memType.pceWorkRam)
        fade_out_trace:write(string.pack('<I4I4I2',fade_out_pending.frame,frame_count+1,black),
          fade_out_pending.current,memory_bytes(emu.memType.pceWorkRam,0xa00,1024))
        fade_out_pending=nil; fade_out_pairs=fade_out_pairs+1
      else fade_out_orphans=fade_out_orphans+1 end
    end,emu.callbackType.exec,address,address,emu.cpuType.pce,emu.memType.pceMemory)
  end
  for _,address in ipairs({0xf64b,0xe1d3}) do
    emu.addMemoryCallback(function()
      if frame_count<frames-actor_frames then return end
      local state=emu.getState()
      if state['memoryManager.mpr[7]']~=0 then return end
      if address==0xf64b then
        assert(not pulse_pending,'Nested palette pulse call')
        pulse_pending={frame=frame_count+1,state=memory_bytes(emu.memType.pceWorkRam,0x8a,4),
          palette=memory_bytes(emu.memType.pcePaletteRam,0,1024)}
      elseif pulse_pending then
        pulse_trace:write(string.pack('<I4I4',pulse_pending.frame,frame_count+1),pulse_pending.state,
          memory_bytes(emu.memType.pceWorkRam,0x8a,4),pulse_pending.palette,
          memory_bytes(emu.memType.pcePaletteRam,0,1024))
        pulse_pending=nil; pulse_pairs=pulse_pairs+1
      else pulse_orphans=pulse_orphans+1 end
    end,emu.callbackType.exec,address,address,emu.cpuType.pce,emu.memType.pceMemory)
  end
end
if trace_frames>0 then
  video_trace=assert(io.open(directory..'/video-writes.csv','w'))
  video_trace:write('frame,pc,mpr,port,register,value,a,x,y\n')
  local selected=0
  emu.addMemoryCallback(function(address,value)
    if address==0 then selected=value end
    if frame_count>=frames-trace_frames then
      local state=emu.getState()
      local pc=state['cpu.pc']
      video_trace:write(string.format('%d,%04X,%02X,%04X,%02X,%02X,%02X,%02X,%02X\n',
        frame_count+1,pc,state['memoryManager.mpr['..(pc>>13)..']'],address,selected,value,
        state['cpu.a'],state['cpu.x'],state['cpu.y']))
    end
    -- No return value: preserve the original bus write.
  end,emu.callbackType.write,0,3,emu.cpuType.pce,emu.memType.pceMemory)
end
cpu:write('frame,pc,sp,clock,mpr0,mpr1,mpr2,mpr3,mpr4,mpr5,mpr6,mpr7\n')
local function dump_memory(name, kind)
  local file = assert(io.open(directory .. '/' .. name .. '-' .. frame_count .. '.bin', 'wb'))
  local bytes = {}
  for i=0,emu.getMemorySize(kind)-1 do bytes[#bytes+1]=string.char(emu.read(i,kind)) end
  file:write(table.concat(bytes)); file:close()
end
local function input()
  local n = emu.getState().frameCount
  emu.setInput({
    run = (n>=180 and n<185) or (n>=240 and n<245) or (n>=300 and n<305),
    i = n>=360 and n%240<100,
    ii = n>=360 and n%181==100,
    left = n>=360 and n%47<19,
    select=false, up=false, down=false, right=false
  },0)
end
local function end_frame()
  frame_count=frame_count+1
  assert(emu.getMemorySize(emu.memType.pceWorkRam)==8192)
  local bytes={}
  for i=0,8191 do bytes[#bytes+1]=string.char(emu.read(i,emu.memType.pceWorkRam)) end
  ram:write(table.concat(bytes))
  local size=emu.getScreenSize()
  if frame_count==1 then pixels:write('ACF1',string.pack('<I4I4I4',size.width,size.height,frames)) end
  local screen=emu.getScreenBuffer()
  local rgb={}
  for i,p in ipairs(screen) do rgb[i]=string.char((p>>16)&255,(p>>8)&255,p&255) end
  pixels:write(table.concat(rgb))
  local state=emu.getState()
  local fields={frame_count,state['cpu.pc'],state['cpu.sp'],state.masterClock}
  for i=0,7 do fields[#fields+1]=state['memoryManager.mpr['..i..']'] end
  cpu:write(table.concat(fields,','),'\n')
  if frame_count%120==0 then
    dump_memory('vram',emu.memType.pceVideoRam)
    dump_memory('sat',emu.memType.pceSpriteRam)
    dump_memory('palette',emu.memType.pcePaletteRam)
    local file=assert(io.open(directory..'/state-'..frame_count..'.txt','w'))
    local keys={}; for key,_ in pairs(state) do keys[#keys+1]=key end; table.sort(keys)
    for _,key in ipairs(keys) do file:write(key,'=',tostring(state[key]),'\n') end
    file:close()
  end
  if frame_count==frames then
    ram:close(); pixels:close(); cpu:close()
    if video_trace then video_trace:close() end
    if actor_trace then
      actor_trace:close(); actor_phases:close(); overlay_trace:close(); background_trace:close()
      pattern_trace:close()
      fade_trace:close()
      fade_out_trace:close()
      pulse_trace:close()
      local manifest=assert(io.open(directory..'/actor-trace-manifest.txt','w'))
      manifest:write('schema=ACAT0001\nrom_sha256=1af27c808be34e0c2fe435c169f5169417d6362dee0e928413581a1f6126557f\n',
        'backend=Mesen-2.1.1\nroute=canonical\nfirst_frame=',math.max(1,frames-actor_frames+1),
        '\nlast_frame=',frames,'\ncomplete_pairs=',actor_pairs,'\norphan_ends=',actor_orphans,
        '\npending_entry=',actor_pending and 1 or 0,
        '\noverlay_schema=ACOV0001\noverlay_complete_pairs=',overlay_pairs,
        '\noverlay_orphan_ends=',overlay_orphans,'\noverlay_pending_entry=',overlay_pending and 1 or 0,
        '\nbackground_schema=ACBG0001\nbackground_complete_pairs=',background_pairs,
        '\nbackground_orphan_ends=',background_orphans,'\nbackground_pending_entry=',background_pending and 1 or 0,
        '\npattern_schema=ACPT0001\npattern_complete_pairs=',pattern_pairs,
        '\npattern_pending_entry=',pattern_pending and 1 or 0,
        '\nfade_schema=ACPF0001\nfade_complete_pairs=',fade_pairs,'\nfade_orphan_ends=',fade_orphans,
        '\nfade_pending_entry=',fade_pending and 1 or 0,
        '\nfade_out_schema=ACFD0001\nfade_out_complete_pairs=',fade_out_pairs,
        '\nfade_out_orphan_ends=',fade_out_orphans,'\nfade_out_pending_entry=',fade_out_pending and 1 or 0,
        '\npulse_schema=ACPZ0001\npulse_complete_pairs=',pulse_pairs,'\npulse_orphan_ends=',pulse_orphans,
        '\npulse_pending_entry=',pulse_pending and 1 or 0,'\n')
      manifest:close()
    end
    local done=assert(io.open(directory..'/complete.txt','w'))
    done:write('Captured ',frames,' frames with Mesen 2.1.1\n'); done:close()
    emu.stop(0)
  end
end
emu.addEventCallback(input,emu.eventType.inputPolled)
emu.addEventCallback(end_frame,emu.eventType.endFrame)
