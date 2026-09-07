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
    local done=assert(io.open(directory..'/complete.txt','w'))
    done:write('Captured ',frames,' frames with Mesen 2.1.1\n'); done:close()
    emu.stop(0)
  end
end
emu.addEventCallback(input,emu.eventType.inputPolled)
emu.addEventCallback(end_frame,emu.eventType.endFrame)
