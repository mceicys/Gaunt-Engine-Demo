-- turret.lua
LFV_EXPAND_VECTORS()

local hit = ghit
local qua = gqua
local rnd = grnd
local scr = gscr
local scn = gscn
local vec = gvec
local wrp = gwrp

local ai = scr.EnsureScript("ai.lua")
local bol = scr.EnsureScript("bolt.lua")
local com = scr.EnsureScript("common.lua")
local exp = scr.EnsureScript("explosion.lua")

local tur = {}
tur.__index = tur
setmetatable(tur, ai)

tur.hulGun = hit.EnsureHull("turgun.hul")
tur.mshGun = rnd.EnsureMesh("turgun.msh")
tur.aniGunFire = tur.mshGun:Animation("fire")

tur.hulBase = hit.EnsureHull("turbase.hul")
tur.hulBroken = hit.EnsureHull("turbroken.hul")
tur.mshBase = rnd.EnsureMesh("turbase.msh")

tur.tex = rnd.EnsureTexture("turret.tex")
tur.texBroken = rnd.EnsureTexture("turret_broken.tex")
tur.texMin = rnd.EnsureTexture("turret_min.tex")

local AIM_RATE = 2.0

--[[------------------------------------
	tur:Init
--------------------------------------]]
function tur:Init()
	
	setmetatable(self:Table(), tur)
	
	self:SetOrientHull(true)
	self:SetHull(self.hulGun)
	self:SetMesh(self.mshGun)
	self:SetTexture(self.tex)
	self:SetAnimationSpeed(2.0)
	self:SetAnimation(self.aniGunFire, 2, 0)
	
	local v3Pos = self:Pos()
	local q4Ori = self:Ori()
	local entBase = scn.CreateEntity(nil, v3Pos, q4Ori)
	self.entBase = entBase
	entBase:SetOrientHull(true)
	entBase:SetHull(self.hulBase)
	entBase:SetMesh(self.mshBase)
	entBase:SetTexture(self.tex)
	
	local entMimic = scn.CreateEntity(nil, v3Pos, q4Ori)
	self.entMimic = entMimic
	self:SetChild(entMimic)
	entMimic:SetMinimumGlass(true)
	entMimic:SetShadow(false)
	entMimic:SetTexture(self.texMin)
	entMimic:MimicMesh(self)
	
	local entBaseMimic = scn.CreateEntity(nil, v3Pos, q4Ori)
	self.entBaseMimic = entBaseMimic
	entBase:SetChild(entBaseMimic)
	entBaseMimic:SetMinimumGlass(true)
	entBaseMimic:SetShadow(false)
	entBaseMimic:SetTexture(self.texMin)
	entBaseMimic:MimicMesh(entBase)
	
	self.nAimPit, self.nAimYaw = 0, 0
	self.nVisPit, self.nVisYaw = 0, 0
	self.nHealth = 15
	self.iFireTime = wrp.SimTime()
	self.iSeeTarTime = wrp.SimTime()
	self.iInFlags = 0
	self:InitThinkTimeVars()
	self:SetThink("Idle")
	
end

--[[------------------------------------
	tur:Term
--------------------------------------]]
function tur:Term()
	
	self.entMimic:Delete()
	self.entBaseMimic:Delete()
	
end

--[[------------------------------------
	tur:Level
--------------------------------------]]
function tur:Level()
	
	local t = self:Transcript()
	
	if not t then
		return
	end
	
end

