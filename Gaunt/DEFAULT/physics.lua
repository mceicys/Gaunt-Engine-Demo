-- physics.lua
-- FIXME: optimize property gets; less ors and more caching
-- FIXME: optimize math
LFV_EXPAND_VECTORS()

local hit = ghit
local scn = gscn
local scr = gscr
local vec = gvec
local wrp = gwrp

local tLocalResults = {}
local iPushes = 0

local phy = {}
phy.zGravity = -76 -- Mega gravity (~8x) because it makes lift more important

local EntPos = scn.EntPos
local EntSetPos = scn.EntSetPos
local EntHull = scn.EntHull
local EntHullOri = scn.EntHullOri
local hit_NONE = hit.NONE
local hit_HIT = hit.HIT
local hit_INTERSECT = hit.INTERSECT
local hit_ALL = hit.ALL
local hit_HullTest = hit.HullTest
local hit_MoveSlide = hit.MoveSlide
local math_abs = math.abs
local math_acos = math.acos
local math_asin = math.asin
local math_huge = math.huge
local math_min = math.min
local math_max = math.max
local math_pi = math.pi
local math_sin = math.sin
local math_sqrt = math.sqrt
local vec_Dot2 = vec.Dot2
local vec_Dot3 = vec.Dot3
local vec_Mag2 = vec.Mag2
local vec_Mag3 = vec.Mag3
local vec_MagSq3 = vec.MagSq3
local vec_Normalized3 = vec.Normalized3
local vec_Resized3 = vec.Resized3
local wrp_TimeStep = wrp.TimeStep

local phy_Push
local phy_Enabled

--[[------------------------------------
	phy.Init

Initializes essential physics-object variables in ent.
--------------------------------------]]
function phy.Init(ent)
	
	ent.v3Vel = 0.0, 0.0, 0.0
	ent.bPushing = false -- true if ent has a phy.Push on the call stack
	
end

--[[------------------------------------
	phy.Push

OUT	v3Vel, bExec

Pushes ent with instant velocity v3Vel * nTime until time is up (the push finishes), velocity is
depleted, or the maximum number of moves have been done. Pushes can stack a limited number of
times before calls return without executing. Returns the resulting velocity and whether the push
was executed.
--------------------------------------]]
function phy.Push(ent, v3Vel, nTime)
	
	if iPushes >= 8 --[[max pushes in call stack]] or ent.bPushing then
		return v3Vel, false
	end
	
	iPushes = iPushes + 1
	ent.bPushing = true
	local hulEnt = EntHull(ent)
	local v3Pos = EntPos(ent)
	local v3Old = v3Pos
	local q4Ori = EntHullOri(ent)
	local fsetIgnore = ent.fsetIgnore or phy.fsetIgnore
	local Move = ent.Move
	local tRes = tLocalResults[iPushes]
	
	if not tRes then
		
		tLocalResults[iPushes] = {}
		tRes = tLocalResults[iPushes]
		
	end
	
	tRes[1] = hit_NONE
	
	for i = 1, 8 --[[max moves in a push]] do
		
		local v3PrevPos = v3Pos
		local v3PrevVel = v3Vel
		
		v3Pos, v3Vel, nTime = hit_MoveSlide(hulEnt, v3Pos, v3PrevVel, q4Ori, nTime, tRes, 0.0,
			0.0, hit_ALL, fsetIgnore, ent)
		
		if Move then
			v3Pos, v3Vel = Move(ent, tRes, v3Pos, v3PrevPos, v3Vel, v3PrevVel, nTime)
		end
		
		local entHit = tRes[7]
		
		if entHit then
			
			local Touch = entHit.Touch
			
			if Touch then
				v3Pos, v3Vel = Touch(ent, tRes, v3Pos, v3PrevPos, v3Vel, v3PrevVel, nTime)
			end
			
		end
		
		if nTime == 0.0 or (xVel == 0.0 and yVel == 0.0 and zVel == 0.0) then
			break
		end
		
	end
	
	EntSetPos(ent, v3Pos)
	ent.bPushing = false
	iPushes = iPushes - 1
	return v3Vel, true
	
end

phy_Push = phy.Push

--[[------------------------------------
	phy.Enabled

OUT	bEnabled

Returns true if ent is physics-enabled.
--------------------------------------]]
function phy.Enabled(ent)
	
	return ent.xVel ~= nil
	
