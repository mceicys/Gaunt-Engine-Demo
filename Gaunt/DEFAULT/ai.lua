-- ai.lua
LFV_EXPAND_VECTORS()

local flg = gflg
local gui = ggui
local hit = ghit
local pat = gpat
local qua = gqua
local rnd = grnd
local scn = gscn
local scr = gscr
local vec = gvec
local wrp = gwrp

local com = scr.EnsureScript("common.lua")
local phy = scr.EnsureScript("physics.lua")

local ai = {}
ai.__index = ai

ai.fsetIgnore = flg.FlagSet():Or(I_ENT_SOFT, I_ENT_TRIGGER)
ai.fsetProbeIgnore = flg.FlagSet():Or(ai.fsetIgnore):Or(I_ENT_MOBILE)
ai.fsetSeeIgnore = flg.FlagSet(I_ENT_TRIGGER)

-- Input flags
ai.IN_SNAP_AIM = 1	--[[ Use the ship's full turn rate for this think interval even if it'll
					reach its goal orientation before the interval is over; looks jaggy over
					several intervals ]]
ai.IN_DELAY_FIRE = 2	--[[ Delay firing until goal orientation is reached; stays set if never
						reached (so manually unset it in think when blind firing) ]]

-- Path state
-- FIXME: confusion with engine pat system enums (GO, FAIL, WAIT)
ai.PATH_NONE = nil -- Path state is clear
ai.PATH_HAVE = 1 -- tPath has or may still get a waypoint
ai.PATH_FAIL = 2 -- Ran out of waypoints without reaching the goal and couldn't add anymore
ai.PATH_DONE = 3 -- Ran out of waypoints because the path's final goal has been reached

-- Smooth path point flags
local SMOOTH_DIRECT = 1
ai.SMOOTH_DIRECT = SMOOTH_DIRECT -- Smooth point is tied to a direct point

local DEFAULT_PATH_STRIDE = 3
	-- v3Pt
local SMOOTH_PATH_STRIDE = 6
	-- v3Pt, iFlags, nTailSegmentDist, [nHeadSegmentSpeedLimit]
ai.DEFAULT_PATH_STRIDE = DEFAULT_PATH_STRIDE
ai.SMOOTH_PATH_STRIDE = SMOOTH_PATH_STRIDE

local THINK_INTERVAL = ai.THINK_INTERVAL
local THINK_STEP = ai.THINK_STEP

local com_Clamp = com.Clamp
local com_InterceptTime = com.InterceptTime
local com_SafeArcCos = com.SafeArcCos
local EntOri = scn.EntOri
local EntPos = scn.EntPos
local EntSetPos = scn.EntSetPos
local hit_ALL = hit.ALL
local hit_ENTITIES = hit.ENTITIES
local hit_HIT = hit.HIT
local hit_HullTest = hit.HullTest
local hit_INTERSECT = hit.INTERSECT
local hit_LineTest = hit.LineTest
local hit_NONE = hit.NONE
local math_abs = math.abs
local math_cos = math.cos
local math_floor = math.floor
local math_huge = math.huge
local math_max = math.max
local math_min = math.min
local math_pi = math.pi
local math_random = math.random
local math_sin = math.sin
local math_sqrt = math.sqrt
local pat_FAIL = pat.FAIL
local pat_GO = pat.GO
local pat_WAIT = pat.WAIT
local qua_Dir = qua.Dir
local qua_QuaAxisAngle = qua.QuaAxisAngle
local qua_VecRot = qua.VecRot
local scn_EntityExists = scn.EntityExists
local select = select
local vec_Cross = vec.Cross
local vec_DirAngle3 = vec.DirAngle3
local vec_DirDelta = vec.DirDelta
local vec_Dot3 = vec.Dot3
local vec_Mag3 = vec.Mag3
local vec_MagSq3 = vec.MagSq3
local vec_Normalized3 = vec.Normalized3
local wrp_SimTime = wrp.SimTime
local wrp_TimeStep = wrp.TimeStep

local iInitThinkAdd = 0

--[[------------------------------------
	ai.ForeLoad
--------------------------------------]]
function ai.ForeLoad()
	
	iInitThinkAdd = 0
	
end

--[[------------------------------------
	ai:InitGeneralInputs
--------------------------------------]]
function ai:InitGeneralInputs()
	
	self.v3InAim = nil, nil, nil -- Aim direction
	self.nInAimRate = nil
	
end

--[[------------------------------------
	ai:InitThinkTimeVars

AI should call this to initialize think time variables. Think functions should update
self.iNextThink to schedule the next Think call.

FIXME: Variable think times can cause resync, especially if multiple agents react to one event.
Check if that can cause a stutter. If so, idling AI should offer to delay or advance their think
time to keep as many agents desync'd as possible. Registration and checking of current think
times shouldn't be necessary, there should be a blind algorithm that does a decent job of
spreading think times out while staying close to each agent's requested think time.
--------------------------------------]]
function ai:InitThinkTimeVars()
	
	-- Spread thinking out across frames
	local INTERVAL = 1000
	iInitThinkAdd = (iInitThinkAdd + 16) % INTERVAL
	local iTime = wrp_SimTime()
	self.iNextThink = iTime + iInitThinkAdd
	self.iLastThink = iTime -- For calculating think-step
	
end

--[[------------------------------------
	ai:ThinkStep

OUT	nThinkStep

Time in seconds since last think.
--------------------------------------]]
function ai:ThinkStep()
	
	return (wrp_SimTime() - self.iLastThink) * 0.001
	
end

--[[------------------------------------
	ai:NextThinkStep

OUT	nNextThinkStep
--------------------------------------]]
function ai:NextThinkStep()
	
	return (self.iNextThink - wrp_SimTime()) * 0.001
	
end

--[[------------------------------------
	ai:SetThink

Call to change to a think state. This allows a think state to clean up after itself in its End
function. The very first think state can be set by calling this or the state's Begin function
directly.
--------------------------------------]]
function ai:SetThink(sBegin)
	
	local EndThink = self.sThink and self["End" .. self.sThink]
	
	if EndThink then
		EndThink(self)
	end
	
	local BeginThink = self["Begin" .. sBegin]
	
	if BeginThink then
		BeginThink(self)
	else
		self.sThink = sBegin
	end
	
end

--[[------------------------------------
	ai:Think

OUT	bThought
--------------------------------------]]
function ai:Think()
	
	local iTime = wrp_SimTime()
	
	if iTime >= self.iNextThink then
		
		-- FIXME: PreThink()
		
		self[self.sThink](self)
		
		-- FIXME: PostThink()
		--[[if self.iFired then
			self.iFired = 0
		end]]
		
		self.iLastThink = iTime
		return true
		
	end
	
	return false
	
end

--[[------------------------------------
	ai:OrientToAim

OUT	q4Ori
--------------------------------------]]
function ai:OrientToAim(q4Ori, v3InAim, nInAimRate)
	
	v3InAim = v3InAim or self.v3InAim
	
	if not xInAim then
		return q4Ori
	end
	
	nInAimRate = nInAimRate or self.nInAimRate or math_pi
	local iTime = wrp_SimTime()
	local nStep = wrp_TimeStep()
	local v3Axis, nAngle = qua.LookDeltaAxisAngle(q4Ori, v3InAim)
	
	local nAimTime = ((self.iInFlags or 0) & self.IN_SNAP_AIM ~= 0 and 0.0) or
		(self.iNextThink - iTime) * 0.001
	
	local nTempRate = nInAimRate
	local nAbsAngle = math_abs(nAngle)
	
	-- Avoid reaching target orientation for nAimTime seconds
	if nAimTime > 0.0 then
		nTempRate = math_min(nTempRate, nAbsAngle / nAimTime)
	end
	
	local nDelta = nTempRate * nStep
	
	if nAbsAngle > nDelta then
		
		if nAngle < 0.0 then
			nDelta = -nDelta
		end
		
	else
		
		nDelta = nAngle
		
		if self.iInFlags then
			self.iInFlags = self.iInFlags & ~self.IN_DELAY_FIRE
		end
		
	end
	
	local q4Delta = qua.QuaAxisAngle(v3Axis, nDelta)
	return qua.Product(q4Delta, q4Ori)
	
end

--[[------------------------------------
	ai:OrientUpright

OUT	q4Ori

Roll so ground is below, faster the less pitched
--------------------------------------]]
function ai:OrientUpright(q4Ori, nWantRol)
	
	local nRol, nPit = qua.Roll(q4Ori), qua.Pitch(q4Ori)
	local nRolFac = 1.0 - math_abs(nPit) * 0.63661977236758134307553505349004 -- 2/pi
	nWantRol = nWantRol or 0.0
	local q4RolFix = qua.QuaEuler(com.ClampMag(nWantRol - nRol, 1.5 * wrp_TimeStep() * nRolFac), 0.0, 0.0)
	return qua.Product(q4Ori, q4RolFix)
	
end

--[[------------------------------------
	ai:LOS

OUT	bLOS, entHit

FIXME: if doing ALL_ALT test, hit transfer should be done here
FIXME: make ALL_ALT the default?
--------------------------------------]]
function ai:LOS(v3Tar, entTarget, fsetIgnore, iTestType, ...)
	
	local v3Pos = EntPos(self)
	
	local iC, _, _, _, _, _, entHit = hit_LineTest(v3Pos, v3Tar, iTestType or hit_ALL,
		fsetIgnore, self, entTarget, ...)
	
	return iC == hit_NONE, entHit
	
end

--[[------------------------------------
	ai:RadiusLOS

OUT	bRadiusLOS, entHit, bInRadius
--------------------------------------]]
function ai:RadiusLOS(v3Tar, entTarget, nDist, fsetIgnore, iTestType, ...)
	
	local v3Pos = EntPos(self)
	local v3Line = v3Tar - v3Pos
	
	if vec_MagSq3(v3Line) > nDist * nDist then
		return false, nil, false -- Too far
	end
	
	local iC, _, _, _, _, _, entHit = hit_LineTest(v3Pos, v3Tar, iTestType or hit_ALL,
		fsetIgnore, self, entTarget, ...)
	
	return iC == hit_NONE, entHit, true
	