--[[------------------------------------
	tur:Tick

FIXME: prevent rotating thru other entities
--------------------------------------]]
function tur:Tick()
	
	local iTime = wrp.SimTime()
	local nStep = wrp.TimeStep()
	self:Think()
	
	-- Lerp visual orientation towards aim orientation
	local nAimTime = (self.iInFlags & self.IN_SNAP_AIM ~= 0 and 0.0) or
		(self.iNextThink - iTime) * 0.001
	
	local nVisPit, nVisYaw = self.nVisPit, self.nVisYaw
	local nAimPit, nAimYaw = self.nAimPit, self.nAimYaw
	nVisPit = com.TimedApproach(nVisPit, nAimPit, nil, nil, nAimTime, nStep)
	
	nVisYaw = com.TimedApproach(nVisYaw, nAimYaw, nil, nil, nAimTime, nStep,
		com.WrapAngle(nAimYaw - nVisYaw))
	
	self.nVisPit, self.nVisYaw = nVisPit, nVisYaw
	
	-- Update absolute orientation
	local q4Base = self.entBase:Ori()
	local q4Add = qua.QuaPitchYaw(nVisPit, nVisYaw)
	self:SetOri(qua.Product(q4Base, q4Add))
	
	-- Update orientation
	--[[
	local v3InAim = self.v3InAim
	
	if xInAim then
		
		local nAddPit, nAddYaw = self.nAddPit, self.nAddYaw
		local q4Base = self.entBase:Ori()
		local nGoalPit, nGoalYaw
		
		if xInAim ~= 0.0 or yInAim ~= 0.0 or zInAim ~= 0.0 then
			
			local v3RelAim = qua.VecRotInv(v3InAim, q4Base)
			nGoalPit, nGoalYaw = vec.PitchYaw(v3RelAim, nAddYaw)
			
		else
			nGoalPit, nGoalYaw = 0, 0 -- Return to base orientation
		end
		
		local AIM_RATE = 2.0
		
		local nAimTime = (self.iInFlags & self.IN_SNAP_AIM ~= 0 and 0.0) or
			(self.iNextThink - iTime) * 0.001
		
		nAddPit = com.TimedApproach(nAddPit, nGoalPit, AIM_RATE, AIM_RATE, nAimTime, nStep)
		
		nAddYaw = com.TimedApproach(nAddYaw, nGoalYaw, AIM_RATE, AIM_RATE, nAimTime, nStep,
			com.WrapAngle(nGoalYaw - nAddYaw))
		
		self.nAddPit, self.nAddYaw = nAddPit, nAddYaw
		local q4Add = qua.QuaPitchYaw(nAddPit, nAddYaw)
		self:SetOri(qua.Product(q4Base, q4Add))
		
	end
	]]
	
	self.entMimic:MimicMesh(self)
	self.entBaseMimic:MimicMesh(self.entBase)
	
end

--[[------------------------------------
	tur:Hurt
--------------------------------------]]
function tur:Hurt(entAttacker, nDmg, v3Dir, entHit)
	
	self.nHealth = self.nHealth - nDmg
	
	if self.nHealth <= 0.0 then
		
		local v3Pos = self:Pos()
		local v3Up = qua.Up(self.entBase:Ori())
		exp.CreateExplosion(self, nil, v3Pos + v3Up * 6.0, 100, 0, 0)
		self.entBase:SetTexture(self.texBroken)
		self.entBase:SetHull(self.hulBroken)
		self:Delete()
		return true
		
	end
	
	if scn.EntityExists(entAttacker) and self.entTarget ~= entAttacker then
		
		self:SetTarget(entAttacker)
		self:SetThink("Attack")
		
	end
	
	return false
	
end

--[[------------------------------------
	tur:AimAtDir

OUT	nAimPit, nAimYaw, bCanFire
--------------------------------------]]
function tur:AimAtDir(v3Dir, nAimRate, nStep)
	
	local nAimPit, nAimYaw = self.nAimPit, self.nAimYaw
	
	if xDir == 0.0 and yDir == 0.0 and zDir == 0.0 then
		return nAimPit, nAimYaw, false
	end
	
	local q4Base = self.entBase:Ori()
	local v3RelAim = qua.VecRotInv(v3Dir, q4Base)
	local nGoalPit, nGoalYaw = vec.PitchYaw(v3RelAim, nAimYaw)
	local nAimDel = nAimRate * nStep
	nAimPit = math.min(com.Approach(nAimPit, nGoalPit, nAimDel), 0.56)
	nAimYaw = com.Approach(nAimYaw, nGoalYaw, nAimDel, com.WrapAngle(nGoalYaw - nAimYaw))
	return nAimPit, nAimYaw, nAimPit == nGoalPit and nAimYaw == nGoalYaw
	
end

--[============================================================================================[


	THINKS


]============================================================================================]--

local FIRE_DIST = 1000

