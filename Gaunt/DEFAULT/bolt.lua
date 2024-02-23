-- bolt.lua
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
local imp = scr.EnsureScript("impact.lua")
local phy = scr.EnsureScript("physics.lua")

local bol = {}
bol.__index = bol

bol.msh = rnd.EnsureMesh("bolt.msh")
bol.tex = rnd.EnsureTexture("tracer.tex")
bol.hulTest = hit.HullBox(-1, -1, -1, 1, 1, 1)
bol.nStretchMove = 40.0
bol.fsetIgnore = flg.FlagSet():Or(I_ENT_SOFT, I_ENT_TRIGGER)
bol.nWorldBound = 8192.0
bol.nOpacity = 0.6
bol.iMirrorFrame = 1

bol.mshTrail = rnd.EnsureMesh("trail.msh")
bol.texTrail = rnd.EnsureTexture("trail.tex")
bol.aniTrail = bol.mshTrail:Animation("stretch")
bol.nTrailPartDist = 128.0
bol.nTrailTime = 1.0
bol.nTrailMaxOpacity = 0.35
bol.iMaxTrailParts = 16
bol.fsetTrailIgnore = flg.FlagSet():Or(I_ENT_SOFT, I_ENT_TRIGGER, I_ENT_MOBILE)

--[[------------------------------------
	bol.CreateBolt

OUT	entBolt
--------------------------------------]]
function bol.CreateBolt(v3Pos, q4Ori, entOwner, v3FireVel, v3ShooterVel, nDmg, entTarget,
	nFrcFac, nHomeRate, bShowTrail)
	
	local ent = scn.CreateEntity(bol.typ, v3Pos, q4Ori)
	ent.nFireVel = vec.Mag3(v3FireVel)
	ent.v3Vel = v3FireVel + v3ShooterVel
	ent.entOwner = entOwner
	ent.nDmg = nDmg
	ent.entTarget = entTarget
	ent.nFrcFac = nFrcFac
	ent.nHomeRate = nHomeRate
	ent.blb = scn.CreateBulb(v3Pos, 70, 1.0, 1.0, 2)
	
	if bShowTrail then
		
		local v3Vel = ent.v3Vel
		local nVel = vec.Mag3(v3Vel)
		local nTrailDist = math.min(nVel * ent.nTrailTime, ent.nTrailPartDist * ent.iMaxTrailParts - 0.1)
		local iNumParts = 0
		
		for nDist = 0.0, nTrailDist, ent.nTrailPartDist do
			
			iNumParts = iNumParts + 1
			local entPart = scn.CreateEntity(nil, v3Pos, 0, 0, 0, 1)
			ent[iNumParts] = entPart
			entPart:SetVisible(false)
			entPart:SetRegularGlass(true)
			entPart:SetShadow(false)
			entPart:SetSubPalette(2)
			entPart:SetMesh(ent.mshTrail)
			entPart:SetTexture(ent.texTrail)
			entPart:SetAnimationSpeed(0)
			entPart:SetOpacity(0)
			entPart:SetAnimation(ent.aniTrail, 1.0, 0)
			
		end
		
	end
	
	return ent
	
end

--[[------------------------------------
	bol:Init
--------------------------------------]]
function bol:Init()
	
	setmetatable(self:Table(), bol)
	self:SetMesh(self.msh)
	self:SetTexture(self.tex)
	self:SetShadow(false)
	self:SetRegularGlass(true)
	self:SetOpacity(self.nOpacity)
	self:SetOldOpacity(self.nOpacity)
	self:SetSubPalette(2)
	self:SetScale(4.0)
	
end

--[[------------------------------------
	bol:Term
--------------------------------------]]
function bol:Term()
	
	self.blb:Delete()
	
	-- Delete trail ents
	for i = 1, #self do
		self[i]:Delete()
	end
	
end