end

--[[------------------------------------
	ai:See

OUT	bSee, entHit, bInField
--------------------------------------]]
function ai:See(v3Tar, entTarget, nDist, nDot, fsetIgnore, iTestType)
	
	local v3Pos = EntPos(self)
	local v3Line = v3Tar - v3Pos
	local nMagSq = vec_MagSq3(v3Line)
	
	if nMagSq > nDist * nDist then
		return false, nil, false -- Too far
	end
	
	if nMagSq ~= 0.0 then
		
		local v3Look = qua_Dir(EntOri(self))
		local nMag = math_sqrt(nMagSq)
		
		if vec_Dot3(v3Look, v3Line / nMag) < nDot then
			return false, nil, false -- Outside view angle
		end
		
	end
	
	local iC, _, _, _, _, _, entHit = hit_LineTest(v3Pos, v3Tar, iTestType or hit_ALL,
		fsetIgnore, self, entTarget)
	
	return iC == hit_NONE, entHit, true
	
end

--[[------------------------------------
	ai:SetTarget

Default function. Overwrite to clear variables, etc.
--------------------------------------]]
function ai:SetTarget(entTarget)
	
	--[[
	if self.entTarget == entTarget then
		return
	end
	]]
	
	self.entTarget = entTarget
	
end

--[[------------------------------------
	ai:ValidTarget

OUT	bValid
--------------------------------------]]
function ai:ValidTarget(entTarget)
	
	return scn_EntityExists(entTarget) and
		(not entTarget.nHealth or entTarget.nHealth > 0.0) and not entTarget.bCheatNoTarget
	
end

--[[------------------------------------
	ai:ClearTargetFuture
--------------------------------------]]
function ai:ClearTargetFuture()
	
	self.iFtrTime, self.v3FtrPos, self.v3FtrVel, self.v3FtrAcc, self.iPrevFtrTime,
		self.v3PrevFtrPos = nil
	
end

--[[------------------------------------
	ai:ClearTargetEstimate
--------------------------------------]]
function ai:ClearTargetEstimate()
	
	-- Clear past
	self.iPstTime, self.v3PstPos, self.v3PstVel, self.v3PstAcc, self.v3PstLongVel = nil
	
	-- Clear future
	self:ClearTargetFuture()
	
end

--[[------------------------------------
	ai:UpdateTargetPast

Updates target's latest known position and derivatives.

FIXME: too easy to trick aim with serpentine yet averaging is also too slow and indecisive
	need some sort of data "build-up and confirm" method
		large changes are stored separately until they're not cancelled out for a few intervals
			then they're put into the main data without averaging
				but make sure this doesn't make chasing less effective by delaying response
					movement decisions might still use the most recent data immediately
						each nugget of data can have a "consistency" [0, 1]
							prediction function then has a "trust" [0, 1] parameter
								each nugget's consistency is added with trust to calc its weight [0, 1]
								old nuggets' consistencies can also be revised based on further data

FIXME: keep 3rd derivative?
--------------------------------------]]
function ai:UpdateTargetPast()
	
	local iPstTime = self.iPstTime
	local iTime = wrp_SimTime()
	
	if iPstTime == iTime then
		return -- Already updated this millisecond
	end
	
	local iTimeDif = iTime - (iPstTime or iTime)
	
	self:ClearTargetFuture()
	local entTarget = self.entTarget
	
	if not scn_EntityExists(entTarget) then
		return
	end
	
	local nInvStep = iTimeDif > 0 and (1000 / iTimeDif) or 0.0
	local v3TarPos = entTarget:Pos()
	local v3TarVel = entTarget.v3Vel
	
	if not xTarVel then
		
		-- entTarget doesn't store its velocity, derive from position delta
		if iPstTime then
			v3TarVel = (v3TarPos - self.v3PstPos) * nInvStep
		else
			v3TarVel = 0.0, 0.0, 0.0
		end
		
	end
	
	-- Derive acceleration from velocity delta
	local v3TarAcc
	local v3PstVel = self.v3PstVel
	
	if iPstTime and xPstVel then
		v3TarAcc = (v3TarVel - v3PstVel) * nInvStep
	else
		v3TarAcc = 0.0, 0.0, 0.0
	end
	
	-- Smooth acceleration change
	--[[
	local v3PstAcc = self.v3PstAcc
	
	if xPstAcc then
		v3TarAcc = v3PstAcc + (v3TarAcc - v3PstAcc) * 0.25 -- FIXME: variable; FIXME: multiply by nThinkStep?
	end
	]]
	
	-- Long-term velocity
	local v3TarLongVel = v3TarVel
	local v3PstLongVel = self.v3PstLongVel
	
	if xPstLongVel then
		v3TarLongVel = v3PstLongVel + (v3TarLongVel - v3PstLongVel) * 0.1 -- FIXME: variable; FIXME: multiply by nThinkStep?
	end
	
	-- Update
	self.iPstTime = iTime
	self.v3PstPos = v3TarPos
	self.v3PstVel = v3TarVel
	self.v3PstAcc = v3TarAcc
	self.v3PstLongVel = v3TarLongVel
	
end

--[[------------------------------------
	ai:IterateTargetFuture

OUT	iFtrTime
--------------------------------------]]
function ai:IterateTargetFuture(iTimeAdvance)
	
	if not self.iFtrTime then
		
		-- Starting new extrapolation
		self.iFtrTime = self.iPstTime
		self.v3FtrPos = self.v3PstPos
		self.v3FtrVel = self.v3PstVel
		self.v3FtrAcc = self.v3PstAcc
		
	end
	
	-- Save current extrapolation as previous for lerping
	self.iPrevFtrTime = self.iFtrTime
	self.v3PrevFtrPos = self.v3FtrPos
	
	-- Extrapolate
	self.iFtrTime = self.iFtrTime + iTimeAdvance
	local nStep = iTimeAdvance * 0.001
	local v3FtrVel = self.v3FtrVel
	local v3SaveVel = v3FtrVel
	
		-- Accelerate
	local nClampSpeed
	local v3FtrAcc = self.v3FtrAcc
	
	if zFtrAcc < 0.0 then
		
		-- Apply -z accel before calculating clamp speed since gravity can push thru top speeds
		-- FIXME: see if this actually helps AI aim at enemies flying downward
		zFtrVel = zFtrVel + zFtrAcc * nStep
		nClampSpeed = vec_Mag3(v3FtrVel)
		v2FtrVel = v2FtrVel + v2FtrAcc * nStep
		
	else
		
		nClampSpeed = vec_Mag3(v3FtrVel)
		v3FtrVel = v3FtrVel + v3FtrAcc * nStep
		
	end
	
		-- Clamp
		-- FIXME: don't do clamp unless entTarget stores an nTop? or default to sound barrier?
	local nWantSpeed = vec_Mag3(v3FtrVel)
	nClampSpeed = math_max(nClampSpeed, 200)
	
	if nWantSpeed > nClampSpeed then
		
		local nMul = nClampSpeed / nWantSpeed
		v3FtrVel = v3FtrVel * nMul
		
	end
	
	self.v3FtrVel = v3FtrVel
	
		-- Rotate accelerator so curve doesn't straighten out
		-- FIXME: don't rotate accelerator if dot(vel, acc) < 0?
	self.v3FtrAcc = qua_VecRot(self.v3FtrAcc, qua_QuaAxisAngle(vec_DirDelta(v3SaveVel, v3FtrVel)))
	
		-- Move
	self.v3FtrPos = self.v3FtrPos + v3FtrVel * nStep
	
	-- FIXME TEMP
	--grnd.DrawLine(self.v3PrevFtrPos, self.v3FtrPos, 31, THINK_STEP)
	
	return self.iFtrTime
	
end

--[[------------------------------------
	ai:TargetAtFutureTime

OUT	v3FuturePos

FIXME: keep sequence of iteration results so any time can be queried without redoing work
FIXME: make all iterating functions take a table of intervals so functions have the expected
	prediction precision so long as the same table is given
--------------------------------------]]
function ai:TargetAtFutureTime(iTime, iStep)
	
	if iTime <= self.iPstTime then
		return self.v3PstPos
	end
	
	local Iterate = self.IterateTargetFuture
	local iFtrTime = self.iFtrTime
	
	if self.iPrevFtrTime and iTime < self.iPrevFtrTime then
		
		self:ClearTargetFuture()
		iFtrTime = nil
		
	end
	
	if not iFtrTime then
		iFtrTime = Iterate(self, iStep)
	end
	
	while iTime > iFtrTime do
		iFtrTime = Iterate(self, iStep)
	end
	
	local iPrevFtrTime = self.iPrevFtrTime
	local nLerp = (iTime - iPrevFtrTime) / (iFtrTime - iPrevFtrTime)
	local v3PrevFtrPos = self.v3PrevFtrPos
	return v3PrevFtrPos + (self.v3FtrPos - v3PrevFtrPos) * nLerp
	
end

