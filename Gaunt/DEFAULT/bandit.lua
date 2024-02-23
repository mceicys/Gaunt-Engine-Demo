-- bandit.lua -- Chasing enemy
-- FIXME: rename "enforcer"
-- FIXME: limit to a radius or radii so he doesn't chase the player everywhere (and avoids massive path searches)
-- FIXME: shoot homing missle so player is running away from an object that has to touch him?
LFV_EXPAND_VECTORS()

local flg = gflg
local hit = ghit
local pat = gpat
local qua = gqua
local rnd = grnd
local scn = gscn
local scr = gscr
local vec = gvec
local wrp = gwrp

local ai = scr.EnsureScript("ai.lua")
local bol = scr.EnsureScript("bolt.lua")
local com = scr.EnsureScript("common.lua")
local exp = scr.EnsureScript("explosion.lua")
local phy = scr.EnsureScript("physics.lua")
local pt = scr.EnsureScript("point.lua")

local com_InterceptTime = com.InterceptTime
local getmetatable = getmetatable
local EntPos = scn.EntPos
local EntType = scn.EntType
local hit_ALL = hit.ALL
local hit_HIT = hit.HIT
local hit_HullTest = hit.HullTest
local hit_INTERSECT = hit.INTERSECT
local hit_LineTest = hit.LineTest
local hit_NONE = hit.NONE
local math_abs = math.abs
local math_huge = math.huge
local math_max = math.max
local math_min = math.min
local math_random = math.random
local math_sqrt = math.sqrt
local vec_Dot3 = vec.Dot3
local vec_Mag2 = vec.Mag2
local vec_Mag3 = vec.Mag3
local vec_MagSq3 = vec.MagSq3
local vec_Normalized3 = vec.Normalized3

local ban = {}
ban.__index = ban
setmetatable(ban, ai)

ban.hul = hit.EnsureHull("bandit.hul")
ban.msh = rnd.EnsureMesh("bandit.msh")
ban.tex = rnd.EnsureTexture("bandit.tex")
ban.nDefaultHeadingRate = math.pi * 0.5
ban.nDefaultSpeed = 200
ban.nDefaultSpeedRate = 200

local PROBE_SIZE = 12
ban.nProbeSize = PROBE_SIZE
ban.hulProbe = hit.HullBox(0, -PROBE_SIZE, -PROBE_SIZE, PROBE_SIZE, PROBE_SIZE, PROBE_SIZE)
ban.hulAhead = hit.HullBox(-PROBE_SIZE, -PROBE_SIZE, -PROBE_SIZE, PROBE_SIZE, PROBE_SIZE, PROBE_SIZE)

-- [odd] iStep, [even] iTimeLimit
ban.tAimIntervals = {
	100, 1000,
	300, 4000
}

local GUN_VELOCITY = 300

--[[------------------------------------
	ban:Init
--------------------------------------]]
function ban:Init()
	
	local q4Ori = self:Ori()
	
	setmetatable(self:Table(), ban)
	self:SetOrientHull(true)
	self:SetHull(self.hul)
	self:SetMesh(self.msh)
	self:SetTexture(self.tex)
	self:FlagSet():Or(I_ENT_MOBILE, I_ENT_ENEMY)
	phy.Init(self)
	self.nHealth = 50
	
	ban.iNumBandits = ban.iNumBandits + 1 -- FIXME TEMP
	entBandit = self -- FIXME TEMP
	
	-- AI
	self:InitGeneralInputs()
	self.iFireTime = nil
	self.iNextThink = self:InitThinkTime()
	self.tPath = {}
	self:SetThink("Patrol")
	
	self:Delete()
	
end

--[[------------------------------------
	ban:Level
--------------------------------------]]
function ban:Level()
	
	local t = self:Transcript()
	
	if not t then
		return
	end
	
	self:SetTarget(scn.EntityFromRecordID(t["target"]))
	self:SetEntityPath(scn.EntityFromRecordID(t["path"]))
	
