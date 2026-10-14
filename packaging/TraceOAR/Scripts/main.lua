 
local source = debug.getinfo(1, "S").source:gsub("^@", "")
local root = source:match("^(.*)[/\\][Ss][Cc][Rr][Ii][Pp][Tt][Ss][/\\][^/\\]+$")
assert(root, "TraceOAR: cannot resolve mod directory")
local bootstrapRoot = root
local challenge = tostring({}) .. ":" .. tostring(os.time()) .. ":" .. tostring(math.random(1, 2147483647))
local request = assert(io.open(bootstrapRoot .. "/attach.request", "w"))
request:write(challenge); request:close()
local tick
local function attach()
    local response = io.open(bootstrapRoot .. "/attach.response", "r")
    if not response then return false end
    local token, path = response:read("*l"), response:read("*l")
    response:close()
    if token ~= challenge or not path then return false end
    local callback, err = package.loadlib(path, "TraceTick")
    if not callback then print("[TraceOAR] Attach failed: " .. tostring(err) .. "\n"); return false end
    local directory = path:match("^(.*)[/\\][^/\\]+$")
    if not directory then return false end
    root = directory
    tick = callback
    print("[TraceOAR] Runtime attached after injection\n")
    return true
end
local cached = {}
local elapsed, refreshElapsed = 0, 1
local lastSnapshot = ""
local hooked = false
local paused = false
local allowed = { Lock_pick_C=true, BP_HackingPoint_C=true, BP_Powerbox_C=true,
    Tool_Grenade_EMP_C=true, Tool_Grenade_base_C=true, RobberTruck_C=true, GuardPhone_C=true, Item_Keycard_C=true, BreakableGlass_C=true, DisplayBreakableGlass_C=true }
local function refresh()
    local control = io.open(root .. "/pause.flag", "r")
    paused = control ~= nil
    if control then control:close() end
    if paused then return end
    local requested = io.open(root .. "/needed.txt", "r")
    if not requested then cached = {}; return end
    local nextCache = {}
    for class in requested:lines() do
        if allowed[class] then
            local ok, objects = pcall(FindAllOf, class)
            if ok and objects then nextCache[class] = objects end
        end
    end
    requested:close()
    cached = nextCache
end
local function playerTick(context, delta)
    local actor = context:get()
    if not actor or not actor:IsValid() or not actor:IsLocallyControlled() then return end
    local dt = delta:get()
    if type(dt) ~= "number" or dt < 0 then dt = 0 end
    elapsed = elapsed + dt
    refreshElapsed = refreshElapsed + dt
    if elapsed < 0.05 then return end
    elapsed = 0
    if refreshElapsed >= 1 then refreshElapsed = 0; refresh() end
    if paused then return end
    local lines = { string.format("PlayerCharacter_C %x\n", actor:GetAddress()) }
    for class, objects in pairs(cached) do
        for _, object in ipairs(objects) do
            if object:IsValid() then lines[#lines+1] = string.format("%s %x\n", class, object:GetAddress()) end
        end
    end
    local snapshot = table.concat(lines)
    if snapshot ~= lastSnapshot then
        local file = assert(io.open(root .. "/objects.txt", "w"))
        file:write(snapshot); file:close()
        lastSnapshot = snapshot
    end
    tick()
end
 
LoopAsync(1000, function()
    if hooked then return true end
    if not tick and not attach() then return false end
    local fn = StaticFindObject("/Game/BP/Player/PlayerCharacter.PlayerCharacter_C:ReceiveTick")
    if fn and fn:IsValid() then
        local ok, pre, post = pcall(RegisterHook,
            "/Game/BP/Player/PlayerCharacter.PlayerCharacter_C:ReceiveTick", playerTick)
        if ok then hooked = true; print("[TraceOAR] Player Tick hooked; idle world scans disabled\n"); return true end
        print("[TraceOAR] Tick hook failed: " .. tostring(pre) .. "\n")
    end
    return false
end)
