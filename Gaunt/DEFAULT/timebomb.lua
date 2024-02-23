-- timebomb.lua
LFV_EXPAND_VECTORS()

local qua = gqua
local scn = gscn
local scr = gscr
local wrp = gwrp

local exp = scr.EnsureScript("explosion.lua")

local tmb = {}
tmb.__index = tmb

--[[------------------------------------
	tmb:Init
--------------------------------------]]
function tmb:Init()
	
	setmetatable(self:Table(), tmb)
	self.iExplodeTime = wrp.SimTime()
	
end

--[[------------------------------------
	tmb:Level
--------------------------------------]]
function tmb:Level()
	
	local t = self:Transcript()
	
	if not t then
		return
	end
	
	self.iExplodeTime = wrp.SimTime() + math.floor(t["time"] * 1000)
	self.nRadius = t["radius"]
	self.nDmg = t["damage"]
	self.nFrc = t["force"]
	self.bFace = t["face"]
	
end

--[[------------------------------------
	tmb:Tick
--------------------------------------]]
function tmb:Tick()
	
	local iTime = wrp.SimTime()
	
	if iTime >= self.iExplodeTime then
		self:Delete()
	end
	
end

--[[------------------------------------
	tmb:Term
--------------------------------------]]
function tmb:Term()
	
	local v3Pos = self:Pos()
	local v3Normal
	
	if self.bFace then
		v3Normal = qua.Dir(self:Ori())
	end
	
	exp.CreateExplosion(nil, nil, v3Pos, self.nRadius, self.nDmg, self.nFrc, v3Normal)
	
end

tmb.typ = scn.CreateEntityType("timebomb", tmb.Init, tmb.Tick, tmb.Term, nil, tmb.Level)

return tmb