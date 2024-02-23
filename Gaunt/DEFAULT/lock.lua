-- lock.lua
LFV_EXPAND_VECTORS()

local hit = ghit
local rnd = grnd
local scn = gscn
local scr = gscr

local exp = scr.EnsureScript("explosion.lua")

local lok = {}
lok.__index = lok

lok.msh = rnd.EnsureMesh("lockcell.msh")
lok.tex = rnd.EnsureTexture("lockcell.tex")
lok.texMin = rnd.EnsureTexture("lockcell_min.tex")
lok.tLockCounts = {}
lok.tLocked = {} -- Table of lists of locked doors (must have Open func); doors reg themselves
local v3Size = 1.0, 1.0, 2.0

--[[------------------------------------
	lok:Init
--------------------------------------]]
function lok:Init()
	
	setmetatable(self:Table(), lok)
	self:SetMesh(self.msh)
	self:SetTexture(self.tex)
	self:SetOrientHull(true)
	self.nHealth = 50
	
	local entMimic = scn.CreateEntity(nil, self:Place())
	self.entMimic = entMimic
	self:SetChild(entMimic)
	entMimic:SetMinimumGlass(true)
	entMimic:SetShadow(false)
	entMimic:SetTexture(self.texMin)
	entMimic:MimicMesh(self)
	
end

--[[------------------------------------
	lok:Term
--------------------------------------]]
function lok:Term()
	
	self.entMimic:Delete()
	
end

--[[------------------------------------
	lok:Level
--------------------------------------]]
function lok:Level()
	
	local t = self:Transcript()
	local nScale = self:Scale()
	self:SetHull(hit.HullBox(-v3Size * nScale, v3Size * nScale))
	local sLock = t["lock"]
	
	if sLock then
		
		local tLockCounts = self.tLockCounts
		self.sLock = sLock
		tLockCounts[sLock] = (tLockCounts[sLock] or 0) + 1
		
	end
	
end

--[[------------------------------------
	lok:Hurt

OUT	bKill
--------------------------------------]]
function lok:Hurt(entAttacker, nDmg, v3Dir, entHit)
	
	self.nHealth = self.nHealth - nDmg
	
	if self.nHealth <= 0.0 then
		
		local tLockCounts, sLock = self.tLockCounts, self.sLock
		
		if tLockCounts[sLock] then
			
			tLockCounts[sLock] = tLockCounts[sLock] - 1
			
			if tLockCounts[sLock] == 0 then
				
				local tDoors = self.tLocked[sLock]
				
				if tDoors then
					
					for i = 1, #tDoors do
						
						if scn.EntityExists(tDoors[i]) then
							tDoors[i]:Open()
						end
						
					end
					
				end
				
			end
			
		end
		
		local v3Pos = self:Pos()
		exp.CreateExplosion(self, nil, v3Pos, 100, 0, 0)
		
		self:Delete()
		return true
		
	end
	
	return false
	
end

--[[------------------------------------
	lok.RegisterDoor
--------------------------------------]]
function lok.RegisterDoor(entDoor, sLock)
	
	local tDoors = lok.tLocked[sLock]
	
	if not tDoors then
		
		tDoors = {}
		lok.tLocked[sLock] = tDoors
		
	end
	
	tDoors[#tDoors + 1] = entDoor
	
end

--[[------------------------------------
	lok.ClearLockInfo
--------------------------------------]]
function lok.ClearLockInfo()
	
	local tLockCounts, tLocked = lok.tLockCounts, lok.tLocked
	for k in pairs(tLockCounts) do tLockCounts[k] = nil end
	for k in pairs(tLocked) do tLocked[k] = nil end
	
end

lok.typ = scn.CreateEntityType("lockcell", lok.Init, nil, lok.Term, nil, lok.Level)

return lok