end

phy_Enabled = phy.Enabled

--[[------------------------------------
	phy.Friction

OUT	v3Vel, nDec
--------------------------------------]]
function phy.Friction(v3Vel, nFac, nMin, nMax, nStep)
	
	nMin = nMin or 0.0
	nMax = nMax or math_huge
	nStep = nStep or wrp_TimeStep()
	local nVel = vec_Mag3(v3Vel)
	local nBase = nVel
	
	if nBase < nMin then
		nBase = nMin
	elseif nBase > nMax then
		nBase = nMax
	end
	
	local nDec = nBase * nFac * nStep
	local nNew = nVel - nDec
	
	if nNew > 0.0 then
		
		local nMul = nNew / nVel
		v3Vel = v3Vel * nMul
		
	else
		
		v3Vel = 0.0, 0.0, 0.0
		nDec = nVel
		
	end
	
	return v3Vel, nDec
	
end

--[[------------------------------------
	phy.Fly

OUT	v3Vel

Normal flight with turn resistance at a high speed. Returns new velocity. bClamp disables a
speed exploit when true.
--------------------------------------]]
function phy.Fly(v3Vel, v3InDir, nAcc, nTop, bClamp, nMinTurn, nStep)
	
	local nInMag = vec_Mag3(v3InDir)
	
	if nInMag > 0.0 then
		
		nStep = nStep or wrp_TimeStep()
		local v3Go = v3InDir / nInMag
		local nVel = vec_Mag3(v3Vel)
		local nDot = vec_Dot3(v3Go, v3Vel)
		local nAdd = nAcc * nStep
		
		-- Accelerate just enough to reach top in original direction; adds turn resistance
		if nDot > 0.0 then
			nAdd = math_min(math_max(nTop - nVel, nMinTurn * nStep) * nVel / nDot, nAdd)
		end
		
		v3Vel = v3Vel + v3Go * nAdd
		
		if bClamp then
			
			local nClamp = math_max(nVel, nTop)
			local nTmp = vec_Mag3(v3Vel)
			
			if nTmp > nClamp then
				
				local nMul = nClamp / nTmp
				v3Vel = v3Vel * nMul
				
			end
			
		end
		
	end
	
	return v3Vel
	
end

--[[------------------------------------
	phy.SprintFly

OUT	v3Vel

Beyond nFast speed, acceleration experiences its own friction or "drag". nDragFac determines the
natural top speed. nHaste and nNegHaste affect how quickly the natural top speed is approached
once experiencing drag. nDec is the velocity loss returned by the last Friction call.
--------------------------------------]]
function phy.SprintFly(v3Vel, v3In, nAcc, nFast, bClamp, nMinTurn, nDragFac, nHaste, nNegHaste,
	nDec)
	
	local nInMag = vec_Mag3(v3In)
	
	if nInMag > 0.0 then
		
		local nStep = wrp_TimeStep()
		local v3Go = v3In / nInMag
		local nVel = vec_Mag3(v3Vel)
		local nBase = nVel + nDec -- 3D speed + 3D friction loss
		nMinTurn = math_min(nMinTurn, nBase) -- So slow speeds still feel drifty
		local nDiff = nAcc - nBase * nDragFac
		local nFinalHaste = nDiff >= 0.0 and nHaste or nNegHaste
		local nInc = nDiff * nFinalHaste * nStep -- Speed differential
		local nClamp = math_max(nFast, math_max(nVel, nBase + nInc))
		local nDot = vec_Dot3(v3Go, v3Vel)
		local nAdd = nAcc * nStep
		
		if nDot > 0.0 then
			nAdd = math_min(math_max(nClamp - nVel, nMinTurn * nStep) * nVel / nDot, nAdd)
		end
		
		gcon.LogF("add %g", nAdd / nStep)
		
		v3Vel = v3Vel + v3Go * nAdd
		local nReq = vec_Mag3(v3Vel)
		
		if bClamp and nReq > nClamp then
			
			local nMul = nClamp / nReq
			v3Vel = v3Vel * nMul
			
		end
		
	end
	
	return v3Vel
	
end

