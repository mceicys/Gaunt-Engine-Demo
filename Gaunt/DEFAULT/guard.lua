-- guard.lua -- Flies a path forever while shooting targets
LFV_EXPAND_VECTORS()

local flg = gflg
local hit = ghit
local qua = gqua
local rnd = grnd
local scn = gscn
local scr = gscr
local vec = gvec
local wrp = gwrp

local ai = scr.EnsureScript("ai.lua")
local bol = scr.EnsureScript("bolt.lua")
local bul = scr.EnsureScript("bullet.lua")
local com = scr.EnsureScript("common.lua")
local exp = scr.EnsureScript("explosion.lua")
local phy = scr.EnsureScript("physics.lua")
local pt = scr.EnsureScript("point.lua")

local gua = {}
gua.__index = gua
setmetatable(gua, ai)

gua.hul = hit.EnsureHull("bandit.hul")
gua.msh = rnd.EnsureMesh("bandit.msh")
gua.tex = rnd.EnsureTexture("bandit.tex")
gua.texMin = rnd.EnsureTexture("bandit_min.tex")
gua.mshStream = rnd.EnsureMesh("banditstream.msh")
gua.texStream = rnd.EnsureTexture("banditstream.tex")
gua.aniStream = gua.mshStream:Animation("thrust")
gua.nTurnRate = 2.5
gua.nDefaultSpeed = 250
gua.nDefaultSpeedRate = 200
gua.nMass = 1.0
gua.ImpactVelocity = phy.ImpactVelocityBounce
gua.nBounceLimit = 50
gua.nBounceFactor = 0.5
gua.nImpactFric = 0.3

local PROBE_SIZE = 12
gua.nProbeSize = PROBE_SIZE
gua.hulProbe = hit.HullBox(0, -PROBE_SIZE, -PROBE_SIZE, PROBE_SIZE, PROBE_SIZE, PROBE_SIZE)
gua.hulAhead = hit.HullBox(-PROBE_SIZE, -PROBE_SIZE, -PROBE_SIZE, PROBE_SIZE, PROBE_SIZE, PROBE_SIZE)

-- [odd] iStep, [even] iTimeLimit
gua.tAimIntervals = {
	100, 1000,
	300, 4000
}

gua.iMirrorFrame = 0

local GUN_VELOCITY = 300

--[[------------------------------------
	gua:Init
--------------------------------------]]
function gua:Init()
	
	setmetatable(self:Table(), gua)
	self:SetOrientHull(true)
	self:SetHull(self.hul)
	self:SetMesh(self.msh)
	self:SetTexture(self.tex)
	self:SetSubPalette(3)
	self:FlagSet():Or(I_ENT_MOBILE, I_ENT_ENEMY)
	phy.Init(self)
	self.nHealth = 50
	
	local entMimic = scn.CreateEntity(nil, self:Place())
	self.entMimic = entMimic
	self:SetChild(entMimic)
	entMimic:SetMinimumGlass(true)
	entMimic:SetShadow(false)
	entMimic:SetTexture(self.texMin)
	entMimic:MimicMesh(self)
	
	local entStream = scn.CreateEntity(nil, self:Place())
	self.entStream = entStream -- FIXME: make this a child but we need a bigger link hull then
	entStream:SetRegularGlass(true)
	entStream:SetSubPalette(3)
	entStream:SetShadow(false)
	entStream:SetMesh(self.mshStream)
	entStream:SetTexture(self.texStream)
	entStream:SetAnimationSpeed(0)
	entStream:SetAnimation(self.aniStream, 0, 0)
	entStream:SetOpacity(1)
	
	-- AI
	self:InitThinkTimeVars()
	self.tPath, self.tSmoothPath = {}, {} -- FIXME: init function
	self.iNumSmoothSegments = 0
	self.nLimitedSmoothDist = 0.0
	self.bRailMode = true
	self:SetThink("Patrol")
	
	guard = (guard or 0) + 1
	self.guard = guard
	--[[
	if self.guard ~= 2 then
		self:Delete()
	end
	--]]
	
end

--[[------------------------------------
	gua:Term
--------------------------------------]]
function gua:Term()
	
	self.entMimic:Delete()
	self.entStream:Delete()
	
end

--[[------------------------------------
	gua:Level
--------------------------------------]]
function gua:Level()
	
	local t = self:Transcript()
	
	if not t then
		return
	end
	
	self:SetTarget(scn.EntityFromRecordID(t["target"]))
	self:SetEntityPath(scn.EntityFromRecordID(t["path"]))
	
end