--[[------------------------------------
	ai:SmartAim

OUT	[v3AimDif, v3HitWish, nTravelTime]

tAimIntervals: [odd] iStep, [even] iTimeLimit
The target's position is predicted for each iStep in between iTimeLimit and the previous limit.

FIXME: too easy to throw aim off by circling mouse; average needs to do better at ignoring serpentine
--------------------------------------]]
function ai:SmartAim(v3Pos, v3Vel, nGunVel, tAimIntervals, nOverLead)
	
	local iTime = wrp_SimTime()
	local v3NegVel = -v3Vel
	local v3PrevAhead, nPrevLead
	local Future = self.TargetAtFutureTime
	
	for i = 1, #tAimIntervals, 2 do
		
		local iStep = tAimIntervals[i]
		local iLimit = tAimIntervals[i + 1]
		local iStart = i > 2 and (tAimIntervals[i - 1] + iStep) or 0 -- Last limit or 0
		
		for iTimeAhead = iStart, iLimit, iStep do
			
			-- Predict target's position iTimeAhead into the future
			local v3Ahead = Future(self, iTime + iTimeAhead, iStep)
			--com.DrawCross(v3Ahead, 8, 31, 31, 31, self.THINK_INTERVAL * 0.001) -- FIXME TEMP
			
			-- Calculate projectile's travel time to target's future position
				-- Done relative to self instead of world since projectile inherits velocity
			local nTravel = com_InterceptTime(v3Pos, nGunVel, v3Ahead, v3NegVel)
			
			if not nTravel then
				goto continue
			end
			
			nTravel = nTravel + nOverLead -- Add lead error
			
			-- Calculate how many seconds ahead-of-target the projectile will be
			local nLead = iTimeAhead * 0.001 - nTravel
			
			-- If sign changed from previous lead time, zero-lead position may be in between
			-- Approximate by lerping based on lead times
			if nPrevLead and (nLead >= 0) ~= (nPrevLead >= 0) then
				
				local nF = -nPrevLead / (nLead - nPrevLead)
				local v3HitWish = v3PrevAhead + (v3Ahead - v3PrevAhead) * nF
				local nIntercept = com_InterceptTime(v3Pos, nGunVel, v3HitWish, v3NegVel)
				local v3Aim = (v3HitWish + v3NegVel * nIntercept) - v3Pos
				--gcon.LogF("lead %s", (iTimeAhead - iStep * (1.0 - nF)) * 0.001 - nIntercept) -- FIXME TEMP
				--com.DrawCross(v3Lerp, 16, 5, 5, 5, nIntercept) -- FIXME TEMP
				return v3Aim, v3HitWish, nTravel
				
			end
			
			-- Save for comparison with next iteration
			v3PrevAhead = v3Ahead
			nPrevLead = nLead
			
			::continue::
			
		end
		
	end
	
	-- Can't hit target's short-term trajectory, try a linear long shot
	
	-- FIXME: do this relative to closest matching hit position, add remaining lead time w/ linear velocity at that time
	
	local v3Tar = self.entTarget:Pos()
	local v3PstLongVel = self.v3PstLongVel
	local v3RelLong = v3PstLongVel - v3Vel
	local nIntercept = com_InterceptTime(v3Pos, nGunVel, v3Tar, v3RelLong)
	
	if not nIntercept then
		return nil, nil, nil -- Can't hit target
	end
	
	nIntercept = nIntercept + nOverLead
	local v3Aim = (v3Tar + v3RelLong * nIntercept) - v3Pos
	local v3HitWish = v3Tar + v3PstLongVel * nIntercept
	return v3Aim, v3HitWish, nIntercept
	
end

--[[------------------------------------
	ai:SafeToFire

OUT	bSafe

v3AimDif, v3HitWish, nTravelTime are returned from SmartAim. To fire safely, the bullet must not
hit within nDanger radius of self's future position (assuming constant velocity), and the bullet
must only hit within nDangerClose radius if it hits the target. nDangerClose > nDanger.
nDangerClose is used to avoid shooting obvious friendlies and obstacles.

FIXME: Smarter friendly fire check
	Do descent to collect nearby entities
	Cancel if any friendly entities get too close to line of fire from now to nTravelTime
	Other entities only cancel if they come too close to line of fire and may be hit within
		nDanger radius
--------------------------------------]]
local hulBullet = hit.HullBox(-1, -1, -1, 1, 1, 1)

function ai:SafeToFire(v3AimDif, v3HitWish, nTravelTime, v3Pos, v3Vel, nGunVel, nBulletSize,
	nDanger, nDangerClose)
	
	local nAimDif = vec_Mag3(v3AimDif)
	
	if nAimDif < nDanger then
		return false -- Target will be hit too close to self's future position
	end
	
	local entTarget = self.entTarget
	hulBullet:SetBox(-nBulletSize, -nBulletSize, -nBulletSize, nBulletSize, nBulletSize,
		nBulletSize)
	
	local iC, nTF, _, _, _, _, entHit = hit_HullTest(hulBullet, v3Pos, v3HitWish, 0, 0, 0, 1,
		hit_ALL, nil, self, entTarget)
	
	return iC == hit_NONE or nGunVel * nTF * nTravelTime >= nDangerClose
	
	--[[
	local nSafeTime = nSafeRadius / nGunVel
	
	local iC = hit_HullTest(hulBullet, v3Pos, v3Pos + v3BulletVel * nSafeTime, 0, 0, 0, 1,
		hit_ALL, nil, self)
	
	return iC == hit_NONE
	--]]
	
end

--[[------------------------------------
	ai:StandardAim

OUT	v3Aim
--------------------------------------]]
function ai:StandardAim()
	
	return self.v3Vel
	
	--[[
	local v3InNorm = vec_Normalized3(self.v3InDir)
	return self.v3Vel + v3InNorm * 100.0
	]]
	
end

--[[------------------------------------
	ai:ChaseCone

OUT	iChaseFlags

0 = none
1 = chasing
2 = getting chased
3 = both
--------------------------------------]]
function ai:ChaseCone(nChaseReq, nChasedReq)
	
	local entTarget = self.entTarget
	local v3Tar = EntPos(entTarget)
	local v3Pos = EntPos(self)
	local v3Dif = v3Tar - v3Pos
	local nDif = vec_Mag3(v3Dif)
	local v3DifNorm
	if nDif > 0.0 then v3DifNorm = v3Dif / nDif else v3DifNorm = v3Dif end
	
	local iFlags = 0
	
	if vec_Dot3(v3DifNorm, vec_Normalized3(self.v3Vel)) >= nChaseReq then
		iFlags = iFlags | 1
	end
	
	if vec_Dot3(-v3DifNorm, vec_Normalized3(entTarget.v3Vel)) >= nChasedReq then
		iFlags = iFlags | 2
	end
	
	return iFlags
	
end

--[============================================================================================[


	NAVIGATION

self.tPath is a sequence of waypoints consisting of a 3D vector and self.iPathExtraStride number
of extra non-nil elements.

self.iPathState is either ai.PATH_NONE, PATH_HAVE, PATH_FAIL, or PATH_DONE.

self.MorePath is called to request another point in the sequence. It should return pat.GO if a
point is added, pat.FAIL if no more path will be added, and pat.WAIT if MorePath might add a
point at another time.

When a point is passed, it is consumed.
]============================================================================================]--

--[[------------------------------------
	ai:ClearPath
--------------------------------------]]
function ai:ClearPath()
	
	local tPath = self.tPath
	for i = 1, #tPath do tPath[i] = nil end
	self.MorePath, self.ResetPath = nil, nil
	self:SetPathExtraStride(0)
	self.iPathState = ai.PATH_NONE
	
end

--[[------------------------------------
	ai:CurrentPathPoint

OUT	v3CurPathPt
--------------------------------------]]
function ai:CurrentPathPoint()
	
	local tPath = self.tPath
	return tPath[1], tPath[2], tPath[3]
	
end

--[[------------------------------------
	ai:FailPath
--------------------------------------]]
function ai:FailPath()
	
	self.iPathState = self.PATH_FAIL
	
end

--[[------------------------------------
	ai:SetPathPoint
--------------------------------------]]
function ai:SetPathPoint(iPt, v3Pt, ...)
	
	local tPath = self.tPath
	local iLen = #tPath
	local iFore = (iPt - 1) * self.iPathStride
	
	if iFore > iLen then
		error(string.format("Tried to set waypoint %s when there's only %s", iPt, iLen / self.iPathStride))
	end
	
	tPath[iFore + 1], tPath[iFore + 2], tPath[iFore + 3] = v3Pt
	
	for i = 1, self.iPathExtraStride do
		tPath[iFore + DEFAULT_PATH_STRIDE + i] = select(i, ...) or false
	end
	
	self:ReportPathEdit(iPt)
	
end

--[[------------------------------------
	ai:ReportPathEdit

Call whenever a vector in tPath is directly edited or added. iEdit should be the index of the
earliest edited point in the sequence. If iEdit is negative, it's an index starting from the end
of the path sequence (e.g. can negate and pass in the number of added points).

FIXME: having to do this is lame

FIXME: remove smoothed segments affected by the new/edited point
--------------------------------------]]
function ai:ReportPathEdit(iEdit)
	
	if iEdit < 0 then
		iEdit = #self.tPath / self.iPathStride + 1 + iEdit
	end
	
	self.iPathState = self.PATH_HAVE
	
end

