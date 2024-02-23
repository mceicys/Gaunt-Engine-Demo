-- pillar.lua
LFV_EXPAND_VECTORS()

local qua = gqua
local rnd = grnd
local scn = gscn
local scr = gscr
local vec = gvec
local wrp = gwrp

local com = scr.EnsureScript("common.lua")

local pil = {}
pil.__index = pil
pil.nMinFallBase = 0.1
pil.nMaxFallBase = 0.75
pil.nFallFac = 1.0

--[[------------------------------------
	pil:Init
--------------------------------------]]
function pil:Init()
	
	setmetatable(self:Table(), pil)
	self:SetOrientHull(true)
	self.nHealth = 100
	self.nFall = 0.0
	
end

--[[------------------------------------
	pil:Level
--------------------------------------]]
function pil:Level()
	
	local t = self:Transcript()
	
	if t then
		
		self.nHealth = t["health"] or self.nHealth
		self.nFallRate = t["fallRate"]
		
		if t["lightTexture"] then
			
			local entMimic = scn.CreateEntity(nil, self:Place())
			self.entMimic = entMimic
			self:SetChild(entMimic)
			entMimic:SetAmbientGlass(true)
			entMimic:SetShadow(false)
			local mshLight
			
			if t["lightMesh"] then
				mshLight = rnd.EnsureMesh(t["lightMesh"])
			else
				mshLight = self:Mesh()
			end
			
			entMimic:SetMesh(mshLight)
			entMimic:SetTexture(rnd.EnsureTexture(t["lightTexture"]))
			entMimic:MimicScaledPlace(self)
			
		end
		
	end
	
	self.q4Init = self:Ori()
	
end

--[[------------------------------------
	pil:Tick
--------------------------------------]]
function pil:Tick()
	
	if self.nHealth <= 0.0 then
		
		self.nFall = math.min(self.nFall + com.Clamp(self.nFall, self.nMinFallBase, self.nMaxFallBase) * self.nFallFac * wrp.TimeStep(), 1.0)
		self:SetOri(qua.Slerp(self.q4Init, self.q4Tipped, self.nFall))
		
	end
	
	local entMimic = self.entMimic
	
	if entMimic then
		
		entMimic:MimicScaledPlace(self)
		entMimic:SetOpacity(1.0 - self.nFall)
		
	end
	
end

--[[------------------------------------
	pil:Hurt
--------------------------------------]]
function pil:Hurt(entAttacker, nDmg, v3Dir, entHit)
	
	local nOldHealth = self.nHealth
	self.nHealth = nOldHealth - nDmg
	
	if self.nHealth <= 0.0 then
		
		if nOldHealth > 0.0 then
			
			-- Start falling over
			local v3Up = qua.Up(self.q4Init)
			
			if not xDir then
				v3Dir = 0, 0, 0
			end
			
			local v3NormDir = vec.Normalized3(v3Dir)
			local v3Axis, bErr = vec.Normalized3(vec.Cross(v3NormDir, v3Up))
			
			if bErr then
				v3Axis = qua.Dir(self.q4Init)
			end
			
			self.q4Tipped = qua.Product(self.q4Init, qua.QuaAxisAngle(v3Axis, math.pi * 0.5))
			
		end
		
		return true
		
	end
	
	return false
	
end

pil.typ = scn.CreateEntityType("pillar", pil.Init, pil.Tick, nil, nil, pil.Level)

return pil