--[[------------------------------------
	phy.BasicFly

OUT	v3Vel
--------------------------------------]]
function phy.BasicFly(v3Vel, v3In, nAcc, nTop)
	
	local nInMag = vec_Mag3(v3In)
	
	if nInMag > 0.0 then
		
		local v3Go = v3In
		
		if nInMag > 1.0 then -- FIXME: probably doesn't feel right with a joystick
			v3Go = v3Go / nInMag
		end
		
		local v3Old = v3Vel
		local nAdd = nAcc * wrp_TimeStep()
		v3Vel = v3Vel + v3Go * nAdd
		
		if nTop then
			
			local nClamp = math_max(vec_Mag3(v3Old), nTop)
			local nTmp = vec_Mag3(v3Vel)
			
			if nTmp > nClamp then
				
				local nMul = nClamp / nTmp
				v3Vel = v3Vel * nMul
				
			end
			
		end
		
	end
	
	return v3Vel
	
end

--[[------------------------------------
	phy.Glide

OUT	v3Vel

Uses velocity already available to go in v3In. Can only turn or decelerate, not accelerate.
--------------------------------------]]
function phy.Glide(v3Vel, v3In, nAcc)
	
	local nInMag = vec_Mag3(v3In)
	
	if nInMag > 0.0 then
		
		-- Accelerate
		local v3Go = v3In / nInMag
		local nInFac = math_min(nInMag, 1.0)
		local nAdd = nAcc * nInFac * wrp_TimeStep()
		local v3Add = v3Go * nAdd
		local v3NewVel = v3Vel + v3Add
		
		-- Get speed limit
		local nClamp = vec_Mag3(v3Vel)
		local nDot = vec_Dot3(v3Add, v3Vel)
		
		if nDot < 0.0 then
			
			-- Decelerated
			local nNewDot = vec.Dot3(v3Add, v3NewVel)
			
			if nNewDot > 0.0 then
				
				-- Then accelerated; use lowest speed dip as new clamp
				local nLerp = nDot / (nDot - nNewDot)
				nClamp = math_min(nClamp, vec_Mag3(v3Vel + v3Add * nLerp))
				
			end
			
		end
		
		-- Clamp
		local nTmp = vec_Mag3(v3NewVel)
		
		if nTmp > nClamp then
			
			local nMul = nClamp / nTmp
			v3NewVel = v3NewVel * nMul
			
		end
		
		return v3NewVel
		
	end
	
	return v3Vel
	
end

--[[------------------------------------
	phy.MagicFly

OUT	v3Vel

Flight that can turn faster than it can change speed.
--------------------------------------]]
function phy.MagicFly(v3Vel, v3In, nAcc, nTop, nDelta, nMinTurn)
	
	local nInMag = vec_Mag3(v3In)
	
	if nInMag > 0.0 then
		
		local nStep = wrp_TimeStep()
		local v3Go = v3In / nInMag
		local nVel = vec_Mag3(v3Vel)
		local nDot = vec_Dot3(v3Go, v3Vel)
		local nAdd = nAcc * nStep
		local nLimit = nDelta * nStep
		
		-- Limit acceleration in original velocity direction to add resistance to small turns
		if nDot > 0.0 then
			nAdd = math_min(nAdd, math_max(nMinTurn * nStep, nLimit) * nVel / nDot)
		end
		
		v3Vel = v3Vel + v3Go * nAdd
		
		-- Multiply resulting velocity to prevent changing speed by more than nLimit overall
		local nHiClamp = nVel >= nTop and nVel or math_min(nVel + nLimit, nTop)
		local nLoClamp = nVel - nLimit
		local nTmp = vec_Mag3(v3Vel)
		
		if nTmp > nHiClamp then
			
			local nMul = nHiClamp / nTmp
			v3Vel = v3Vel * nMul
			
		elseif nTmp < nLoClamp then
			
			local nMul = nLoClamp / nTmp
			v3Vel = v3Vel * nMul
			
		end
		
	end
	
	return v3Vel
	
end

