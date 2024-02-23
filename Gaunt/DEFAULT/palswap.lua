-- palswap.lua

local rnd = grnd
local wrp = gwrp

local psp = {}

psp.palBase = rnd.EnsurePalette("base.pal") -- FIXME: level must be able to set this
psp.palGet = rnd.EnsurePalette("get.pal")

-- Time a temporary palette should be reset
-- This is process time, NOT simulation time (so add to wrp.Time(), not wrp.SimTime())
psp.iGetTime = nil

--[[------------------------------------
	roc.Frame
--------------------------------------]]
function psp.Frame()
	
	local iTime = wrp.Time()
	local palSet = psp.palBase
	
	if psp.iGetTime then
		
		if iTime - psp.iGetTime >= 0 then
			psp.iGetTime = nil
		else
			palSet = psp.palGet
		end
		
	end
	
	rnd.SetPalette(palSet)
	
end

return psp