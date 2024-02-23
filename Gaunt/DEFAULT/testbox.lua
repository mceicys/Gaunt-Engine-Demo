-- testbox.lua
LFV_EXPAND_VECTORS()

local flg = gflg
local hit = ghit
local qua = gqua
local rnd = grnd
local scn = gscn
local scr = gscr
local vec = gvec
local wrp = gwrp

local com = scr.EnsureScript("common.lua")
local phy = scr.EnsureScript("physics.lua")

local box = {}
box.__index = box
setmetatable(box, ai)

box.nSize = 16
box.hul = hit.HullBox(-box.nSize, -box.nSize, -box.nSize, box.nSize, box.nSize, box.nSize)
box.msh = rnd.EnsureMesh("box.msh")
box.tex = rnd.EnsureTexture("cgrid.tex")

box.nMass = 1.0
box.Impact = phy.Impact
box.ImpactVelocity = phy.ImpactVelocityBounce
box.nBounceFactor = 0.5
box.nImpactFric = 0.3

--[[------------------------------------
	box:Init
--------------------------------------]]
function box:Init()
	
	setmetatable(self:Table(), box)
	self:SetOrientHull(true)
	self:SetHull(self.hul)
	self:SetMesh(self.msh)
	self:SetTexture(self.tex)
	self:SetScale(self.nSize)
	self:FlagSet():Or(I_ENT_MOBILE)
	phy.Init(self)
	
end

--[[------------------------------------
	box:Tick
--------------------------------------]]
function box:Tick()
	
	self.v3Vel = phy.Push(self, self.v3Vel, wrp.TimeStep())
	
end

--[[------------------------------------
	box:Move

OUT	v3Pos, v3Vel
--------------------------------------]]
function box:Move(tRes, v3Pos, v3PrevPos, v3Vel, v3PrevVel, nTime)
	
	if tRes[1] == hit.HIT then
		
		v3Vel = self:Impact(tRes, v3Pos, v3PrevVel, v3Vel, tRes[4], tRes[5], tRes[6], nTime,
			tRes[7])
		
	end
	
	return v3Pos, v3Vel
	
end

box.typ = scn.CreateEntityType("box", box.Init, box.Tick, nil, nil, nil)

return box