end

--[[------------------------------------
	ban:Tick
--------------------------------------]]
function ban:Tick()
	
	self:ClearTargetFuture()
	self:Think()
	
	-- Update orientation
	local q4Ori = self:Ori()
	q4Ori = self:OrientToAim(q4Ori)
	q4Ori = self:OrientUpright(q4Ori)
	-- FIXME: add some roll when strafing or turning
	-- FIXME: physical reorientation, prevent intersections
	self:SetOri(q4Ori)
	
	-- Update velocity
	local v3Vel = self.v3Vel
	v3Vel = self:UpdateSpeedAndHeading(v3Vel)
	
	-- Move
	v3Vel = phy.Push(self, v3Vel, wrp.TimeStep())
	self.v3Vel = v3Vel
	
end

--[[------------------------------------
	ban:Hurt
--------------------------------------]]
function ban:Hurt(entAttacker, nDmg, v3Dir, entHit)
	
	self.nHealth = self.nHealth - nDmg
	
	if self.nHealth <= 0.0 then
		
		ban.iNumBandits = ban.iNumBandits - 1 -- FIXME TEMP
		gcon.LogF("bandits %d", ban.iNumBandits)
		local v3Pos = EntPos(self)
		exp.CreateExplosion(self, nil, v3Pos, 100, 0, 0)
		self:Delete()
		return true
		
	end
	
	if scn.EntityExists(entAttacker) then
		
		self:SetTarget(entAttacker)
		local iTime = wrp.SimTime()
		
		if self.sThink ~= "Flee" then
			
			self.iFleeExtend = 1000
			self.iFleeStopTime = iTime + 1000
			self:SetThink("Flee")
			
		end
		
		local iFleeSpan = self.iFleeStopTime and self.iFleeStopTime - iTime or 0
		
		if iFleeSpan < self.iFleeExtend then
			self.iFleeStopTime = iTime + self.iFleeExtend
		end
		
	end
	
	return false
	
end

--[[------------------------------------
	ban:SetTarget
--------------------------------------]]
function ban:SetTarget(entTarget)
	
	if self.entTarget == entTarget then
		return
	end
	
	self.entTarget = entTarget
	self:ClearTargetEstimate()
	
end

--[[------------------------------------
	ban:Move

OUT	v3Pos, v3Vel

FIXME TEMP
--------------------------------------]]
function ban:Move(tRes, v3Pos, v3PrevPos, v3Vel, v3PrevVel, nTime)
	
	if tRes[1] ~= hit_NONE then
		gcon.LogF("contact %d", tRes[1])
	end
	
	return v3Pos, v3Vel
	
end

--[============================================================================================[


	THINKS


]============================================================================================]--

--[[------------------------------------
	ban:BeginPatrol
--------------------------------------]]
function ban:BeginPatrol()
	
	self.sThink = "Patrol"
	self.nInHeadingRate = self.nDefaultHeadingRate
	
end

--[[------------------------------------
	ban:EndPatrol
--------------------------------------]]
function ban:EndPatrol()
	
	self.v3InHeading = nil
	
end