--[[------------------------------------
	phy.ThrustTurn

OUT	v3Vel
--------------------------------------]]
function phy.ThrustTurn(v3Vel, v3InDir, nAcc, nTurnBarrier, nStep)
	
	local v3Go, bErr = vec_Normalized3(v3InDir)
	
	if not bErr then
		
		nStep = nStep or wrp_TimeStep()
		local v3OldVel = v3Vel
		local nVelSq = vec_MagSq3(v3Vel)
		local nVel = math_sqrt(nVelSq)
		local nDot = vec_Dot3(v3Go, v3Vel)
		local nAdd = nAcc * nStep
		
		--[[
		if nDot < 0.0 and nVelSq > 0.0 then
			
			-- Trying to go backwards, mirror acceleration direction
			nDot = -nDot
			local nMul = 2.0 * nDot / nVelSq
			v3Go = v3Go + v3Vel * nMul
			
		end
		]]
		
		-- Clip acceleration vector with a line nTurnBarrier * nStep past original velocity
			-- Adds more turn resistance when accelerating in current direction
			-- FIXME: explain what nStep is doing here
		if nDot > 0.0 then
			nAdd = math_min(nTurnBarrier * nStep * nVel / nDot, nAdd)
		end
		
		v3Vel = v3Vel + v3Go * nAdd
		
		if nDot >= 0.0 then
			
			-- Undo speed change
			local bResizeErr
			v3Vel, bResizeErr = vec_Resized3(v3Vel, nVel)
			
			if bResizeErr then
				v3Vel = v3OldVel -- Just in case
			end
			
		end
		
	end
	
	return v3Vel
	
end

--[[------------------------------------
	phy.ThrustSpeed

OUT	v3Vel
--------------------------------------]]
function phy.ThrustSpeed(v3Vel, v3InDir, nAcc, nDragFac, nStep)
	
	local v3Go, bErr = vec_Normalized3(v3InDir)
	
	if not bErr then
		
		nStep = nStep or wrp_TimeStep()
		local nVel = vec_Mag3(v3Vel)
		local nAdjustedAcc = math.max(0.0, nAcc - nVel * nDragFac)
		local nDot = vec_Dot3(v3Go, v3Vel)
		
		if nVel == 0.0 then
			v3Vel = v3Go * nAdjustedAcc * nStep
		else
			
			nDot = nDot / nVel
			
			if nDot > 0.0 then
				
				nVel = nVel + nAdjustedAcc * nDot * nStep
				v3Vel = vec_Resized3(v3Vel, nVel)
				
			end
			
		end
		
	end
	
	return v3Vel
	
end

--[[------------------------------------
	phy.SprintThrust

OUT	v3Vel

... is a sequence of additional nAcc, nSpeedLimit pairs in ascending order of nSpeedLimit.

A nil nMinTurnAcc acts like it's infinity, no turn resistance.

FIXME: call SprintThrust recursively to make the transition across speed limits consistent no matter the nStep?
--------------------------------------]]
function phy.SprintThrust(v3Vel, nBaseClamp, v3InDir, nDotPow, nGainFrac, nMinTurnAcc, nStep, nMainAcc, nMainLimit, ...)
	
	local v3Go, bErr = vec_Normalized3(v3InDir)
	local nClamp
	
	if not bErr then
		
		local nVel = vec_Mag3(v3Vel)
		nBaseClamp = nBaseClamp or nVel
		local nAcc, nSpeedLimit = nMainAcc, nMainLimit
		local iNumVars, iCurVarIndex = select('#', ...), -1
		
		while nSpeedLimit and nVel >= nSpeedLimit do
			
			iCurVarIndex = iCurVarIndex + 2
			nAcc, nSpeedLimit = select(iCurVarIndex, ...)
			
		end
		
		if not nAcc then
			nAcc = 0.0
		end
		
		local nStep = nStep or wrp_TimeStep()
		local nDot = vec_Dot3(v3Go, v3Vel)
		local nAdd = math_max(nMainAcc, nAcc) * nStep
			-- Still use higher acceleration here so turning rate is not affected by lower nAcc
		
		if nDot > 0.0 then
			
			nClamp = nBaseClamp + nAcc * nStep * nGainFrac * (nDot / nVel) ^ nDotPow
			
			if nMinTurnAcc then
				-- Clip acceleration vector to add turn resistance
				nAdd = math_min(math_max(nClamp - nBaseClamp, nMinTurnAcc * nStep) * nVel / nDot, nAdd)
			end
			
		else
			nClamp = nBaseClamp + nAcc * nStep
		end
		
		local nTop
		
		if iNumVars > 0 then
			nTop = iNumVars % 2 == 0 and select(iNumVars, ...) or nil
		else
			nTop = nMainLimit
		end
		
		if nTop and nTop < nClamp then
			nClamp = nTop
		end
		
		v3Vel = v3Vel + v3Go * nAdd
		
	else
		nClamp = nBaseClamp
	end
	
	if nClamp then
		
		local nReq = vec_Mag3(v3Vel)
		
		if nReq > nClamp then
			
			local nMul = nClamp / nReq
			v3Vel = v3Vel * nMul
			
		end
		
	end
	
	return v3Vel
	
