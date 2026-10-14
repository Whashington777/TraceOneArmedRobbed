local file, root = arg[1], arg[2]
local reads, snapshotWrites, nativeTicks, loadCalls = 0, 0, 0, 0
local bootstrapRoot = root
local runtimeRoot = root .. "/runtime"
local hook, attempt
local realOpen, realInfo, realLoadlib = io.open, debug.getinfo, package.loadlib
io.open = function(path, mode)
    if path == root .. '/objects.txt' and mode == 'w' then snapshotWrites = snapshotWrites + 1 end
    return realOpen(path, mode)
end
debug.getinfo = function() return { source='@' .. root .. '/scripts/main.lua' } end
package.loadlib = function(path, symbol)
    assert(path == runtimeRoot .. '/TraceRuntime.dll' and symbol == 'TraceTick')
    loadCalls = loadCalls + 1
    return function() nativeTicks = nativeTicks + 1 end
end
StaticFindObject = function() return { IsValid=function() return true end } end
RegisterHook = function(_, fn) hook=fn; return 1, 1 end
LoopAsync = function(delay, fn) assert(delay==1000); attempt=fn end
FindAllOf = function(name)
    reads = reads + 1
    assert(name=='BP_HackingPoint_C')
    return { { IsValid=function() return true end, GetAddress=function() return 0x4321 end } }
end
local localPlayer = true
local address = 0x1234
local actor={ IsValid=function()return true end, IsLocallyControlled=function()return localPlayer end, GetAddress=function()return address end }
local context={get=function()return actor end};local delta={get=function()return 1/60 end}
dofile(file)
assert(loadCalls == 0 and not hook, 'Bootstrap must not autoload a DLL')
assert(attempt() == false and loadCalls == 0)
local request=assert(realOpen(bootstrapRoot .. '/attach.request','r'));local challenge=request:read('*a');request:close()
local response=assert(realOpen(bootstrapRoot .. '/attach.response','w'));response:write('stale\n' .. runtimeRoot .. '/TraceRuntime.dll\n');response:close()
assert(attempt() == false and loadCalls == 0, 'Old session must not attach')
response=assert(realOpen(bootstrapRoot .. '/attach.response','w'));response:write(challenge .. '\n' .. runtimeRoot .. '/TraceRuntime.dll\n');response:close()
assert(attempt() == true and hook and loadCalls == 1)
root=runtimeRoot
for i=1,240 do hook(context,delta) end
assert(reads==0, 'Idle must not enumerate world objects')
assert(snapshotWrites==1, 'Unchanged snapshots must not rewrite files')
assert(nativeTicks>30 and nativeTicks<100, 'Native updates must be throttled')
local needed=assert(realOpen(root .. '/needed.txt','w'));needed:write('BP_HackingPoint_C\n');needed:close()
for i=1,120 do hook(context,delta) end
assert(reads>=1 and reads<=3, 'Only requested classes at <=1 Hz')
assert(snapshotWrites==2, 'Cached addresses should not be rewritten every tick')
local before=nativeTicks;localPlayer=false
for i=1,60 do hook(context,delta) end
assert(nativeTicks==before,'Remote players must not dispatch native updates')
localPlayer=true;address=0x5678
local writes=snapshotWrites;before=nativeTicks
for i=1,60 do hook(context,delta) end
assert(nativeTicks>before and snapshotWrites==writes+1,'New map/player must continue updates and replace snapshot')
local snapshot=assert(realOpen(root .. '/objects.txt','r'));local contents=snapshot:read('*a');snapshot:close()
assert(contents:find('PlayerCharacter_C 5678',1,true) and not contents:find('PlayerCharacter_C 1234',1,true),'Previous player must not survive map change')
io.open,debug.getinfo,package.loadlib=realOpen,realInfo,realLoadlib
print('Manual attach, stale session rejection, external DLL root; bootstrap: zero idle scans, <=1 Hz selective refresh, stable snapshots, local-player dispatch passed')