--[[------------------------------------
	ai:AddPathPoint
--------------------------------------]]
function ai:AddPathPoint(v3Pt, ...)
	
	self:SetPathPoint(#self.tPath / self.iPathStride + 1, v3Pt, ...)
	
end

--[[------------------------------------
	ai:ShiftPath

Shifts path array left.
--------------------------------------]]
function ai:ShiftPath(iNumShiftPts)
	
	if iNumShiftPts <= 0 then
		return
	end
	
	local tPath = self.tPath
	local iStride = self.iPathStride
	
	--[[
	-- FIXME TEMP
	if self.xPathStart then
		
		if self.xPathStart == tPath[1] and self.yPathStart == tPath[2] and self.zPathStart == tPath[3] then
			
			local iTime = wrp.SimTime()
			local iSpan = iTime - self.iPathStartTime
			gcon.LogF("lap %g", iSpan / 1000.0)
			self.iPathStartTime = iTime
			
		end
		
	else
		
		self.v3PathStart = tPath[1], tPath[2], tPath[3]
		self.iPathStartTime = wrp.SimTime()
		
	end
	--]]
	
	local iLen = #tPath
	local iShift = iStride * iNumShiftPts
	for i = 1, iLen do tPath[i] = tPath[i + iShift] end
	
	self.iNumSmoothSegments = math_max(0, self.iNumSmoothSegments - iNumShiftPts)
	
end

--[[------------------------------------
	ai:InsertPathPoint
--------------------------------------]]
function ai:InsertPathPoint(iPt, v3Pt, ...)
	
	local tPath = self.tPath
	local iLen = #tPath
	local iStride = self.iPathStride
	
	-- FIXME: does breaking the sequence temporarily cause Lua to allocate a hash table?
	for i = iLen + iStride, 1 + (iPt - 1) * iStride, -1 do
		tPath[i + iStride] = tPath[i]
	end
	
	self:SetPathPoint(iPt, v3Pt, ...)
	
end

--[[------------------------------------
	ai:AddPathToPoint

OUT	iState, iNumPts

Var args are other entities to ignore.
--------------------------------------]]
function ai:AddPathToPoint(v3Dest, ...)
	
	local fmap = self.fmap
	local hulMap = fmap:Hull()
	local tPath = self.tPath
	local iLen = #tPath
	local v3Start
	
	if iLen > 0 then
		
		local iIndex = iLen - self.iPathStride
		v3Start = tPath[iIndex + 1], tPath[iIndex + 2], tPath[iIndex + 3]
		
	else
		v3Start = EntPos(self)
	end
	
	local iC = hit_HullTest(hulMap, v3Start, v3Dest, 0, 0, 0, 1, hit_ALL, self.fsetProbeIgnore,
		self, ...)
	
	if iC == hit_NONE then
		
		self:AddPathPoint(v3Dest)
		return pat_GO, 1
		
	else
		
		local iState, iNumPts = pat.InstantFlightPathToPoint(fmap, v3Start, v3Dest, tPath, true,
			iLen, self.iPathExtraStride)
		
		self:ReportPathEdit(-iNumPts)
		return iState, iNumPts
		
	end
	
end

--[[------------------------------------
	ai:SetPathExtraStride
--------------------------------------]]
function ai:SetPathExtraStride(iNumExtra)
	
	self.iPathStride = DEFAULT_PATH_STRIDE + iNumExtra
	self.iPathExtraStride = iNumExtra
	
end

--[[------------------------------------
	ai:AddSmoothPoint
--------------------------------------]]
function ai:AddSmoothPoint(tSmoothPath, v3Pt, iFlags)
	
	local iFore = #tSmoothPath
	
	if iFore == 0 and tSmoothPath[-5] == nil then
		
		-- Set fake passed point
		-- FIXME: this allocates a hash table; is preventing that worth dealing with a non-1 starting index?
		tSmoothPath[-5], tSmoothPath[-4], tSmoothPath[-3] = EntPos(self)
		tSmoothPath[-2] = 0
		tSmoothPath[-1] = 0.0
		tSmoothPath[0] = false
		
	end
	
	tSmoothPath[iFore + 1], tSmoothPath[iFore + 2], tSmoothPath[iFore + 3] = v3Pt
	tSmoothPath[iFore + 4] = iFlags
	
	local iCornerFore = iFore - SMOOTH_PATH_STRIDE
	local v3Corner = tSmoothPath[iCornerFore + 1], tSmoothPath[iCornerFore + 2], tSmoothPath[iCornerFore + 3]
	local nA = tSmoothPath[iCornerFore + 5]
	local v3B = v3Pt - v3Corner
	tSmoothPath[iFore + 5] = vec_Mag3(v3B)
	
	tSmoothPath[iFore + 6] = false -- Don't know next corner's angle, can't calc speed limit yet
	
	-- If we have two points, calculate speed limit of previous segment (previous-previous point)
	if iCornerFore >= 0 then
		
		local iStartFore = iCornerFore - SMOOTH_PATH_STRIDE
		local v3Start = tSmoothPath[iStartFore + 1], tSmoothPath[iStartFore + 2], tSmoothPath[iStartFore + 3]
		local v3A = v3Corner - v3Start
			-- Assume nA is the distance over which the turn is done
		local nCornerSpeed
		
		if nA == 0.0 then
			nCornerSpeed = math_huge -- Already on corner, don't restrict speed
		else
			
			local nB = vec_Mag3(v3B)
			local nAngle = com_SafeArcCos(vec_Dot3(v3A / nA, v3B / nB))
			
			if nAngle == 0.0 then
				nCornerSpeed = math_huge
			else
				nCornerSpeed = nA * self.nTurnRate / nAngle -- segment_length / time_to_reach_angle
			end
			
		end
		
		tSmoothPath[iStartFore + 6] = nCornerSpeed
		
	end
	
	-- v3Corner's tail segment now has a speed limit; add its distance to the count
	self.nLimitedSmoothDist = self.nLimitedSmoothDist + nA
	
end

--[[------------------------------------
	ai:ShiftSmoothPath
--------------------------------------]]
function ai:ShiftSmoothPath(iNumShiftSmooth)
	
	if iNumShiftSmooth <= 0 then
		return
	end
	
	-- Check how many direct points need to be shifted
	-- Also subtract from nLimitedSmoothDist
	local tSmoothPath = self.tSmoothPath
	local iDirectPt = 1
	local nLimitedSmoothDist = self.nLimitedSmoothDist
	
	for iSmoothIndex = 1, iNumShiftSmooth * SMOOTH_PATH_STRIDE, SMOOTH_PATH_STRIDE do
		
		if (tSmoothPath[iSmoothIndex + 3] & SMOOTH_DIRECT) ~= 0 then
			iDirectPt = iDirectPt + 1
		end
		
		--[[ Subtract the tail distance stored by the next point so the new current point's tail
		distance is subtracted right now, while we're on that segment rather than when we're
		past it. This triggers the look-ahead in SmoothPath sooner rather than too late. ]]
		nLimitedSmoothDist = nLimitedSmoothDist - (tSmoothPath[iSmoothIndex + SMOOTH_PATH_STRIDE + 4] or 0.0)
		
	end
	
	self.nLimitedSmoothDist = nLimitedSmoothDist
	
	-- Shift smooth points
	local iLen = #tSmoothPath
	local iShift = SMOOTH_PATH_STRIDE * iNumShiftSmooth
	for i = 1 - SMOOTH_PATH_STRIDE, iLen do tSmoothPath[i] = tSmoothPath[i + iShift] end
		-- The last passed smooth point is kept in the non-positives
	
	-- Shift direct points
	if iDirectPt > 1 then
		self:ShiftPath(iDirectPt - 1)
	end
	
end

--[[------------------------------------
	ai:MaxPathLookDistance
--------------------------------------]]
function ai:MaxPathLookDistance(nStep, nVel, nMinSpeed, nMaxSpeed, nDecelRate, nAccelRate)
	
	local nActualMaxSpeed = math_max(nVel, nMaxSpeed)
	local nSpeedRange = nActualMaxSpeed - nMinSpeed
	local nSpeedDist = math_max(nSpeedRange / nDecelRate, nSpeedRange / nAccelRate) * (nMinSpeed + nActualMaxSpeed) * 0.5
	return nActualMaxSpeed * nStep + nSpeedDist
	
end

--[[------------------------------------
	ai:FollowPath

OUT	v3Vel, nStepRem, bRailMode

FIXME: Return a fake heading vector (based on distance to corners) so caller can do smooth
	orientation if it wants to
	FIXME: FollowPath does hit tests against entities, so it should be updating the orientation
		while moving. Do a callback?

FIXME: Lua runs out of memory when an entity's position becomes undefined; I think pathing has
	something to do with it
--------------------------------------]]
local FOLLOW_PATH_RESULT = {hit_NONE}

function ai:FollowPath(v3Vel, nStep, nMinSpeed, nMaxSpeed, nDecelRate, nAccelRate)
	
	if self.iPathState ~= self.PATH_HAVE then
		return v3Vel, nStep, true
	end
	
	local tSmoothPath = self.tSmoothPath
	local iNumPts = #tSmoothPath / SMOOTH_PATH_STRIDE
	
	if iNumPts < 1 then
		return v3Vel, nStep, true
	end
	
	-- Calculate max distance of relevant speed limits
	local nVel = vec_Mag3(v3Vel)
	local nMaxLookDist = self:MaxPathLookDistance(nStep, nVel, nMinSpeed, nMaxSpeed, nDecelRate, nAccelRate)
	
	--[[ String together acceleration programs that go as fast as possible while meeting the
	path's speed limits ]]
	-- FIXME: It would be more precise if progs were tied to single path segments (a prog couldn't pass a point)
	local tProgs = com.TempTable() -- nInitDist, nInitSpeed, nAcc, ...
	local PROG_STRIDE = 3
		-- Prog's distance/time: d(t) = vt + at^2/2
			-- time/distance:	t(d) = (-v +- sqrt(v^2 + 2ad)) / a, a ~= 0
			--					t(d) = d/v, a == 0
		-- Prog's speed/distance: v(d) = sqrt(i^2 + 2da), i => init speed
			--[[
			speed change => c = at
			final speed => f = i + at = i + c
			d = it + at^2/2
			t = c/a
			d = i(c/a) + a(c/a)^2/2
			d = i(c/a) + a(c^2/a^2)/2
			d = i(c/a) + (c^2/a)/2
			d = i(c/a) + c^2/2a
			d = ic/a + c^2/2a
			da = ic + c^2/2
			0 = 0.5c^2 + ic - da
			c = (-i +- _/i^2 + 2da)) / (2 * 0.5)
			c = -i +- _/i^2 + 2da)
			f = i + c
			f = i + -i +- _/i^2 + 2da)
			f = +- _/i^2 + 2da)
				speed will always be positive in our case
			]]
	
	local v3Pos = EntPos(self)
	local v3Prev = v3Pos
	local nLookDist = 0.0
	local nLatestSpeed
	
	for iPt = 0, iNumPts do
		
		local iPtFore = (iPt - 1) * SMOOTH_PATH_STRIDE
		local nOrigSpeedLimit = tSmoothPath[iPtFore + 6]
		
		if not nOrigSpeedLimit then
			break -- Got to a point without a calculated speed limit; stop, but Think shouldn't let this happen
		end
		
		-- Get total distance to this point
		local v3Pt, nPrevToPtDist
		
		if iPt == 0 then
			v3Pt, nPrevToPtDist = v3Prev, 0.0
		else
			
			v3Pt = tSmoothPath[iPtFore + 1], tSmoothPath[iPtFore + 2], tSmoothPath[iPtFore + 3]
			
			if iPt == 1 then
				nPrevToPtDist = vec_Mag3(v3Pt - v3Prev)
			else
				nPrevToPtDist = tSmoothPath[iPtFore + 5]
			end
			
		end
		
		nLookDist = nLookDist + nPrevToPtDist
		
		if nLookDist >= nMaxLookDist then
			break -- Point is too far away to require any changes in speed right now
		end
		
		-- Ensure that the last prog starts before this point
		local iLastProgFore = #tProgs - PROG_STRIDE
		
		if iLastProgFore >= 0 then
			
			local nLastProgDist = tProgs[iLastProgFore + 1]
			
			if nLastProgDist >= nLookDist then
				
				-- Last prog was created past this point, delete it
					-- Assume the new last prog starts before this point
				for i = 1, PROG_STRIDE do
					tProgs[iLastProgFore + i] = nil
				end
				
				iLastProgFore = iLastProgFore - PROG_STRIDE
				
			end
			
		end
		
		-- Update nLatestSpeed based on the last prog
		if iLastProgFore >= 0 then
			
			local nLastProgDist = tProgs[iLastProgFore + 1]
			local nLastProgSpeed = tProgs[iLastProgFore + 2]
			local nLastProgAcc = tProgs[iLastProgFore + 3]
			nLatestSpeed = math_sqrt(math_max(0.0, nLastProgSpeed * nLastProgSpeed + 2.0 * (nLookDist - nLastProgDist) * nLastProgAcc))
			
		else
			nLatestSpeed = nVel
		end
		
		local nSpeedLimit = com_Clamp(nOrigSpeedLimit, nMinSpeed, nMaxSpeed)
			-- Max speed upon reaching pt and until the next point
		
		-- Check if deceleration is needed
		if nLatestSpeed > nSpeedLimit and nDecelRate > 0.0 then
			
			-- Going too fast
			-- Create an ideal deceleration curve
			local nCurveDist = nLookDist
			local nCurveSpeed = nSpeedLimit
			local nCurveAcc = -nDecelRate
			local nEndDist = nCurveDist
			local bIntersect = false
			
			-- Revise latest progs by figuring out where this curve intersects them on a speed/distance graph
			for iProgFore = #tProgs - PROG_STRIDE, 0, -PROG_STRIDE do
				
				local nProgDist = tProgs[iProgFore + 1]
				local nProgSpeed = tProgs[iProgFore + 2]
				local nProgAcc = tProgs[iProgFore + 3]
				
				-- Check for an intersection
				-- Solve for d in sqrt(i^2 + 2a(d + o)) = sqrt(j^2 + 2bd)
						-- o => nProgDist - nCurveDist
					-- i^2 + 2a(d + o) = j^2 + 2bd
					-- i^2 + 2ad + 2ao = j^2 + 2bd
					-- i^2 + 2ao = j^2 + 2bd - 2ad
					-- i^2 + 2ao - j^2 = 2bd - 2ad
					-- i^2 + 2ao - j^2 = d(2b - 2a)
					-- d = (i^2 + 2ao - j^2) / (2b - 2a)
						-- Note: if two progs have the same acceleration, they never intersect
				
				local nDivisor = 2.0 * (nProgAcc - nCurveAcc)
				
				if nDivisor ~= 0.0 then
					
					-- Prog and curve don't have the same acceleration, they will intersect at some point
					-- Get intersection distance, relative to nProgDist
					local nIntersect = (nCurveSpeed * nCurveSpeed + 2.0 * nCurveAcc * (nProgDist - nCurveDist) - nProgSpeed * nProgSpeed) / nDivisor
					
					if nIntersect > 0.0 and nIntersect <= nEndDist - nProgDist then
						
						-- FIXME: might need epsilons here ^^, but clamp the actual effective distance to this prog's section; it doesn't have to be perfect
							-- FIXME: and if intersect <= 0.0, overwrite this prog entirely
						-- Intersected relevant section
						bIntersect = true
						local iNewFore = iProgFore + PROG_STRIDE
						tProgs[iNewFore + 1] = nProgDist + nIntersect
						tProgs[iNewFore + 2] = math_sqrt(nProgSpeed * nProgSpeed + 2.0 * nIntersect * nProgAcc)
						tProgs[iNewFore + 3] = nCurveAcc
						
						for i = iNewFore + PROG_STRIDE + 1, #tProgs do
							tProgs[i] = nil
						end
						
						break
						
					end
					
				else
					
					-- Accelerations are similar; check if prog and curve are collinear
					local nPtSpeed = math_sqrt(math_max(0.0, nProgSpeed * nProgSpeed + 2.0 * (nCurveDist - nProgDist) * nProgAcc))
					
					if math_abs(nPtSpeed - nCurveSpeed) <= 0.01 then
						
						-- FIXME: smarter epsilon?
						-- Curve is just an extension of prog
						bIntersect = true
						
						for i = iProgFore + PROG_STRIDE + 1, #tProgs do
							tProgs[i] = nil
						end
						
						break
						
					end
					
				end
				
				nEndDist = nProgDist
				
			end
			
			if not bIntersect then
				
				-- No intersection; assume there's not enough distance to decelerate to the limit
				-- Replace all progs with one big deceleration
				-- FIXME: make sure precision error can't cause an intersection miss that would create an early, useless deceleration
				for i = PROG_STRIDE + 1, #tProgs do
					tProgs[i] = nil
				end
				
				-- Decelerate immediately
				tProgs[1] = 0.0
				tProgs[2] = nVel
				tProgs[3] = nCurveAcc
				
				-- Check at what distance the speed limit will be reached
				--[[
					-- FIXME: not needed because a constant prog is always made below after a prog changes speed
				local nIntersect = (nVel * nVel - nSpeedLimit * nSpeedLimit) / (2.0 * -nCurveAcc)
				
				-- If any distance is left before pt, do a constant speed prog
				if nIntersect < nCurveDist then
					
					tProgs[4] = nIntersect
					tProgs[5] = nSpeedLimit
					tProgs[6] = 0.0
					
				end
				]]
				
			end
			
		end
		
		-- Check if acceleration is needed
		if nLatestSpeed < nSpeedLimit then
			
			-- Can go faster, accelerate
			local nCurveDist = nLookDist
			local nCurveSpeed = nLatestSpeed
			local nCurveAcc = nAccelRate
			local iProgFore = #tProgs
			tProgs[iProgFore + 1] = nCurveDist
			tProgs[iProgFore + 2] = nCurveSpeed
			tProgs[iProgFore + 3] = nCurveAcc
			
		end
		
		-- Check if a constant speed prog is needed
		iLastProgFore = #tProgs - PROG_STRIDE
		local nLastProgAcc = tProgs[iLastProgFore + 3]
		
		if iLastProgFore < 0 then
			
			-- No progs; that must mean speed doesn't need to change yet
			-- Create a constant prog
			local iNewFore = iLastProgFore + PROG_STRIDE
			tProgs[iNewFore + 1] = 0.0
			tProgs[iNewFore + 2] = nSpeedLimit
			tProgs[iNewFore + 3] = 0.0
			
		elseif nLastProgAcc ~= 0.0 then
			
			-- Last prog changed speed, so create a constant prog once the speed limit is hit
			-- This might happen after the next point is reached; the next iteration will erase this prog in that case
			local nLastProgDist = tProgs[iLastProgFore + 1]
			local nLastProgSpeed = tProgs[iLastProgFore + 2]
			local nIntersect = (nLastProgSpeed * nLastProgSpeed - nSpeedLimit * nSpeedLimit) / (2.0 * -nLastProgAcc)
			local iNewFore = iLastProgFore + PROG_STRIDE
			tProgs[iNewFore + 1] = nLastProgDist + nIntersect
			tProgs[iNewFore + 2] = nSpeedLimit
			tProgs[iNewFore + 3] = 0.0
			
		end
		
		v3Prev = v3Pt
		
	end
	
	if #tProgs == 0 then
		
		com.FreeTempTable(tProgs)
		return v3Vel, nStep, true
		
	end
	
	--[=====[
	-- Draw speed/time graph
	local nAccumTime = 0.0
	local iPassPt = 0
	local nPassDist
	if tSmoothPath[-5] then
		nPassDist = -vec_Mag3(tSmoothPath[-5] - xPos, tSmoothPath[-4] - yPos, tSmoothPath[-3] - zPos)
	end
	local nPassSpeedLimit = tSmoothPath[0]
	local DRAW_STEP = 0.05
	local DRAW_MAX_TIME = 10.0
	local DRAW_MAX_SPEED = 800.0
	local nDrawXFactor = wrp.VideoWidth() / DRAW_MAX_TIME
	local nDrawYFactor = wrp.VideoHeight() / DRAW_MAX_SPEED
	local rnd_Draw2DLine = grnd.Draw2DLine
	local gui_Draw2DText = ggui.Draw2DText
	for iProgFore = 0, #tProgs, PROG_STRIDE do
		
		local nProgDist = tProgs[iProgFore + 1]
		local nProgSpeed = tProgs[iProgFore + 2]
		local nProgAcc = tProgs[iProgFore + 3]
		local iNextFore = iProgFore + PROG_STRIDE
		local nNextDist = tProgs[iNextFore + 1]
		local nTimeToNext, nCoverDist
		
		if nNextDist then
			
			local nSegment = nNextDist - nProgDist
			
			if nProgAcc == 0.0 then
				nTimeToNext = nSegment / nProgSpeed
			else
				nTimeToNext = (-nProgSpeed + math_sqrt(math_max(0.0, nProgSpeed * nProgSpeed + 2.0 * nProgAcc * nSegment))) / nProgAcc
			end
			
			nCoverDist = nNextDist
			
		else
			
			nTimeToNext = DRAW_MAX_TIME - nAccumTime
			--nCoverDist = nProgDist + nProgSpeed * nTimeToNext + 0.5 * nProgAcc * nTimeToNext * nTimeToNext
			
		end
		
		local nStartTime = nAccumTime
		
		while nPassDist and nPassSpeedLimit and nCoverDist and nCoverDist >= nPassDist do
			
			local nTimeToPass
			local nSegment = nPassDist - nProgDist
			
			if nProgAcc == 0.0 then
				nTimeToPass = nSegment / nProgSpeed
			else
				nTimeToPass = (-nProgSpeed + math_sqrt(math_max(0.0, nProgSpeed * nProgSpeed + 2.0 * nProgAcc * nSegment))) / nProgAcc
			end
			
			local nPassSpeed = nProgSpeed + nTimeToPass * nProgAcc
			nTimeToPass = nTimeToPass + nStartTime
			gui_Draw2DText(nTimeToPass * nDrawXFactor + 9, nPassSpeedLimit * nDrawYFactor - 9, string.format("%.0f", nPassSpeedLimit))
			
			--[[rnd_Draw2DLine(nTimeToPass * nDrawXFactor, nPassSpeed * nDrawYFactor + 16,
				nTimeToPass * nDrawXFactor, nPassSpeed * nDrawYFactor - 16, 31)]]
			
			rnd_Draw2DLine(nTimeToPass * nDrawXFactor, nPassSpeed * nDrawYFactor,
				nTimeToPass * nDrawXFactor, nPassSpeedLimit * nDrawYFactor, 31)
			
			rnd_Draw2DLine(nTimeToPass * nDrawXFactor, nPassSpeedLimit * nDrawYFactor,
				DRAW_MAX_TIME * nDrawXFactor, nPassSpeedLimit * nDrawYFactor, 0)
			
			iPassPt = iPassPt + 1
			local iFore = (iPassPt - 1) * SMOOTH_PATH_STRIDE
			local nGetDist = tSmoothPath[iFore + 5]
			nPassDist = nGetDist and (nPassDist + nGetDist) or nil
			nPassSpeedLimit = tSmoothPath[iFore + 6]
			
		end
		
		nAccumTime = nAccumTime + nTimeToNext
		local nPrevTime = nStartTime
		local nPrevSpeed = nProgSpeed
		
		while true do
			
			local nCurTime = math_min(nPrevTime + DRAW_STEP, nAccumTime)
			local nCurSpeed = nProgSpeed + nProgAcc * (nCurTime - nStartTime)
			
			rnd_Draw2DLine(nPrevTime * nDrawXFactor, nPrevSpeed * nDrawYFactor,
				nCurTime * nDrawXFactor, nCurSpeed * nDrawYFactor, 31)
			
			nPrevTime, nPrevSpeed = nCurTime, nCurSpeed
			
			if nCurTime == nAccumTime then
				break
			end
			
		end
		
		if not nNextDist then
			break
		end
		
	end
	--]=====]
	
	-- Follow acceleration programs for nStep seconds
	local nEndSpeed
	local nStepRem = nStep
	local iTravelPt = 0
	local v3TravelPt
	local v3HitPt
	local v3New = v3Pos
	local iShift = 0
	local v3LastMoveDir
	local hulSelf = self:Hull()
	local q4HullOri = self:HullOri()
	local fsetIgnore = self.fsetIgnore
	local bHit = false
	local Move = self.Move
	
	for iProgFore = 0, #tProgs, PROG_STRIDE do
		
		local nStepProgStart = nStepRem
		
		-- Get prog
		local nProgDist = tProgs[iProgFore + 1]
		local nProgSpeed = tProgs[iProgFore + 2]
		local nProgAcc = tProgs[iProgFore + 3]
		
		-- Get next prog's starting dist
		local iNextFore = iProgFore + PROG_STRIDE
		local nNextDist = tProgs[iNextFore + 1]
		
		-- Check if we're stopping on this prog and calculate how much distance it covers
		local nTimeToStop, nTravelDist
		
		if nNextDist then
			
			-- Check how many seconds will pass until next prog
			local nSegment = nNextDist - nProgDist
			local nTimeToNext
			
			if nProgAcc == 0.0 then
				nTimeToNext = nSegment / nProgSpeed
			else
				nTimeToNext = (-nProgSpeed + math_sqrt(math_max(0.0, nProgSpeed * nProgSpeed + 2.0 * nProgAcc * nSegment))) / nProgAcc
			end
			
			if nTimeToNext < nStepRem then
				
				-- Going to next prog
				nTravelDist = nSegment
				nStepRem = nStepProgStart - nTimeToNext
				
			else
				
				-- Going to run out of time before reaching next prog
				nTimeToStop = nStepProgStart
				nStepRem = 0.0
				
			end
			
		else
			
			-- No next prog; use current prog until time runs out
			nTimeToStop = nStepProgStart
			nStepRem = 0.0
			
		end
		
		if nTimeToStop then
			
			-- Stopping on this prog; get distance based on time
			nTravelDist = nProgSpeed * nTimeToStop + 0.5 * nProgAcc * nTimeToStop * nTimeToStop
			
		end
		
		-- Travel along path to get new position and count passed path points
		local nRemainingDist = nTravelDist
		
		while nRemainingDist > 0.0 do
			
			if not xTravelPt then
				
				-- No point saved, get next point on path
				iTravelPt = iTravelPt + 1
				local iPtFore = (iTravelPt - 1) * SMOOTH_PATH_STRIDE
				v3TravelPt = tSmoothPath[iPtFore + 1], tSmoothPath[iPtFore + 2], tSmoothPath[iPtFore + 3]
				
				-- Test segment for obstacles
				local iC, nTF, nTL, v3Norm, entHit = hit_HullTest(hulSelf, v3New, v3TravelPt,
					q4HullOri, hit_ENTITIES, fsetIgnore, self)
				
				if iC == hit_INTERSECT then
					iC, nTF, nTL, v3Norm = hit_HIT, 0, 0, 0, 0, 0
				end
				
				if iC == hit_HIT then
					-- Going to hit something on this segment, save stopping position
						-- Position is BEFORE the hit happens; actual hit should be handled in free mode
					v3HitPt = hit.RespondStop(v3New, v3TravelPt, iC, nTF, nTL, v3Norm)
				end
				
			end
			
			local v3Dif = v3TravelPt - v3New
			local nDistToPt = vec_Mag3(v3Dif) -- FIXME: use tSmoothPath[iPtFore + 5] whenever possible (when iTravelPt just changed)
			local v3Dir
			
			if nDistToPt > 0.0 then
				
				v3Dir = v3Dif / nDistToPt
				v3LastMoveDir = v3Dir
				
			else
				v3Dir = 0.0, 0.0, 0.0
			end
			
			if xHitPt then
				
				-- There's a saved upcoming hit on this segment
				local nDistToHit = vec_Mag3(v3HitPt - v3New)
				
				if nDistToHit < nRemainingDist then
					
					-- The hit occurs during this prog, recalculate travel time
					bHit = true
					
					if nDistToHit > nDistToPt then -- Paranoid about imprecision
						nDistToHit = nDistToPt
					end
					
					local nTimeToHit
					
					if nProgAcc == 0.0 then
						nTimeToHit = nDistToHit / nProgSpeed
					else
						nTimeToHit = (-nProgSpeed + math_sqrt(math_max(0.0, nProgSpeed * nProgSpeed + 2.0 * nProgAcc * nDistToHit))) / nProgAcc
					end
					
					if nTimeToStop and nTimeToHit > nTimeToStop then -- Paranoid about imprecision
						nTimeToHit = nTimeToStop
					end
					
					nStepRem = math_max(0.0, nStepProgStart - nTimeToHit)
					nTimeToStop = nTimeToHit
					
					-- And clamp travel distance
					nRemainingDist = nDistToHit
					
				end
				
			end
			
			local v3Prev = v3New
			
			if nRemainingDist < nDistToPt then
				
				v3New = v3New + v3Dir * nRemainingDist
				nRemainingDist = 0.0
				
			else
				
				v3New = v3TravelPt
				nRemainingDist = nRemainingDist - nDistToPt
				iShift = iShift + 1
				v3TravelPt = nil, nil, nil
				v3HitPt = nil, nil, nil
				
			end
			
			if Move then
				
				-- Call Move as if this was a phy.Push movement so entity can update its state
				local nStepDelta = nStepProgStart - nStepRem
				local v3AvgVel
				
				if nStepDelta > 0.0 then
					v3AvgVel = (v3New - v3Prev) / nStepDelta
				else
					v3AvgVel = 0.0, 0.0, 0.0
				end
				
				--[[ FIXME: Ignoring returned values for now, but that means things like
				teleporters wouldn't work. Really should revert to free mode or pause further
				movement on this tick if Move returns a different v3Pos or v3Vel. ]]
				Move(self, FOLLOW_PATH_RESULT, v3New, v3Prev, v3AvgVel, v3AvgVel, nStepRem)
				
			end
			
		end
		
		-- Follow next prog?
		if nTimeToStop then
			
			-- No more time remaining, done
			nEndSpeed = com_Clamp(nProgSpeed + nProgAcc * nTimeToStop, nMinSpeed, nMaxSpeed)
			break
			
		end
		
	end
	
	com.FreeTempTable(tProgs)
	EntSetPos(self, v3New)
	self:ShiftSmoothPath(iShift)
	local v3EndVel = v3Vel
	
	if nEndSpeed and xLastMoveDir then
		v3EndVel = v3LastMoveDir * nEndSpeed
	end
	
	--[=====[
	-- Draw current speed on graph
	if nEndSpeed then
		ggui.Draw2DText(9, nEndSpeed * nDrawYFactor, string.format("%.0f", nEndSpeed))
	end
	--]=====]
	
	--[=====[
	-- Draw turn circles at current and max speed
	if #tSmoothPath / SMOOTH_PATH_STRIDE >= 3 then
		
		local v3First = v3New
		local v3Second = tSmoothPath[1], tSmoothPath[2], tSmoothPath[3]
		local v3Third = tSmoothPath[7], tSmoothPath[8], tSmoothPath[9]
		local v3Fwd = vec_Normalized3(v3Second - v3First)
		local v3Cross = vec_Normalized3(vec_Cross(v3Third - v3Second, v3Fwd))
		local v3Side = vec_Normalized3(vec_Cross(v3Fwd, v3Cross))
		local nRadius = vec_Mag3(v3EndVel) / self.nTurnRate
		local v3Center = v3New + v3Side * nRadius
		com.DrawCircle(v3Center, nRadius, v3Cross, 12)
		local nMaxRadius = nMaxSpeed / self.nTurnRate
		local v3MaxCenter = v3New + v3Side * nMaxRadius
		com.DrawCircle(v3MaxCenter, nMaxRadius, v3Cross, 76)
		
	end
	--]=====]
	
	--[=====[
	-- Print turn angle speed over last ANGLE_HISTORY_TIME seconds
	local tAngleHistory = self.tAngleHistory
	if not tAngleHistory then
		tAngleHistory = {}
		self.tAngleHistory = tAngleHistory
	end
	local iHisLen = #tAngleHistory
	tAngleHistory[iHisLen + 1] = vec_DirAngle3(v3EndVel, v3Vel)
	tAngleHistory[iHisLen + 2] = nStep
	local ANGLE_HISTORY_TIME = 1.0
	local nExcessHistory = -ANGLE_HISTORY_TIME
	for i = 2, #tAngleHistory, 2 do
		nExcessHistory = nExcessHistory + tAngleHistory[i]
	end
	while nExcessHistory > 0.0 do
		if tAngleHistory[2] <= nExcessHistory then
			nExcessHistory = nExcessHistory - tAngleHistory[2]
			for i = 1, #tAngleHistory do
				tAngleHistory[i] = tAngleHistory[i + 2]
			end
		else
			local nBef = tAngleHistory[2]
			tAngleHistory[2] = tAngleHistory[2] - nExcessHistory
			tAngleHistory[1] = tAngleHistory[1] * (tAngleHistory[2] / nBef)
			nExcessHistory = 0.0
		end
	end
	if nExcessHistory == 0.0 then
		local nTotalAngle = 0.0
		for i = 1, #tAngleHistory, 2 do
			nTotalAngle = nTotalAngle + tAngleHistory[i]
		end
		local nAnglePerSec = nTotalAngle / ANGLE_HISTORY_TIME
		gcon.LogF("angle change %g", nAnglePerSec)
	end
	--]=====]
	
	return v3EndVel, nStepRem, not bHit
	