--[[------------------------------------
	gua:Tick
--------------------------------------]]
function gua:Tick()
	
	if self.nHealth <= 0.0 then
		
		self:Delete()
		return
		
	end
	
	-- FIXME TEMP
	--[[
	if not self.skipped then
		
		if self.guard ~= 1 and self.guard ~= 5 then
			
			local SKIP_TIME = 1.37
			local _
			self:SmoothPath(self:MaxPathLookDistance(SKIP_TIME, vec.Mag3(self.v3Vel), 50, 300, 150, 150), 15.0, true)
			self.v3Vel, _, self.bRailMode = self:FollowPath(self.v3Vel, SKIP_TIME, 50, 300, 150, 150)
			
		end
		
		self.skipped = true
		
	end
	]]
	
	self:Think()
	
	local v3Vel = self.v3Vel
	local v3OldVel = v3Vel
	local nStep = wrp.TimeStep()
	local nMoveStepRem = nStep
	
	if self.bRailMode then
		v3Vel, nMoveStepRem, self.bRailMode = self:FollowPath(v3Vel, nMoveStepRem, 50, 300, 150, 150)
	end
	
	if not self.bRailMode then
		
		zVel = zVel + phy.zGravity * nMoveStepRem
		v3Vel = phy.Push(self, v3Vel, nMoveStepRem)
		nMoveStepRem = 0.0
		
	end
	
	self.v3Vel = v3Vel
	
	--self:DrawPath(true, 8, 16, 31, 31, 0.0)
	
	-- Update orientation
	
	-- FIXME TEMP
	local v3Pos = self:Pos()
	self:SmoothPath(200.0, 15.0, true)
	local _, _, v3Ahead = self:SmoothPathDist(200.0, true)
	local v2Right = vec.Normalized2(yVel, -xVel)
	local nRolTarget = com.ClampMag(vec.Dot2(v2Right, v2Ahead - v2Pos) * 0.01, math.pi * 0.5)
	
	local q4Ori = self:Ori()
	q4Ori = self:OrientToAim(q4Ori, v3Vel)
	q4Ori = self:OrientUpright(q4Ori, nRolTarget)
	self:SetOri(q4Ori)
	
	self.entMimic:MimicMesh(self)
	
	local entStream = self.entStream
	entStream:MimicScaledPlace(self)
	local nVel = vec.Mag3(v3Vel)
	local nSpeedFrac = com.Clamp(nVel / 300.0, 0, 1)
	nSpeedFrac = nSpeedFrac * nSpeedFrac
	entStream:SetAnimation(self.aniStream, nSpeedFrac, 0)
	
	--[[
	ggui.Draw3DText(v2Pos, zPos + 50, string.format("%d", self.guard))
	]]
	
end

--[[------------------------------------
	gua:Hurt
--------------------------------------]]
function gua:Hurt(entAttacker, nDmg, v3Dir, entHit)
	
	local nOldHealth = self.nHealth
	self.nHealth = nOldHealth - nDmg
	
	if self.nHealth <= 0.0 then
		
		if nOldHealth > 0.0 then
			
			local v3Pos = self:Pos()
			exp.CreateExplosion(self, nil, v3Pos, 100, 0, 0)
			
		end
		
		return true
		
	end
	
	if scn.EntityExists(entAttacker) then
		self:SetTarget(entAttacker)
	end
	
	return false
	
end

--[[------------------------------------
	gua:SetTarget
--------------------------------------]]
function gua:SetTarget(entTarget)
	
	if self.entTarget == entTarget then
		return
	end
	
	self.entTarget = entTarget
	self:ClearTargetEstimate()
	
end

--[[------------------------------------
	gua:Move

OUT	v3Pos, v3Vel
--------------------------------------]]
function gua:Move(tRes, v3Pos, v3PrevPos, v3Vel, v3PrevVel, nTime)
	
	if tRes[1] == hit.HIT then
		
		v3Vel = self:Impact(tRes, v3Pos, v3PrevVel, v3Vel, tRes[4], tRes[5], tRes[6], nTime,
			tRes[7])
		
	end
	
	return v3Pos, v3Vel
	
end

--[[------------------------------------
	gua:Impact

OUT	v3Vel, nAway

FIXME: one general AI impact func?
--------------------------------------]]
function gua:Impact(tMoveRes, v3Pos, v3ImpactVel, v3DefaultVel, v3Normal, nTime, entOther,
	v3OtherImpactVel)
	
	local v3Vel, nAway = phy.Impact(self, tMoveRes, v3Pos, v3ImpactVel, v3DefaultVel, v3Normal,
		nTime, entOther, v3OtherImpactVel)
	
	if nAway < 0.0 then
		self:Hurt(nil, -nAway * 0.5, 0, 0, 0, nil)
	end
	
	self.bRailMode = false
	
	return v3Vel, nAway
	