--[[------------------------------------
	ban:Patrol

FIXME: If bandit only has LOS of target and not sight, should take a few seconds to trigger Chase
	Player can shoot bandit while it's in Patrol to trigger immediate Flee
--------------------------------------]]
function ban:Patrol()
	
	if (self.iPathFlags & self.PATH_HAVE) ~= 0 then
		
		local iDirectAge = self:FollowPath(50, 200, nil)
		
		if iDirectAge >= 1000 then
			self:ResetPath()
		end
		
	else
		
		-- Circle counter-clockwise at constant speed and altitude
		local v3Vel = self.v3Vel
		local v3InHeading = v2Vel, 0.0
		
		if xInHeading == 0.0 and yInHeading == 0.0 then
			xInHeading = 1.0
		end
		
		xInHeading, yInHeading = -yInHeading, xInHeading
		self.v3InHeading = v3InHeading
		
	end
	
	self.v3InAim = self.v3Vel
	
	-- FIXME: check all nearby ents for enemies and enemy projectiles
	if scn.EntityExists(entPlayer) and not entPlayer.bCheatNoTarget then
		
		local v3Player = entPlayer:Pos()
		
		if self:RadiusLOS(v3Player, entPlayer, 1000) then
			
			self:SetTarget(entPlayer)
			self:SetThink("DirectChase")
			
			--[[
			local entTarget = self.entTarget
			local v3Tar = EntPos(entTarget)
			local v3TarVel = entTarget.v3Vel
			local nTarVel = vec_Mag3(v3TarVel)
			local v3TarVelDir = v3TarVel
			if nTarVel > 0.0 then v3TarVelDir = v3TarVel / nTarVel end
			local v3Pos = EntPos(self)
			local nVel = vec_Mag3(v3Vel)
			local v3VelDir = v3Vel
			if nVel > 0.0 then v3VelDir = v3Vel / nVel end
			local v3DangerousDir = vec_Normalized3(v3Pos - v3Tar)
			local nToward = vec_Dot3(v3DangerousDir, v3TarVelDir) * 0.5 + 0.5
			local nBehind = vec_Dot3(v3DangerousDir, v3VelDir) * 0.5 + 0.5
			local nFast = math_min(nTarVel / nVel, 1.0)
			local nScared = nToward * nBehind * nFast
			local iFleeAdd = math.floor(nScared * nScared * 8000)
			local iTime = wrp.SimTime()
			self.iFleeStopTime = iTime + iFleeAdd
			self.iFleeExtend = 20000
			self:SetThink("Flee")
			]]
			
		end
		
	end
	
end

--[[------------------------------------
	ban:BeginDirectChase
--------------------------------------]]
function ban:BeginDirectChase()
	
	self.sThink = "DirectChase"
	local iTime = wrp.SimTime()
	self.iChaseTime = iTime
	self.iChasePathTime = iTime
	self.nInHeadingRate = self.nDefaultHeadingRate
	self:ClearPath()
	
end