end

--[[------------------------------------
	ai:ClearSmoothPath
--------------------------------------]]
function ai:ClearSmoothPath()
	
	self.iNumSmoothSegments = 0
	self.nLimitedSmoothDist = 0.0
	local tSmoothPath = self.tSmoothPath
	for i = 1 - SMOOTH_PATH_STRIDE, #tSmoothPath do tSmoothPath[i] = nil end
	self.v3LastSmoothPt = nil, nil, nil -- FIXME TEMP
	
end

--[[------------------------------------
	ai:SmoothPath

OUT	bMetLookAhead

Fills tSmoothPath with enough points so that it contains at least nLookAhead distance of
segments that have their speed-limit calculated and have not yet been traveled on. Returns false
if nLookAhead can't be met. nSplitDist is the maximum distance between smooth points. If
bAllowMorePath is true, ConclusiveMorePath is called when more direct path points are needed.

Thinks should call this and pass in MaxPathLookDistance's result for nLookAhead to ensure they
have enough smooth path for FollowPath to traverse until the next Think.

FIXME: Figure out how to handle non-circuit paths. If the agent is allowed to stop and hover,
	the rest of the smooth path should have its speed limit set to infinity. If the agent can't
	stop, more path _must_ be created. Default to a backup MorePath func? Should every MorePath
	transition to a different path type? Have a decision-making callback?
--------------------------------------]]
local spl3Temp = com.Spline3()

