-- blastdoor.lua
LFV_EXPAND_VECTORS()

local hit = ghit
local rnd = grnd
local scn = gscn
local scr = gscr

local lok = scr.EnsureScript("lock.lua")

local bla = {}
bla.__index = bla

bla.msh = rnd.EnsureMesh("blastdoor.msh")
bla.tex = rnd.EnsureTexture("blastdoor.tex")
local v3Size = 0.1, 1.0, 1.0

--[[------------------------------------
	bla:Init
--------------------------------------]]
function bla:Init()
	
	setmetatable(self:Table(), bla)
	self:SetMesh(self.msh)
	self:SetTexture(self.tex)
	self:SetOrientHull(true)
	
end

--[[------------------------------------
	bla:Level
--------------------------------------]]
function bla:Level()
	
	local t = self:Transcript()
	local nScale = self:Scale()
	self:SetHull(hit.HullBox(-v3Size * nScale, v3Size * nScale))
	
	if t["lock"] then
		lok.RegisterDoor(self, t["lock"])
	end
	
end

--[[------------------------------------
	bla:Tick
--------------------------------------]]
function bla:Tick()
	
end

--[[------------------------------------
	bla:Open
--------------------------------------]]
function bla:Open()
	
	self:Delete()
	
end

bla.typ = scn.CreateEntityType("blastdoor", bla.Init, bla.Tick, nil, nil, bla.Level)

return bla