--[[------------------------------------
	bol:Tick
--------------------------------------]]
function bol:Tick()
	
	local nStep = wrp.TimeStep()
	local entOwner = self.entOwner
	local v3Pos = self:Pos()
	local q4Ori = self:Ori()
	local v3Vel = self.v3Vel
	local entTarget = self.entTarget
	
	if scn.EntityExists(entTarget) then
		
		local v3Target = entTarget:Pos()
		local v3TarVel = entTarget.v3Vel or 0.0
		
		--[[
		local v3Dif = v3Target - v3Pos
		local nVel = vec.Mag3(v3Vel)
		local nNewVel = com.Approach(nVel, 200.0, 100.0 * nStep)
		
		if nVel > 0.0 then
			
			if vec.Dot3(v3Dif, v3Vel) >= 0.0 then
				
				local nMul = nNewVel / nVel
				v3Vel = v3Vel * nMul
				local v3Axis, nAngle = vec.DirDelta(v3Vel, v3Dif)
				local q4Rot = qua.QuaAxisAngle(v3Axis, math.min(nAngle, self.nHomeRate * nStep))
				v3Vel = qua.VecRot(v3Vel, q4Rot)
				
			else
				self.entTarget = nil -- Lose lock-on
			end
			
		else
			v3Vel = vec.Resized3(v3Dif, nNewVel)
		end
		]]
		
		-- Rotate relative velocity to home in on target
		-- FIXME: remove
		local v3RelVel = v3Vel - v3TarVel
		local v3Dif = v3Target - v3Pos
		
		if vec.Dot3(v3RelVel, v3Dif) > 0.0 then
			
			local v3Axis, nAngle = vec.DirDelta(v3RelVel, v3Dif)
			local q4Rot = qua.QuaAxisAngle(v3Axis, math.min(nAngle, self.nHomeRate * nStep))
			local nRelVel = vec.Mag3(v3RelVel)
			local nModVel = self.nFireVel
			local v3Modifiable = v3RelVel
			local v3Const = 0, 0, 0
			
			--[[
			if nRelVel > nModVel then
				
				local nMul = nModVel / nRelVel
				v3Modifiable = v3Modifiable * nMul
				v3Const = v3RelVel - v3Modifiable
				
			end
			]]
				
			local v3Rotated = qua.VecRot(v3Modifiable, q4Rot)
			v3Vel = v3Rotated + v3Const + v3TarVel
			
		else
			self.entTarget = nil -- Lose lock-on
		end
		
	end
	
	-- Apply gravity once lock is lost so stationary bolts don't float forever
	local bLockOn = scn.EntityExists(self.entTarget)
	
	if not bLockOn then
		zVel = zVel + phy.zGravity * nStep
	end
	
	local v3New = v3Pos + v3Vel * nStep
	
	local iContact, nTF, nTL, v3Nrm, entHit = hit.HullTest(self.hulTest, v3Pos, v3New,
		0, 0, 0, 1, hit.ALL_ALT, self.fsetIgnore, entOwner)
	
	if iContact ~= hit.NONE then
		
		v3New = self:DoHit(v3Pos, v3New, iContact, nTF, nTL, v3Nrm, entHit)
		self:Delete()
		return
		
	end
	
	if com.OutsideWorld(v3New, bol.nWorldBound) then
		
		self:Delete()
		return
		
	end
	
	self.v3Vel = v3Vel
	self:SetPlace(v3New, q4Ori)
	self.blb:SetPos(v3New)
	
	-- Update trail
	local cam = scn.ActiveCamera()
	
	if self[1] and cam then
		
		-- Only show trail if bolt is nearly off screen
		local OFF_SCREEN_BUFFER = 0.2 -- In radians
		local nVFOV = cam:FOV()
		local nHFOV = com.HorizontalFOV(nVFOV, wrp.VideoWidth() / wrp.VideoHeight())
		local v3Cam = cam:Pos()
		local q4Cam = cam:Ori()
		local v3Rel = qua.VecRotInv(v3New - v3Cam, q4Cam)
		local nBoltPit, nBoltYaw = vec.PitchYaw(v3Rel, 0.0)
		
		local nOffScreenOpacity = math.max(
			0.0,
			(math.abs(nBoltPit) - nVFOV * 0.5) / OFF_SCREEN_BUFFER + 1.0,
			(math.abs(nBoltYaw) - nHFOV * 0.5) / OFF_SCREEN_BUFFER + 1.0
		)
		
		if nOffScreenOpacity > 1.0 then nOffScreenOpacity = 1.0 end
		
		if nOffScreenOpacity > 0.0 then
			
			-- Place trail parts
			local q4Go = qua.QuaPitchYaw(vec.PitchYaw(self.v3Vel, 0.0))
			local nVel = vec.Mag3(v3Vel)
			local v3Dir = v3Vel
			if nVel > 0.0 then v3Dir = v3Dir / nVel end
			local nTrailDist = nVel * self.nTrailTime
			
			iContact, nTF = hit.HullTest(self.hulTest, v3New, v3New + v3Dir * nTrailDist,
				0, 0, 0, 1, hit.ALL_ALT, self.fsetTrailIgnore, entOwner)
			
			if iContact == hit.NONE then
				nTF = 1.0
			elseif iContact == hit.INTERSECT then
				nTF = 0.0
			end
			
			local nActualDist = nTF * nTrailDist
			local iNumParts = #self
			local iCurPart = 0
			local nTrailPartDist = self.nTrailPartDist
			local aniTrail = self.aniTrail
			
			for nDist = 0.0, nActualDist, nTrailPartDist do
				
				iCurPart = iCurPart + 1
				
				if iCurPart > iNumParts then
					break -- Could create another part here if trail length can increase over time
				end
				
				local entPart = self[iCurPart]
				local nF = math.min((nActualDist - nDist) / nTrailPartDist, 1.0)
				entPart:SetAnimation(aniTrail, nF, nStep)
				local nOrigin = nDist + nTrailPartDist * 0.5 -- Mesh's origin is in center of trail part for better culling
				entPart:SetPlace(v3New + v3Dir * nOrigin, q4Go)
				
				-- Calculate opacity
				local nOpacityFactor = 1.0 - nDist / nTrailDist -- Parts closer to bolt are brighter
				nOpacityFactor = nOpacityFactor * nOpacityFactor * nOffScreenOpacity
				
					-- Fade out if close to camera
				local nEndDist = nDist + nTrailPartDist * nF
				local v3Start = v3New + v3Dir * nDist
				local v3End = v3New + v3Dir * nEndDist
				local nProjF = com.Clamp(com.ProjectPointLine(v3Start, v3End, v3Cam) or 0.0, 0.0, 1.0)
				local v3Proj = v3Start + (v3End - v3Start) * nProjF
				local nCamDist = vec.Mag3(v3Cam - v3Proj)
				nOpacityFactor = nOpacityFactor * com.Clamp((nCamDist - 2.0) / 6.0, 0.0, 1.0)
				
				entPart:SetOpacity(self.nTrailMaxOpacity * nOpacityFactor)
				entPart:SetVisible(nOpacityFactor > 0.0)
				
			end
			
			for i = iCurPart + 1, iNumParts do
				self[i]:SetVisible(false)
			end
			
		else -- nOffScreenOpacity == 0.0
			
			for i = 1, #self do
				self[i]:SetVisible(false)
			end
			
		end
		
	end
	
end

--[[------------------------------------
	bol:DoHit

OUT	v3Hit
--------------------------------------]]
function bol:DoHit(v3Pos, v3New, iContact, nTF, nTL, v3Nrm, entHit)
	
	local entHurt = entHit
	local v3Vel = self.v3Vel
	
	if entHit then
		
		if phy.Enabled(entHurt) then
			entHurt.v3Vel = entHurt.v3Vel + v3Vel * self.nFrcFac
		end
		
		if entHurt.Hurt then
			entHurt:Hurt(self.entOwner, self.nDmg, v3Vel, entHit)
		end
		
	end
	
	local v3Hit = hit.RespondStop(v3Pos, v3New, iContact, nTF, nTL, v3Nrm)
	
	-- FIXME: CreateImpact should take position and direction, not collision results
	imp.CreateImpact(v3Pos, v3New, iContact, nTF, nTL, v3Nrm, entHurt)
	return v3Hit
	
end

bol.typ = scn.CreateEntityType("bolt", bol.Init, bol.Tick, bol.Term, nil, nil)

return bol