function ai:SmoothPath(nLookAhead, nSplitDist, bAllowMorePath)
	
	if self.iPathState ~= self.PATH_HAVE then
		return false
	end
	
	local ConclusiveMorePath = self.ConclusiveMorePath
	local tPath = self.tPath
	local tSmoothPath = self.tSmoothPath
	local iNumSmoothSegments = self.iNumSmoothSegments
	local iStride = self.iPathStride
	local iNumPts = #tPath / iStride
	local iPt = iNumSmoothSegments + 1
	local AddSmoothPoint = self.AddSmoothPoint
	spl3Temp:SetNumPts(0)
	spl3Temp:SetVelFinal(0.0, 0.0, 0.0)
	
	while self.nLimitedSmoothDist < nLookAhead do
		
		while iPt > iNumPts do
			
			if (not bAllowMorePath or ConclusiveMorePath(self) == pat_FAIL) then
				break
			end
			
			iNumPts = #tPath / iStride
			
		end
		
		if iPt > iNumPts then
			break
		end
		
		-- Found an unsmoothed segment
		local iCurFore = (iPt - 1) * iStride
		local v3Cur = tPath[iCurFore + 1], tPath[iCurFore + 2], tPath[iCurFore + 3]
		local v3Prev
		
		if iPt >= 2 then
			
			local iPrevFore = iCurFore - iStride
			v3Prev = tPath[iPrevFore + 1], tPath[iPrevFore + 2], tPath[iPrevFore + 3]
			
		else
			v3Prev = EntPos(self)
		end
		
		local nLinearDist = vec_Mag3(v3Cur - v3Prev)
		
		if nSplitDist < nLinearDist then
			
			-- Segment is big enough to smooth, insert points
			-- FIXME: localize Spline3's functions
			spl3Temp:SetPt(0, v3Prev, 0.0)
			spl3Temp:SetPt(1, v3Cur, nLinearDist)
			
			if iPt + 1 <= iNumPts then
				
				local iNextFore = iPt * iStride
				local v3Next = tPath[iNextFore + 1], tPath[iNextFore + 2], tPath[iNextFore + 3]
				spl3Temp:SetPt(2, v3Next, nLinearDist + vec_Mag3(v3Next - v3Cur))
				
			end
			
			local v3VI
			
			if iPt == 1 then
				v3VI = vec_Normalized3(self.v3Vel)
			else
				
				local iPSFore = #tSmoothPath - SMOOTH_PATH_STRIDE * 2
				local v3PrevSmooth
				
				if iPSFore >= 0 then
					v3PrevSmooth = tSmoothPath[iPSFore + 1], tSmoothPath[iPSFore + 2], tSmoothPath[iPSFore + 3]
				else
					v3PrevSmooth = EntPos(self)
				end
				
				v3VI = vec_Normalized3(v3Prev - v3PrevSmooth)
				
			end
			
			spl3Temp:SetVelInit(v3VI)
			spl3Temp:CalculateCoefficients()
			local iNumNewSegments = math_floor(nLinearDist / nSplitDist) + 1
			
			for iSplit = 1, iNumNewSegments - 1 do
				
				local v3SmoothPos = spl3Temp:Pos(0, nLinearDist * (iSplit / iNumNewSegments))
				AddSmoothPoint(self, tSmoothPath, v3SmoothPos, 0)
				
			end
			
		end
		
		AddSmoothPoint(self, tSmoothPath, v3Cur, SMOOTH_DIRECT) -- Add direct point to the smooth path
		iNumSmoothSegments = iNumSmoothSegments + 1
		iPt = iPt + 1
		
	end
	
	self.iNumSmoothSegments = iNumSmoothSegments
	return self.nLimitedSmoothDist >= nLookAhead
	
