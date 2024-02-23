-- explosion.lua
LFV_EXPAND_VECTORS()

local hit = ghit
local qua = gqua
local rnd = grnd
local scn = gscn
local scr = gscr
local vec = gvec
local wrp = gwrp

local com = scr.EnsureScript("common.lua")
local phy = scr.EnsureScript("physics.lua")

local BlbDelete = scn.BlbDelete
local BlbSetColorPriority = scn.BlbSetColorPriority
local BlbSetColorSmooth = scn.BlbSetColorSmooth
local BlbSetIntensity = scn.BlbSetIntensity
local BlbSetPos = scn.BlbSetPos
local com_Clamp = com.Clamp
local com_FreeTempTable = com.FreeTempTable
local com_TempTable = com.TempTable
local EntDelete = scn.EntDelete
local EntOri = scn.EntOri
local EntPos = scn.EntPos
local EntScale = scn.EntScale
local EntSetAnimation = scn.EntSetAnimation
local EntSetAnimationLoop = scn.EntSetAnimationLoop
local EntSetAnimationSpeed = scn.EntSetAnimationSpeed
local EntSetRegularGlass = scn.EntSetRegularGlass
local EntSetMesh = scn.EntSetMesh
local EntSetOldOpacity = scn.EntSetOldOpacity
local EntSetOldScale = scn.EntSetOldScale
local EntSetOpacity = scn.EntSetOpacity
local EntSetPos = scn.EntSetPos
local EntSetScale = scn.EntSetScale
local EntSetScaledPlace = scn.EntSetScaledPlace
local EntSetShadow = scn.EntSetShadow
local EntSetSubPalette = scn.EntSetSubPalette
local EntSetTexture = scn.EntSetTexture
local EntSetUV = scn.EntSetUV
local EntSetVisible = scn.EntSetVisible
local EntTable = scn.EntTable
local EntUV = scn.EntUV
local hit_ALL = hit.ALL
local hit_LineTest = hit.LineTest
local hit_NONE = hit.NONE
local hit_SphereEntities = hit.SphereEntities
local math_max = math.max
local math_min = math.min
local math_pi = math.pi
local math_pi_2 = math_pi * 2.0
local math_random = math.random
local phy_Enabled = phy.Enabled
local qua_Dir = qua.Dir
local qua_QuaEuler = qua.QuaEuler
local scn_CreateBulb = scn.CreateBulb
local scn_CreateEntity = scn.CreateEntity
local scn_EntityExists = scn.EntityExists
local vec_Dot3 = vec.Dot3
local vec_Mag3 = vec.Mag3
local vec_Normalized3 = vec.Normalized3
local vec_PitchYaw = vec.PitchYaw

local exp = {}
exp.__index = exp

local EXPLOSION_SCALE = 1.0 -- Makes all explosion visuals bigger/smaller
local EXPLOSION_TIME_SCALE = 1.0
local INV_TIME_SCALE = 1.0 / EXPLOSION_TIME_SCALE
local FLASH_SCALE = 0.4

exp.texSmoke = rnd.EnsureTexture("smoke.tex")
exp.mshFire = rnd.EnsureMesh("fire.msh")
exp.texFire = rnd.EnsureTexture("fire_additive.tex")
exp.aniFire = exp.mshFire:Animation("spin")
exp.mshFlash = rnd.EnsureMesh("flash.msh")
exp.texFlash = rnd.EnsureTexture("flash.tex")
--exp.mshFlashIn = rnd.EnsureMesh("flashin.msh")
exp.fsetIgnore = nil
exp.nPartMinTime, exp.nPartExtendTime = 2.0 * EXPLOSION_TIME_SCALE, 2.0 * EXPLOSION_TIME_SCALE
exp.nBulbInitTime, exp.nBulbMaxTime = 0.1 * EXPLOSION_TIME_SCALE, 2.2 * EXPLOSION_TIME_SCALE
exp.nBulbMaxIntensity = 3.0