--[[------------------------------------
	tur:Idle
--------------------------------------]]
function tur:Idle()
	
	-- Reset to original orientation
	self.iNextThink = wrp.SimTime() + 100
	local nAimStep = AIM_RATE * self:ThinkStep()
	self.nAimPit = com.Approach(self.nAimPit, 0, nAimStep)
	self.nAimYaw = com.Approach(self.nAimYaw, 0, nAimStep, com.WrapAngle(-self.nAimYaw))
	
	-- Look for a target
	-- FIXME: check all nearby ents for enemies, switch if closer than current target and haven't switched target recently
	local bLOS
	
	if scn.EntityExists(entPlayer) and not entPlayer.bCheatNoTarget then
		
		local v3Player = entPlayer:Pos()
		
		bLOS = self:RadiusLOS(v3Player, entPlayer, FIRE_DIST, self.fsetSeeIgnore, hit.ALL,
			self.entBase)
		
		if bLOS then
			
			self:SetTarget(entPlayer)
			self:SetThink("Attack")
			return
			
		end
		
	end
	
end

--[[------------------------------------
	tur:BeginAttack
--------------------------------------]]
function tur:BeginAttack()
	
	self.sThink = "Attack"
	self.v3LastTarPos = self:Pos()
	
end

--[[------------------------------------
	tur:Attack
--------------------------------------]]
function tur:Attack()
	
	local iTime = wrp.SimTime()
	self.iNextThink = iTime + 100
	
	local entTarget = self.entTarget
	if not self:ValidTarget(entTarget) then return self:SetThink("Idle") end
	
	local nThinkStep = self:ThinkStep()
	local v3Pos = self:Pos()
	local v3Tar = entTarget:Pos()
	
	if self:RadiusLOS(v3Tar, entTarget, FIRE_DIST, self.fsetSeeIgnore, hit.ALL,
	self.entBase) then
		
		self.iSeeTarTime = iTime
		self.v3LastTarPos = v3Tar
		
	end
	
	local iSeeTarAge = iTime - self.iSeeTarTime
	
	if iSeeTarAge > 5000 then
		return self:SetThink("Idle")
	end
	
	if iSeeTarAge <= 1000 then
		
		local v3Dif = v3Tar - v3Pos
		local nDif = vec.Mag3(v3Dif)
		local nHitTime = com.Clamp(nDif / 300.0, 0.35, 1.5)
		
		-- Predict target's average velocity based on its orientation
		-- FIXME: Can't tell if this is any more effective than constant velocity prediction; curve prediction with frequency-filtered inputs might do better
		local v3TarVel = entTarget.v3Vel or 0.0
		local nTarVel = vec.Mag3(v3TarVel)
		local q4Tar = entTarget:Ori()
		local v3TarDir = qua.Dir(q4Tar)
		local v3Axis, nAngle = vec.DirDelta(v3TarVel, v2TarDir, zTarDir - 27.0 / nTarVel)
			-- zTarDir subtraction is compensating for the player's fight between gravity and lift
		local v3Left = qua.Left(q4Tar)
		local nUpFac = math.max(0.0, -vec.Dot3(v3Axis, v3Left)) -- Get a [0,1] pitch-up factor to emulate lift helping change velocity
		local q4Rot = qua.QuaAxisAngle(v3Axis, nAngle * (0.4 + 0.2 * nUpFac))
		local v3PredTarVel = qua.VecRot(v3TarVel, q4Rot)
		local v3Hit = v3Tar + v3PredTarVel * nHitTime
		
		-- Aim and fire
		local bCanFire
		local v3FireDif = v3Hit - v3Pos
		self.nAimPit, self.nAimYaw, bCanFire = self:AimAtDir(v3FireDif, 2.0, nThinkStep)
		
		if bCanFire and iTime - self.iFireTime >= 1000 then
			
			local q4Base = self.entBase:Ori()
			local q4Ori = qua.Product(q4Base, qua.QuaPitchYaw(self.nAimPit, self.nAimYaw))
			local v3Dir = qua.Dir(q4Ori)
			local v3Prod = v3Dir * 12.0
			
			bol.CreateBolt(v3Pos + v3Prod, q4Ori, self, (v3FireDif - v3Prod) / nHitTime, 0, 0, 0, 10,
				entTarget, 0, 0, true)
			
			self.iFireTime = iTime
			self:SetAnimation(self.aniGunFire, 0, 0)
			
		end
		
	else
		
		self.nAimPit, self.nAimYaw = self:AimAtDir(self.v3LastTarPos - v3Pos, 2.0,
			nThinkStep)
		
	end
	
end

tur.typ = scn.CreateEntityType("turret", tur.Init, tur.Tick, tur.Term, nil, tur.Level)

return tur