end

--[============================================================================================[


	THINKS


]============================================================================================]--

--[[------------------------------------
	gua:BeginPatrol
--------------------------------------]]
function gua:BeginPatrol()
	
	self.sThink = "Patrol"
	local iTime = wrp.SimTime()
	self.iBoltFireTime = iTime
	
end

--[[------------------------------------
	gua:EndPatrol
--------------------------------------]]
function gua:EndPatrol()
	
end

--[[------------------------------------
	gua:Patrol
--------------------------------------]]
function gua:Patrol()
	
	local iTime = wrp.SimTime()
	local v3Pos = self:Pos()
	local v3Vel = self.v3Vel
	
	self.iNextThink = iTime + 100
	
	-- Update target
	-- FIXME: check all nearby ents for enemies, switch if closer than current target and haven't switched target recently
	local bLOS
	local FIRE_DIST = 1000
	local FIRE_LOS_DELAY = 1000
	
	if scn.EntityExists(entPlayer) and not entPlayer.bCheatNoTarget then
		
		local v3Player = entPlayer:Pos()
		bLOS = self:RadiusLOS(v3Player, entPlayer, FIRE_DIST)
		
		if bLOS then
			self:SetTarget(entPlayer)
		end
		
	end
	
	local entTarget = self.entTarget
	local v3Tar, v3TarVel, v3TarDif, nTarDif, nTarDot
	
	if self:ValidTarget(entTarget) then
		
		self:UpdateTargetPast()
		v3Tar = entTarget:Pos()
		v3TarVel = entTarget.v3Vel or 0.0
		
		if bLOS == nil then
			bLOS = self:RadiusLOS(v3Tar, entTarget, FIRE_DIST)
		end
		
		v3TarDif = v3Tar - v3Pos
		nTarDif = vec.Mag3(v3TarDif)
		local v3VelDir = vec.Normalized3(v3Vel)
		local v3TarDir
		if nTarDif > 0.0 then v3TarDir = v3TarDif / nTarDif else v3TarDir = v3TarDif end
		nTarDot = vec.Dot3(v3VelDir, v3TarDir)
		
	else
		
		self:SetTarget(nil)
		entTarget = nil
		self.iBoltFireTime = math.max(self.iBoltFireTime, iTime - FIRE_LOS_DELAY)
		bLOS = false
		
	end
	
	-- Follow entity path
	local nWantSpeed = 300
	local nLookAhead = nWantSpeed * 0.1
	--self:MorePathToDistance(nLookAhead, 1)
	self:SmoothPath(self:MaxPathLookDistance(self:NextThinkStep(), vec.Mag3(v3Vel), 50, 300, 150, 150), 15.0, true)
	--self:DrawPath(false, 4, 31, 31)
	--self:DrawPath(true, 2, 7, 7)
	local bFollowingTarget
	
	if bLOS then
		
		if nTarDot >= 0.5 and nTarDif <= 500.0 then
			
			nWantSpeed = math.max(150, vec.Mag3(entTarget.v3Vel))
			bFollowingTarget = true
			
		else
			nWantSpeed = 250
		end
		
	end
	
	-- Shoot at target
	if bLOS then
		
		if bFollowingTarget then
			
			-- Shoot bolts offensively
			self.v3InAim = v3Tar - v3Pos
			
			if iTime - self.iBoltFireTime > 2000 then
				
				self.iBoltFireTime = iTime
				local q4Ori = self:Ori()
				local v3Dif = v3Tar - v3Pos
				local nDif = vec.Mag3(v3Dif)
				local nHitTime = com.Clamp(nDif / 300.0, 0.35, 1.5)
				local v3Hit = v3Tar + v3TarVel * nHitTime
				
				local v3FireVel = (v3Hit - v3Pos) / nHitTime
				
				-- FIXME: just use the firing code below instead of copy pasting; bFollowing branch changes v3Hit and nHitTime, that's all
				local iC = hit.LineTest(v3Pos, v3Hit, hit.ALL, bol.fsetIgnore, self, entTarget)
				
				if iC == hit.HIT then
					
					local v3Axis, nAngle = vec.DirDelta(v3FireVel, v3Dif)
					local q4Rot = qua.QuaAxisAngle(v3Axis, nAngle * 0.5)
					v3FireVel = qua.VecRot(v3FireVel, q4Rot)
					
					iC = hit.LineTest(v3Pos, v3Pos + v3FireVel * nHitTime, hit.ALL, bol.fsetIgnore, self, entTarget)
					
					if iC == hit.HIT then
						v3FireVel = qua.VecRot(v3FireVel, q4Rot)
					end
					
				end
				
				bol.CreateBolt(v3Pos, q4Ori, self, v3FireVel, 0, 0, 0, 10, entTarget, 0, 0, true)
				
			end
			
		elseif nTarDot <= 0.0 then
			
			-- Shoot bolts defensively
			if iTime - self.iBoltFireTime > 2000 then
				
				self.iBoltFireTime = iTime
				local q4Ori = self:Ori()
				local nTarVel = vec.Mag3(v3TarVel)
				local q4Tar = entTarget:Ori()
				local v3TarDir = qua.Dir(q4Tar)
				local v3Dif = v3Tar - v3Pos
				local nDif = vec.Mag3(v3Dif)
				local nHitTime = com.Clamp(nDif / 300.0, 0.35, 1.5)
				
				-- Approximately predict target as if it's player aiming bullets at agent
					-- FIXME: This isn't a perfect prediction, so I think a similar or better result could be achieved with simpler expressions and magic numbers and without any iteration
				local v3Hit = v3Tar
				local v3PredTarVel = v3TarVel
				local v3Pred = v3Pos
				local tPath = self.tPath
				local v3TempGoal = self:CurrentPathPoint()
				local iTempGoalIndex = 1
				local v3PredVel = v3Vel
				local nVel = vec.Mag3(v3Vel)
				
				local nT = 0.0
				
				while nT < nHitTime do
					
					local nPredStep = nHitTime - nT
					
					if nPredStep >= 0.1 then
						nPredStep, nT = 0.1, nT + 0.1
					else
						nT = nHitTime
					end
					
					-- Predict agent
					local nSelfStep = nPredStep
					--local v3OldPred = v3Pred
					
					if nVel > 0.0 then
						
						while xTempGoal do
							
							local v3Heading = v3TempGoal - v3Pred
							local nDist = vec.Mag3(v3Heading)
							if nDist > 0.0 then v3Heading = v3Heading / nDist end
							v3PredVel = v3Heading * nVel
							local nDelta = nVel * nSelfStep
							
							if nDist > nDelta then
								
								local v3Delta = v3Heading * nDelta
								v3Pred = v3Pred + v3Delta
								break
								
							else
								
								v3Pred = v3TempGoal
								nSelfStep = nDist / nVel
								iTempGoalIndex = iTempGoalIndex + self.iPathStride
								v3TempGoal = tPath[iTempGoalIndex], tPath[iTempGoalIndex + 1], tPath[iTempGoalIndex + 2]
								
							end
							
						end
						
					end
					
					--rnd.DrawLine(v3OldPred, v3Pred, 31, self.THINK_STEP)
					
					-- Predict target
					v3PredTarVel = phy.Friction(v3PredTarVel, 0.01, nil, nil, nPredStep)
					local nTarAimTime = com.InterceptTime(v3Hit, 1500, v3Pred, v3PredVel - v3PredTarVel) or 0.0
					local v3TarAim = v3Pred + v3PredVel * nTarAimTime
					v3PredTarVel = phy.Fly(v3PredTarVel, v3TarAim - v3Hit, 320, nTarVel, true, nTarVel, nPredStep)
					--local v3OldHit = v3Hit
					v3Hit = v3Hit + v3PredTarVel * nPredStep
					--rnd.DrawLine(v3OldHit, v3Hit, 31, self.THINK_STEP)
					
				end
				
				local v3FireVel = (v3Hit - v3Pos) / nHitTime
				
				local iC = hit.LineTest(v3Pos, v3Hit, hit.ALL, bol.fsetIgnore, self, entTarget)
				
				if iC == hit.HIT then
					
					local v3Axis, nAngle = vec.DirDelta(v3FireVel, v3Dif)
					local q4Rot = qua.QuaAxisAngle(v3Axis, nAngle * 0.5)
					v3FireVel = qua.VecRot(v3FireVel, q4Rot)
					
					iC = hit.LineTest(v3Pos, v3Pos + v3FireVel * nHitTime, hit.ALL, bol.fsetIgnore, self, entTarget)
					
					if iC == hit.HIT then
						v3FireVel = qua.VecRot(v3FireVel, q4Rot)
					end
					
				end
				
				bol.CreateBolt(v3Pos, q4Ori, self, v3FireVel, 0, 0, 0, 10, entTarget, 0, 0, true)
				
			end
			
		end
		
	else -- not bLOS
		
		-- Don't fire for at least a second after regaining LOS
		self.iBoltFireTime = math.max(self.iBoltFireTime, iTime - FIRE_LOS_DELAY)
		
	end
	
end

gua.typ = scn.CreateEntityType("guard", gua.Init, gua.Tick, gua.Term, nil, gua.Level)

return gua