end

--[[------------------------------------
	phy:Impact

OUT	v3Vel, nAway

Default Impact function. Entities that want to have more realistic collisions should set an
nMass, have an Impact function (this or a func that calls this) and an ImpactVelocity function,
and call their Impact in their Move function when a HIT happens.
--------------------------------------]]
function phy:Impact(tMoveRes, v3Pos, v3ImpactVel, v3DefaultVel, v3Normal, nTime, entOther,
	v3OtherImpactVel)
	
	local nAway
	local v3Vel = v3DefaultVel
	
	if entOther and entOther.nMass then
		
		if not xOtherImpactVel then v3OtherImpactVel = entOther.v3Vel end
		
		-- entOther is influenceable
		if not tMoveRes or not entOther.bPushing then
			
			-- entOther requested a response or is not already part of this impact chain
			-- Calculate new velocity
			local nImpactAway = vec_Dot3(v3Normal, v3ImpactVel)
			local nOtherAway = vec_Dot3(v3Normal, v3OtherImpactVel)
			
				-- Detect fake impact (entOther moving away from self at higher speed)
			local bRealImpact = not tMoveRes or (nImpactAway >= 0.0) ~= (nOtherAway >= 0.0) or
				math_abs(nImpactAway) > math_abs(nOtherAway)
			
			local nMass, nOtherMass = self.nMass, entOther.nMass
			local nAccumMass = nOtherMass
			
			if bRealImpact then
				
				v3Vel, nAway = self:ImpactVelocity(v3ImpactVel, v3DefaultVel, nImpactAway,
					nMass, v3Normal, nTime, entOther, v3OtherImpactVel, nOtherAway, nOtherMass)
				
			else
				
				-- Cancel the physics hit completely
				v3Vel, nAway = v3ImpactVel, 0
				tMoveRes[1], tMoveRes[7] = hit_NONE, nil -- Move func has to watch out for this
				
			end
			
			if tMoveRes then
				
				-- self is moving rather than updating state cause of something hitting it
				-- Request entOther update its state
				if bRealImpact and entOther.Impact then
					
					local v3OtherPos = EntPos(entOther)
					
					entOther.v3Vel = entOther:Impact(nil, v3OtherPos, v3OtherImpactVel,
						v3OtherImpactVel, -v3Normal, nTime, self, v3ImpactVel)
					
				end
				
				local nVelDot = vec_Dot3(v3Normal, v3Vel)
				local v3OtherVel = entOther.v3Vel
				local nOtherVelDot = vec_Dot3(v3Normal, v3OtherVel)
				local nGapTime = nOtherVelDot ~= 0.0 and math_min(nVelDot / nOtherVelDot * nTime, nTime) or nil
					-- Necessary push time to create minimal gap
					-- FIXME: might need some bias here to prevent too small of a gap
				
				if nVelDot < 0.0 and nGapTime and nGapTime > 0.0 then
					
					-- entOther is in the way of new velocity and we still need to move
					-- Don't wait for it to move on its own time, push it now to make a gap
					entOther.nReverseMass = nil -- Reset entOther's hit-mass accumulation
					
					if bRealImpact then
						
						-- Let entOther borrow some of self's mass
						-- FIXME: make more efficient, already have dots, just divide by mags
						local v3VelDir = vec_Normalized3(v3Vel)
						local v3OtherVelDir = vec_Normalized3(v3OtherVel)
						
						entOther.nMass = nOtherMass + nMass *
							math_max(0, -vec_Dot3(v3Normal, v3OtherVelDir)) *
							math_max(0, -vec_Dot3(v3Normal, v3VelDir))
						
					end
					
					local bExec
					v3OtherVel, bExec = phy_Push(entOther, v3OtherVel, nGapTime, nil)
					entOther.v3Vel = v3OtherVel
					
					if bRealImpact then
						
						entOther.nMass = nOtherMass -- Stop borrowing mass
						
						-- Check for more hit mass
						local nReverseMass = entOther.nReverseMass
						
						if not bExec then
							-- Reached Push limit, act like entOther hit a static
							--[[ This breaks from the expected result but prevents a lot of
							Impact calls in a single tick ]]
							nReverseMass = math_huge
						end
						
						if nReverseMass then 
							
							-- entOther hit more mass, revise self's response
							nAccumMass = nOtherMass + nReverseMass
							
							v3Vel, nAway = self:ImpactVelocity(v3ImpactVel, v3DefaultVel,
								nImpactAway, nMass, v3Normal, nTime, entOther, v3OtherImpactVel,
								nOtherAway, nAccumMass)
							
						end
						
					end
					
				end
				
				if bRealImpact then
					self.nReverseMass = (self.nReverseMass or 0.0) + nAccumMass
				end
				
			end
			
		else -- entOther is already part of impact chain
			v3Vel, nAway = v3ImpactVel, 0 -- Ignore the hit, keep the same velocity
		end
		
	else
		
		-- Hit something static
		v3Vel, nAway = self:ImpactVelocity(v3ImpactVel, v3DefaultVel,
			vec.Dot3(v3Normal, v3ImpactVel), self.nMass, v3Normal, nTime)
		
		self.nReverseMass = math_huge
		
	end
	
	--self:Hurt(nil, math.max(0, -nAway) * 0.5, 0, 0, 0, nil)
		-- FIXME: Call this in box:Impact, which also calls phy.Impact beforehand
	
	return v3Vel, nAway
	