-- These vars are multiplied by nRadius
exp.nInitVelFac, exp.nMinVelFac = 2.0 * EXPLOSION_SCALE * INV_TIME_SCALE, 0.3 * EXPLOSION_SCALE * INV_TIME_SCALE
exp.nVelDecFac = 13.0 * EXPLOSION_SCALE * INV_TIME_SCALE
exp.nStartScaleFac = 0.05 * EXPLOSION_SCALE
exp.nInitScaleVelFac, exp.nMinScaleVelFac = 1.5 * EXPLOSION_SCALE * INV_TIME_SCALE, 0.04 * EXPLOSION_SCALE * INV_TIME_SCALE
exp.nScaleVelDecFac = 22.5 * EXPLOSION_SCALE * INV_TIME_SCALE
exp.nAttachDistFac = 0.07 * EXPLOSION_SCALE

--[[------------------------------------
	exp.CreateExplosion

FIXME FIXME: low framerates make noticeably smaller explosions
FIXME: bigger explosions should be slower
FIXME: chain particles and link once with a max-visual-radius box hull

v3Normal is used to make particles move away from a blasted surface and is optional.
--------------------------------------]]
function exp.CreateExplosion(entOwner, entIgnore, v3Pos, nRadius, nDmg, nFrc, v3Normal)
	
	local ent = scn_CreateEntity(exp.typ, v3Pos, 0, 0, 0, 1)
	ent:SetPointLink(true) -- So explosions can be found thru descents
	local fsetIgnore = ent.fsetIgnore
	ent.nRadius = nRadius
	ent.nExistTime = 0.0
	
	if nDmg > 0.0 and nFrc > 0.0 then
		
		-- Blow up
		local tHits = com_TempTable()
		local iNumHits = hit_SphereEntities(tHits, v3Pos, nRadius, fsetIgnore)
		
		for i = 1, iNumHits do
			
			local entVictim = tHits[i]
			
			if entVictim == ent or entVictim == entIgnore or not scn_EntityExists(entVictim) then
				goto continue
			end
			
			local Hurt = entVictim.Hurt
			local bPhy = phy_Enabled(entVictim)
			
			if not Hurt and not bPhy then
				goto continue
			end
			
			local v3Vic = EntPos(entVictim)
			local iC, _, _, _, _, _, entHit = hit_LineTest(v3Pos, v3Vic, hit_ALL, fsetIgnore, nil)
			
			if iC ~= hit_NONE and entHit ~= entVictim then
				goto continue
			end
			
			local v3Dif = v3Vic - v3Pos
			local nDist = vec_Mag3(v3Dif)
			local nAttn = (nRadius - nDist) / nRadius
			
			if bPhy and nDist ~= 0.0 then
				
				local nMul = nFrc * nAttn / nDist
				entVictim.v3Vel = entVictim.v3Vel + v3Dif * nMul
				
			end
			
			if Hurt and nDmg > 0 then
				Hurt(entVictim, entOwner, nDmg * nAttn, v3Dif, entVictim)
			end
			
			::continue::
			
		end
		
		com_FreeTempTable(tHits)
		
	end
	
	-- Create visual
	
	--[[ FIXME: engine should not create a table for each entity until it's written to so
		garbage collector doesn't get mad here ]]
	
	local CreateParticle = ent.CreateParticle
	CreateParticle(ent, 0.0, v3Normal)
	
	for i = 1, 9 do
		CreateParticle(ent, i / 9, v3Normal)
	end
	
	local entFlash = scn_CreateEntity(nil, v3Pos, qua_QuaEuler(math_random() * math_pi_2, 0.0, 0.0))
	ent.entFlash = entFlash
	EntSetScale(entFlash, nRadius * FLASH_SCALE)
	EntSetMesh(entFlash, ent.mshFlash)
	EntSetTexture(entFlash, ent.texFlash)
	EntSetRegularGlass(entFlash, true)
	EntSetShadow(entFlash, false)
	EntSetOpacity(entFlash, 0.0)
	EntSetOldOpacity(entFlash, 0.0)
	
	--[[
	local entFlashIn = scn_CreateEntity(nil, 0, 0, 0, 0, 0, 0, 1)
	ent.entFlashIn = entFlashIn
	entFlashIn:SetPlace(entFlash:Place())
	entFlashIn:SetScale(nRadius)
	EntSetMesh(entFlashIn, ent.mshFlashIn)
	EntSetTexture(entFlashIn, ent.texFlash)
	EntSetRegularGlass(entFlashIn, true)
	EntSetShadow(entFlashIn, false)
	EntSetOpacity(entFlashIn, 0.0)
	EntSetOldOpacity(entFlashIn, 0.0)
	]]
	
	local blb = scn_CreateBulb(v3Pos, nRadius, 1.0, 0.0, 1)
	ent.blb = blb
	
end

--[[------------------------------------
	exp:CreateParticle

OUT	v3Dir
--------------------------------------]]
function exp:CreateParticle(nRank, v3Normal)
	
	local v3Pos = EntPos(self)
	local mshFire = self.mshFire
	local aniFire = self.aniFire
	local nRadius = self.nRadius
	local nInitVel = nRadius * self.nInitVelFac
	local nStartScale = nRadius * self.nStartScaleFac
	local nInitScaleVel = nRadius * self.nInitScaleVelFac
	
	local v3Dir = math_random() * 2.0 - 1.0, math_random() * 2.0 - 1.0, math_random() * 2.0 - 1.0
	local bErr
	v3Dir, bErr = vec_Normalized3(v3Dir)
	
	if bErr then
		v3Dir = 0, 0, 1
	end
	
	if xNormal then
		
		local nDot = vec_Dot3(v3Dir, v3Normal)
		
		if nDot < 0.0 then
			
			local nBounce = -nDot * 2.0
			v3Dir = v3Dir + v3Normal * nBounce
			
		end
		
	end
	
	-- FIXME: base orientation partially on movement direction
	local q4Ori = qua_QuaEuler(math_random() * math_pi, math_random() * math_pi_2,
		math_random() * math_pi_2) -- FIXME: more efficient random quaternion
	
	local entSmoke = scn_CreateEntity(nil, v3Pos, q4Ori)
	EntSetOldScale(entSmoke, nStartScale)
	EntSetScale(entSmoke, nStartScale)
	EntSetMesh(entSmoke, mshFire)
	EntSetTexture(entSmoke, self.texSmoke)
	EntSetUV(entSmoke, math_random(), 0.0)
	EntSetAnimationLoop(entSmoke, true)
	EntSetAnimationSpeed(entSmoke, 0.6)
	local nAnimOffset = math_random() * 20.0
	EntSetAnimation(entSmoke, aniFire, nAnimOffset, 0)
	
	local entFire = scn_CreateEntity(nil, v3Pos, q4Ori)
	EntSetOldScale(entFire, nStartScale)
	EntSetScale(entFire, nStartScale)
	EntSetMesh(entFire, mshFire)
	EntSetTexture(entFire, self.texFire)
	EntSetUV(entFire, math_random(), 0.0)
	EntSetAnimationLoop(entFire, true)
	EntSetAnimationSpeed(entFire, 0.6)
	EntSetAnimation(entFire, aniFire, nAnimOffset, 0)
	EntSetShadow(entFire, false)
	EntSetRegularGlass(entFire, true)
	EntSetOpacity(entFire, 0.5) -- Lowered to account for additive flash FIXME: but flash must be drawn even when player is close, then!
	EntSetSubPalette(entFire, 1)
	
	local iStart = #self + 1
	self[iStart] = entSmoke -- Smoke entity reference
	self[iStart + 1] = entFire -- Fire entity reference
	self[iStart + 2] = nRank -- Rank
	local nVel = nInitVel * nRank
	self[iStart + 3], self[iStart + 4], self[iStart + 5] = v3Dir * nVel -- Velocity
	self[iStart + 6] = nInitScaleVel -- Scale velocity
	
	return v3Dir
	
end

--[[------------------------------------
	exp:Init
--------------------------------------]]
function exp:Init()
	
	setmetatable(EntTable(self), exp)
	
end

--[[------------------------------------
	exp:Tick
--------------------------------------]]
function exp:Tick()
	
	local nStep = wrp.TimeStep()
	local nRadius = self.nRadius
	local nPartMinTime, nPartExtendTime = self.nPartMinTime, self.nPartExtendTime
	local nMinVel = nRadius * self.nMinVelFac
	local nVelDec = nRadius * self.nVelDecFac
	local nMinScaleVel = nRadius * self.nMinScaleVelFac
	local nScaleVelDec = nRadius * self.nScaleVelDecFac
	local nAttachDist = nRadius * self.nAttachDistFac
	local nExistTime = self.nExistTime + nStep
	self.nExistTime = nExistTime
	
	local iNumDeleted = 0
	local iSelfLength = #self
	local iParticle = 0
	local v3Accum = 0.0, 0.0, 0.0
	local nAccumWeight = 0.0
	
	for iStart = 1, iSelfLength, 7 do -- 7 vars per particle
		
		iParticle = iParticle + 1
		local entSmoke = self[iStart]
		
		if not entSmoke then
			
			iNumDeleted = iNumDeleted + 1
			goto continue
			
		end
		
		local entFire = self[iStart + 1]
		local nRank = self[iStart + 2]
		local nRevRank = 1.0 - nRank
		local nDeleteTime = nPartMinTime + nPartExtendTime * nRevRank
		
		if nExistTime >= nDeleteTime then
			
			EntDelete(entSmoke)
			EntDelete(entFire)
			self[iStart], self[iStart + 1] = false, false
			iNumDeleted = iNumDeleted + 1
			goto continue
			
		end
		
		-- Update smoke particle
		local nSmokeStopTime = nDeleteTime
		local nSmokeLife = math_min(1.0 - (nSmokeStopTime - nExistTime) / nSmokeStopTime, 1.0)
		local xUV = EntUV(entSmoke)
		EntSetUV(entSmoke, xUV, -nSmokeLife * 0.75)
		local v3Pos = EntPos(entSmoke)
		local v3Vel = self[iStart + 3], self[iStart + 4], self[iStart + 5]
		local nVel = vec_Mag3(v3Vel)
		
		if nVel > 0.0 then
			
			local nNewVel = math_max(nVel - nVelDec * nStep, nMinVel * nRank)
			local nMul = nNewVel / nVel
			v3Vel = v3Vel * nMul
			self[iStart + 3], self[iStart + 4], self[iStart + 5] = v3Vel
			
		end
		
		v3Pos = v3Pos + v3Vel * nStep
		local nScaleVel = self[iStart + 6]
		nScaleVel = math_max(nScaleVel - nScaleVelDec * nStep, nMinScaleVel * nRevRank)
		self[iStart + 6] = nScaleVel
		local nNewScale = EntScale(entSmoke) + nScaleVel * nStep
		
		if iParticle >= 7 then
			
			-- Attach faster particles to slower neighbors to prevent splitting from group
			-- FIXME: engine function to find closest entity from a list?
			local nMinDist, v3CloseDif, nCloseDist
			
			for iComp = 1, iStart - 1, 7 do
				
				local entOther = self[iComp]
				
				if not entOther then
					goto other_continue
				end
				
				local v3Other = EntPos(entOther)
				local v3Dif = v3Other - v3Pos
				local nDist = vec_Mag3(v3Dif) + nAttachDist
				local nTouchDist = nDist - EntScale(entOther) - nNewScale
				
				if nTouchDist <= 0.0 then
					
					nMinDist = false
					break
					
				end
				
				if not nMinDist or nTouchDist < nMinDist then
					nMinDist, v3CloseDif, nCloseDist = nTouchDist, v3Dif, nDist
				end
				
				::other_continue::
				
			end
			
			if nMinDist and nCloseDist > 0.0 then
				
				local nMul = nMinDist / nCloseDist
				v3Pos = v3Pos + v3CloseDif * nMul
				
			end
			
		end
		
		local q4Ori = EntOri(entSmoke)
		EntSetScaledPlace(entSmoke, v3Pos, q4Ori, nNewScale)
		
		-- Update fire particle
		local nFireStopTime = nDeleteTime * 0.7
		local nFireLife = 1.0 - (nFireStopTime - nExistTime) / nFireStopTime
		
		if nFireLife <= 1.0 then
		
			xUV = EntUV(entFire)
			EntSetUV(entFire, xUV, -nFireLife * 0.75)
			EntSetScaledPlace(entFire, v3Pos, q4Ori, nNewScale)
			-- FIXME: if any GPU causes z-fighting between smoke and fire, multiply scaling here or add fire via brights in the texture
			
		else
			EntSetVisible(entFire, false)
		end
		
		local nWeight = 1.0 - nSmokeLife
		v3Accum = v3Accum + v3Pos * nWeight
		nAccumWeight = nAccumWeight + nWeight
		::continue::
		
	end
	
	local iNumParticles = iSelfLength / 7
	local iRemaining = iNumParticles - iNumDeleted
	
	if iRemaining <= 0 then
		
		BlbDelete(self.blb)
		EntDelete(self.entFlash)
		--EntDelete(self.entFlashIn)
		EntDelete(self)
		return
		
	end
	
	-- Update flash
	-- FIXME: do this per-frame to support constant tick rate
	local camActive = scn.ActiveCamera()
	
	if camActive then
		
		local entFlash = self.entFlash
		--local entFlashIn = self.entFlashIn
		local v3Flash = EntPos(entFlash)
		local q4Flash = EntOri(entFlash)
		local v3Cam = camActive:Pos()
		local v3Dif = v3Cam - v3Flash
		local q4Delta = qua.LookDelta(q4Flash, v3Dif)
		local q4FlashOri = qua.Product(q4Delta, q4Flash)
		entFlash:SetOri(q4FlashOri)
		--entFlashIn:SetOri(q4FlashOri)
		
		local nFlashInitTime = nPartMinTime * 0.05
		local nFlashMaxTime = nPartMinTime
		local nDist = vec_Mag3(v3Dif)
		
		local nTimeFade = math_min(nExistTime / nFlashInitTime, 1.0) * -- Fade in over time
			(1.0 - math_min(nExistTime / nFlashMaxTime, 1.0)) -- Fade out over time
		
		--[[ FIXME: Make explosion look glowy when close to it but prevent clipping thru glow mesh
			Tried flashing screen with additive, exposure, doesn't look right and explosion doesn't look like it's glowing
				This might be part of the effect, but it doesn't sell it by itself
			Tried inside-out flash mesh that only appears when regular mesh fades out
				Still doesn't look glowy because it's behind the explosion particles
				Also likely blocked by geometry behind the explosion
				Also rapidly changing orientation as player flies thru explosion, like glow is getting out of the way
			Need GUI glass + exposure trick but only when looking AT explosion
		]]
		
		local nFlashRadius = nRadius * FLASH_SCALE
		local nProximityFade = com_Clamp((nDist - nFlashRadius) / (nFlashRadius * 0.25), 0.0, 1.0)
			-- Proximity fade out
		
		EntSetOpacity(entFlash, nTimeFade * nProximityFade)
		--EntSetOpacity(entFlashIn, nTimeFade * (1.0 - nProximityFade))
		--rnd.SetExposure(rnd.Exposure() + (nTimeFade * (1.0 - nProximityFade)))
		
	end
	
	-- Update bulb
	local blb = self.blb
	
	if nAccumWeight > 0.0 then
		BlbSetPos(blb, v3Accum / nAccumWeight)
	end
	
	BlbSetIntensity(blb, self.nBulbMaxIntensity *
		math_min(nExistTime / self.nBulbInitTime, 1.0) * -- Fade in over time
		(1.0 - math_min(nExistTime / self.nBulbMaxTime, 1.0)) -- Fade out over time
	)
	
	-- FIXME TEMP
	--[[
	local v3Pos = self:Pos()
	rnd.DrawSphereLines(v3Pos, self.nRadius, 7, nStep + 0.01)
	--]]
	
end

exp.typ = scn.CreateEntityType("explosion", exp.Init, exp.Tick, nil, nil, nil)
return exp