end

--[[------------------------------------
	ai:ConclusiveMorePath

OUT	iState

Marks the path as failed if MorePath is nil or returns pat.FAIL and tPath has no more points.
--------------------------------------]]
function ai:ConclusiveMorePath()
	
	local MorePath = self.MorePath
	local iState = (not MorePath) and pat_FAIL or MorePath(self)
	
	if iState == pat_FAIL and #self.tPath == 0 then
		self:FailPath()
	end
	
	return iState
	
end

--[[------------------------------------
	ai:MorePathToDistance

OUT	bGotRequest

Calls ConclusiveMorePath until the path is at least nWantDist long.

If iNumWantExtra is given, calls ConclusiveMorePath to get iNumWantExtra points beyond the point
that passed nWantDist. This is useful for ensuring SmoothPath has an extra segment beyond its
look-ahead distance to base the last curve on.

If the distance and extra points are achieved, returns true.
--------------------------------------]]
function ai:MorePathToDistance(nWantDist, iNumWantExtra)
	
	local nCurDist, iStopPt, v3Prev = self:PathDist(nWantDist)
	local tPath = self.tPath
	local iStride = self.iPathStride
	local iIndex = iStopPt * iStride
	local ConclusiveMorePath = self.ConclusiveMorePath
	
	if nCurDist >= nWantDist then
		goto have_distance -- Already have distance
	end
	
	-- Need more points to get distance
	while nCurDist < nWantDist do
		
		if ConclusiveMorePath(self) ~= pat_GO then
			return false
		end
		
		while iIndex < #tPath do
			
			local v3Cur = tPath[iIndex + 1], tPath[iIndex + 2], tPath[iIndex + 3]
			nCurDist = nCurDist + vec_Mag3(v3Cur - v3Prev)
			v3Prev = v3Cur
			iIndex = iIndex + iStride
			
		end
		
	end
	
	::have_distance::
	local bGotExtra = false
	
	if iNumWantExtra then
		
		local iNumPts = #tPath / iStride
		local iNumWantPts = iStopPt + iNumWantExtra
		
		while iNumPts < iNumWantPts do
			
			if ConclusiveMorePath(self) ~= pat_GO then
				break
			end
			
			iNumPts = #tPath / iStride
			
		end
		
		bGotExtra = iNumPts >= iNumWantPts
		
	end
	
	return bGotExtra
	