end

--[[------------------------------------
	phy:ImpactVelocity

OUT	v3Vel, nAway
--------------------------------------]]
function phy:ImpactVelocity(v3ImpactVel, v3DefaultVel, nImpactAway, nMass, v3Normal, nTime,
	entOther, v3OtherImpactVel, nOtherAway, nOtherMass)
	
	local v3Vel, nAway
	
	if nOtherMass and nOtherMass < math_huge then
		
		nAway = nImpactAway -
			(nMass * nImpactAway + nOtherMass * nOtherAway) / (nMass + nOtherMass)
		
		v3Vel = v3ImpactVel - v3Normal * nAway -- FIXME: might need some bias here to prevent continual scraping
		
	else
		
		nAway = nImpactAway
		v3Vel = v3DefaultVel
		
	end
	
	return v3Vel, nAway
	
end

--[[------------------------------------
	phy:ImpactVelocityBounce

OUT	v3Vel, nAway
--------------------------------------]]
function phy:ImpactVelocityBounce(v3ImpactVel, v3DefaultVel, nImpactAway, nMass, v3Normal,
	nTime, entOther, v3OtherImpactVel, nOtherAway, nOtherMass)
	
	local v3Vel, nAway = phy.ImpactVelocity(self, v3ImpactVel, v3DefaultVel, nImpactAway, nMass,
		v3Normal, nTime, entOther, v3OtherImpactVel, nOtherAway, nOtherMass)
	
	-- Bounce
	local nBounceFactor = self.nBounceFactor
	local bBounce = nBounceFactor and (not self.nBounceLimit or nAway < -self.nBounceLimit)
	
	if bBounce then
		
		local nBounce = -nAway * nBounceFactor
		local v3Bounce = v3Normal * nBounce
		v3Vel = v3Vel + v3Bounce
		
	end
	
	-- Center-of-mass friction
	local nFricFrac = self.nImpactFric
	
	if nFricFrac then
		
		local v3FricVel = v3ImpactVel
		
		if xOtherImpactVel then
			v3FricVel = v3FricVel - v3OtherImpactVel
		end
		
		local nDot = vec.Dot3(v3Normal, v3FricVel)
		local v3Proj = v3FricVel - v3Normal * nDot
		
		if not bBounce then
			nFricFrac = nFricFrac * nTime
		end
		
		v3Vel = v3Vel - v3Proj * nFricFrac
		
	end
	
	return v3Vel, nAway
	
end

return phy