--[[------------------------------------
	ban:DirectChase
--------------------------------------]]
function ban:DirectChase()
	
	local entTarget = self.entTarget
	
	if not self:ValidTarget(entTarget) then
		
		-- FIXME: go to last patrol point
		self:SetTarget(nil)
		self:SetThink("Patrol")
		return
		
	end
	
	local iTime = wrp.SimTime()
	
	--
	--	ADJUST SPEED
	--
	
	self:UpdateTargetPast() -- FIXME: only if in LOS
		-- FIXME: implement dossiers so this only has to be done once a tick per target?
			-- FIXME: just have the target manage its own prediction, when it's asked to
	
	local v3Pos = EntPos(self)
	local v3Tar = EntPos(entTarget)
	local v3TarVel = entTarget.v3Vel
	local nTarVel = vec_Mag3(v3TarVel)
	local v3Dif = v3Tar - v3Pos
	local nDif = vec_Mag3(v3Dif)
	local TargetAtFutureTime = self.TargetAtFutureTime
	local LOS = self.LOS
	local bTarLOS = LOS(self, v3Tar, entTarget, self.fsetSeeIgnore)
	
	if bTarLOS == false then
		self.iChaseLostTime = iTime
	else
		
		local v3LastCheck = v3Tar
		local fsetTargetIgnore = entTarget.fsetIgnore
		
		for iAhead = 100, 1000, 100 do
			
			local v3Future = TargetAtFutureTime(self, iTime + iAhead, 100)
			
			local iC = hit_LineTest(v3LastCheck, v3Future, hit_ALL, fsetTargetIgnore, self,
				entTarget)
			
			if iC ~= hit_NONE then
				break
			end
			
			if LOS(self, v3Future, entTarget, self.fsetSeeIgnore) == false then
				
				self.iChaseLostTime = iTime
				break
				
			end
			
			v3LastCheck = v3Future
			
		end
		
	end
	
	if (bTarLOS and nDif < 150) or (self.iChaseLostTime and iTime - self.iChaseLostTime <= 2000) then
		
		self.nInSpeed = nTarVel * 0.75
		self.nInSpeedRate = 200
		
	else
		
		self.nInSpeed = nTarVel + 40
		self.nInSpeedRate = 50
		
	end
	
	local MIN_SPEED = 100
	
	if self.nInSpeed < MIN_SPEED then
		self.nInSpeed = MIN_SPEED
	end
	
	--
	--	CHASE PATH
	--
	
	if bTarLOS and nDif < 75 then
		
		-- Back off
		local v3TarDir
		
		if nTarVel then
			v3TarDir = v3TarVel / nTarVel
		else
			v3TarDir = qua.Dir(entTarget:Ori())
		end
		
		self:SetGoalPath(v3Tar - v3TarDir * 200.0)
		
	elseif (self.iPathFlags & self.PATH_HAVE) == 0 then
		self:SetChasePath() -- Chase
	end
	
	local iDirectAge = self:FollowPath(MIN_SPEED, self.nInSpeed, nil, entTarget)
	local nPathDist = self:PathDist(10000.0)
	
	if iDirectAge > 0 or (nPathDist < 1000 or iTime - self.iChasePathTime > nPathDist) then
		
		-- Path outdated
		self:ClearPath()
		self.iChasePathTime = iTime
		
	end
	
	self.v3InAim = self.v3Vel
	
	--
	--	FIRE
	--
	
	if nDif < 400 and (not self.iFireTime or iTime - self.iFireTime > 1000) then
		
		local v3Aim = self:SmartAim(v3Pos, v3TarVel, GUN_VELOCITY, self.tAimIntervals, 0.0)
		local q4Ori = self:Ori()
		local v3FireVel = vec_Normalized3(v3Aim)
		v3FireVel = v3FireVel * GUN_VELOCITY
		bol.CreateBolt(v3Pos, q4Ori, self, v3FireVel, v3TarVel, 10, entTarget, 0, 0.3)
		self.iFireTime = iTime
		
	end
	
	--
	--	COOLDOWN
	--
	
	if iTime - self.iChaseTime > 30000 then
		
		self.iFleeStopTime = iTime + 30000
		self.iFleeExtend = 1000
		self:SetThink("Flee")
		gcon.LogF("aaahh help me")
		
	end
	
end

--[[------------------------------------
	ban:BeginFlee

FIXME: Choose forward or reverse based on the path to the first flee point
	And/or give a wanted approach direction to the path search
		But that could force agent to turn around in a tight space
--------------------------------------]]
function ban:BeginFlee()
	
	self.sThink = "Flee"
	local v3Pos = self:Pos()
	local v3Dir = vec_Normalized3(self.v3Vel)
	local entFlee = pt.ClosestFleePoint(v3Pos + v3Dir * 300.0)
	self:SetEntityPath(entFlee)
	self.nInHeadingRate = self.nDefaultHeadingRate
	self.nInSpeedRate = self.nDefaultSpeedRate
	
end

--[[------------------------------------
	ban:Flee
--------------------------------------]]
function ban:Flee()
	
	local iDirectAge = self:FollowPath(100, 250, nil)
	
	if iDirectAge >= 1000 then
		self:ResetPath()
	end
	
	self.v3InAim = self.v3Vel
	local iTime = wrp.SimTime()
	
	if self.iFleeStopTime and iTime >= self.iFleeStopTime then
		
		self.iFleeStopTime = nil
		self:SetThink("DirectChase")
		
	end
	
end

ban.typ = scn.CreateEntityType("bandit", ban.Init, ban.Tick, nil, nil, ban.Level)

return ban