end

--[[------------------------------------
	ai:EnsureSufficientSmoothPath

FIXME: remove
--------------------------------------]]
function ai:EnsureSufficientSmoothPath(v3Vel, nMinSpeed, nMaxSpeed, nDecelRate, nAccelRate, nSplitDist)
	
	local nStep = wrp_TimeStep()
	local nVel = vec_Mag3(v3Vel)
	local nMaxLookDist = self:MaxPathLookDistance(nStep, nVel, nMinSpeed, nMaxSpeed, nDecelRate, nAccelRate)
	
	self:MorePathToDistance(nMaxLookDist, 1)
	
end

--[[------------------------------------
	ai:WaitMorePath

OUT	pat_WAIT
--------------------------------------]]
function ai:WaitMorePath() return pat_WAIT end

--[[------------------------------------
	ai:DrawPath
--------------------------------------]]
function ai:DrawPath(bSmoothPath, nPointRadius, nSpeedLimitUp, iPointColor, iLineColor, nTime)
	
	nTime = nTime or self:ThinkStep()
	local tPath, iStride
	
	if bSmoothPath then
		tPath, iStride = self.tSmoothPath, SMOOTH_PATH_STRIDE
	else
		
		tPath, iStride = self.tPath, self.iPathStride
		nSpeedLimitUp = nil
		
	end
	
	local v3Prev = self:Pos()
	
	for i = 1, #tPath, iStride do
		
		local v3Pt
		
		if i == 1 and self.xPathTar then
			v3Pt = self.v3PathTar
		else
			v3Pt = tPath[i], tPath[i + 1], tPath[i + 2]
		end
		
		rnd.DrawLine(v3Prev, v3Pt, iLineColor, nTime)
		
		if nPointRadius then
			com.DrawCross(v3Pt, nPointRadius, iPointColor, iPointColor, iPointColor, nTime)
		end
		
		if nSpeedLimitUp then
			
			local nSpeedLimit = tPath[i + 5]
			
			if nSpeedLimit then
				gui.Draw3DText(v2Pt, zPt + nSpeedLimitUp, string.format("%.0f", nSpeedLimit), 1, 0, nTime)
			end
			
		end
		
		v3Prev = v3Pt
		
	end
	
end

--[[------------------------------------
	ai:PathDist

OUT	nDist, iStopPt, v3StopPt
--------------------------------------]]
function ai:PathDist(nSoftLimit)
	
	nSoftLimit = nSoftLimit or math_huge
	local tPath = self.tPath
	local nDist, v3Prev = 0.0, EntPos(self)
	local iPt = 0
	
	for i = 1, #tPath, self.iPathStride do
		
		if nDist >= nSoftLimit then
			break
		end
		
		iPt = iPt + 1
		
		local v3Pt = tPath[i], tPath[i + 1], tPath[i + 2]
		nDist = nDist + vec_Mag3(v3Pt - v3Prev)
		v3Prev = v3Pt
		
	end
	
	return nDist, iPt, v3Prev
	
end

--[[------------------------------------
	ai:SmoothPathDist

OUT	nDist, nStopPt, v3StopPt
--------------------------------------]]
function ai:SmoothPathDist(nLimit, bHardLimit)
	
	nLimit = nLimit or math_huge
	local tPath = self.tSmoothPath
	local nDist, v3Prev = 0.0, EntPos(self)
	local iPt = 0
	
	for i = 1, #tPath, SMOOTH_PATH_STRIDE do
		
		if nDist >= nLimit then
			break
		end
		
		iPt = iPt + 1
		
		local v3Pt = tPath[i], tPath[i + 1], tPath[i + 2]
		local nAdd = vec_Mag3(v3Pt - v3Prev)
		
		if bHardLimit and nDist + nAdd >= nLimit then
			
			local nF = (nLimit - nDist) / nAdd
			return nLimit, iPt + nF, v3Prev + (v3Pt - v3Prev) * nF
			
		end
		
		nDist = nDist + nAdd
		v3Prev = v3Pt
		
	end
	
	return nDist, iPt, v3Prev
	
end

--[============================================================================================[


	GOAL PATH


]============================================================================================]--

--[[------------------------------------
	ai:SetGoalPath
--------------------------------------]]
function ai:SetGoalPath(v3Goal)
	
	self:ClearPath()
	self.MorePath = self.WaitMorePath
	self.ResetPath = self.ResetGoalPath
	
	local v3Pos = EntPos(self)
	local tPath = self.tPath
	
	local iState, iNumPts = pat.InstantFlightPathToPoint(self.fmap, v3Pos, v3Goal, tPath, true,
		#tPath, self.iPathExtraStride)
	
	if iState == pat_GO then
		self:ReportPathEdit(-iNumPts)
	else
		self:FailPath()
	end
	
end

--[[------------------------------------
	ai:ResetGoalPath
--------------------------------------]]
function ai:ResetGoalPath()
	
	local tPath = self.tPath
	local iLen = #tPath
	
	if iLen == 0 then
		
		self:FailPath()
		return
		
	end
	
	local iFore = iLen - self.iPathStride
	self:SetGoalPath(tPath[iFore + 1], tPath[iFore + 2], tPath[iFore + 3])
	
end

--[============================================================================================[


	ENTITY PATH


]============================================================================================]--

--[[------------------------------------
	ai:SetEntityPath
--------------------------------------]]
function ai:SetEntityPath(entPath, bReverse)
	
	self:ClearPath()
	self:SetPathExtraStride(1)
	
	self.MorePath = self.MoreEntityPath
	self.ResetPath = self.ResetEntityPath
	self.entPath = entPath
		--[[ FIXME: Maybe tPath should always store the last consumed waypoint in the non-positive indices so storing stuff like this isn't necessary
			Doing that will allocate another hash map though
			The first waypoint could always be kept, instead, but then the AI cannot check if there's more path with a length check
		]]
	self.bReversePath = bReverse -- FIXME: store this for every waypoint in tPath?
	
	if entPath then
		self.iPathState = self.PATH_HAVE
	end
	
end

--[[------------------------------------
	ai:MoreEntityPath

OUT	iState

FIXME: overwrite to make non-random path decisions during flee?
--------------------------------------]]
function ai:MoreEntityPath()
	
	-- Add waypoint(s) to current entPath
	local entPath = self.entPath
	
	if not entPath then
		return pat_FAIL
	end
	
	local fmap = self.fmap
	local hulMap = fmap:Hull()
	local tPath = self.tPath
	local iLen = #tPath
	local v3Path = EntPos(entPath)
	local v3Start
	
	if iLen > 0 then
		
		local iIndex = iLen - self.iPathStride
		v3Start = tPath[iIndex + 1], tPath[iIndex + 2], tPath[iIndex + 3]
		
	else
		v3Start = EntPos(self)
	end
	
	local iState, iNumPts = self:AddPathToPoint(v3Path)
	
	if iState == pat_GO then
		tPath[iLen + iNumPts * self.iPathStride] = entPath
	elseif iState == pat_FAIL then
		return iState
	end
	
	-- Update entPath for next MorePath call
	if self.bReversePath then
		
		if entPath.iNumPrevs > 0 then
			entPath = entPath[entPath.iNumNexts + math_random(entPath.iNumPrevs)]
		else
			entPath = nil
		end
		
	else
		
		if entPath.iNumNexts > 0 then
			entPath = entPath[math_random(entPath.iNumNexts)]
		else
			entPath = nil
		end
		
	end
	
	self.entPath = entPath
	return iState
	
end

--[[------------------------------------
	ai:ResetEntityPath
--------------------------------------]]
function ai:ResetEntityPath()
	
	local tPath = self.tPath
	local iStride = self.iPathStride
	
	for i = DEFAULT_PATH_STRIDE + 1, #tPath, iStride do
		
		if scn_EntityExists(tPath[i]) then
			
			self:SetEntityPath(tPath[i], self.bReversePath)
			return
			
		end
		
	end
	
	-- Couldn't find a waypoint referencing an entity
	self:FailPath()
	
end

--[============================================================================================[


	CHASE PATH


]============================================================================================]--

--[[------------------------------------
	ai:SetChasePath

FIXME: add some prediction points if nearby?
FIXME: optional search cost limit so it can't take a long time?
--------------------------------------]]
function ai:SetChasePath()
	
	self:ClearPath()
	
	self.MorePath = self.WaitMorePath
	self.ResetPath = self.SetChasePath
	
	local entTarget = self.entTarget
	local v3Tar = EntPos(entTarget)
	local v3Pos = EntPos(self)
	local tPath = self.tPath
	
	local iState, iNumPts = pat.InstantFlightPathToPoint(self.fmap, v3Pos, v3Tar, tPath, true,
		#tPath, self.iPathExtraStride)
	
	if iState == pat_GO then
		self:ReportPathEdit(-iNumPts)
	else
		self:FailPath()
	end
	
end

return ai