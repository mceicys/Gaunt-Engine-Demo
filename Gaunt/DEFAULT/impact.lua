-- impact.lua
-- FIXME: looks like a growing flower
LFV_EXPAND_VECTORS()

local aud = gaud
local hit = ghit
local qua = gqua
local rnd = grnd
local scn = gscn
local scr = gscr
local vec = gvec
local wrp = gwrp

local com = scr.EnsureScript("common.lua")

local imp = {}
imp.__index = imp

imp.msh = rnd.EnsureMesh("dust.msh")
imp.tex = rnd.EnsureTexture("dust.tex")
imp.ani = imp.msh:Animation("expand")
imp.nOpacityFac = 0.35
imp.nOpacityMinDec = 0.05
imp.nMaxVel = 10.0
imp.nUp = 9.0

local tImpactSounds = {
	--[[
	aud.EnsureSound("copyright_impact0.wav"),
	aud.EnsureSound("copyright_impact1.wav"),
	aud.EnsureSound("copyright_impact2.wav"),
	aud.EnsureSound("copyright_impact3.wav"),
	aud.EnsureSound("copyright_impact4.wav"),
	aud.EnsureSound("copyright_impact5.wav"),
	aud.EnsureSound("copyright_impact6.wav"),
	aud.EnsureSound("copyright_impact7.wav"),
	aud.EnsureSound("copyright_impact8.wav"),
	aud.EnsureSound("copyright_impact9.wav"),
	aud.EnsureSound("copyright_impact10.wav"),
	
	-- FIXME TEMP: duplicating for weighting, but there should be no chance of repeating
	aud.EnsureSound("copyright_impact0.wav"),
	aud.EnsureSound("copyright_impact0.wav"),
	aud.EnsureSound("copyright_impact0.wav"),
	aud.EnsureSound("copyright_impact0.wav"),
	aud.EnsureSound("copyright_impact0.wav"),
	aud.EnsureSound("copyright_impact0.wav"),
	aud.EnsureSound("copyright_impact1.wav"),
	aud.EnsureSound("copyright_impact1.wav"),
	aud.EnsureSound("copyright_impact1.wav"),
	aud.EnsureSound("copyright_impact1.wav"),
	aud.EnsureSound("copyright_impact1.wav"),
	aud.EnsureSound("copyright_impact1.wav"),
	aud.EnsureSound("copyright_impact8.wav"),
	aud.EnsureSound("copyright_impact8.wav"),
	aud.EnsureSound("copyright_impact8.wav"),
	aud.EnsureSound("copyright_impact9.wav"),
	aud.EnsureSound("copyright_impact9.wav"),
	aud.EnsureSound("copyright_impact10.wav"),
	aud.EnsureSound("copyright_impact10.wav")
	--]]
}

local tHitSounds = {
	--[[
	aud.EnsureSound("copyright_hit0.wav"),
	aud.EnsureSound("copyright_hit1.wav"),
	aud.EnsureSound("copyright_hit2.wav"),
	aud.EnsureSound("copyright_hit3.wav"),
	aud.EnsureSound("copyright_hit4.wav"),
	aud.EnsureSound("copyright_hit5.wav"),
	aud.EnsureSound("copyright_hit6.wav"),
	aud.EnsureSound("copyright_hit7.wav"),
	aud.EnsureSound("copyright_hit8.wav"),
	aud.EnsureSound("copyright_hit9.wav")
	--]]
}

--[[------------------------------------
	imp.CreateImpact

OUT	entImpact
--------------------------------------]]
function imp.CreateImpact(v3Old, v3New, iC, nTF, nTL, v3N, entHit)
	
	local v3Hit, v3Dir
	
	if iC == hit.HIT then
		
		v3Hit = com.LerpVec(v3Old, v3New, nTF)
		v3Dir = v3New - v3Old
		local nRfl = vec.Dot3(v3Dir, v3N) * -imp.nUp
		v3Dir = vec.Normalized3(v3Dir + v3N * nRfl)
		
	else -- INTERSECT
		
		v3Hit = v3Old
		v3Dir = vec.Normalized3(v3Old - v3New)
		
	end
	
	-- FIXME: use custom engine random
	local q4Lok = qua.QuaLook(v3Dir)
	local q4Rol = qua.QuaAxisAngle(v3Dir, math.random() * math.pi * 2.0)
	local ent = scn.CreateEntity(imp.typ, v3Hit, qua.Product(q4Rol, q4Lok))
	local vox = ent.vox
	vox:SetLogarithmic(true)
	
	-- FIXME: need better way of determining entity's debris type (string?)
	if scn.EntityExists(entHit) and entHit.nHealth then
		
		ent:SetSubPalette(1) -- FIXME: blood particles + dust
		ent:SetOpacity(0.75)
		ent.nOpacityMinDec = 1.5 -- FIXME: should have a different metatable
		
		vox:SetRadius(800) -- Health impact noises go farther since they're important
		vox:FadeVolume(1.5, 0) -- FIXME TEMP: make actual sound file louder
		--vox:Play(tHitSounds[math.random(#tHitSounds)], 0)
		
	else
		
		-- FIXME: smarter global mixing that doesn't allow too many distinguishable sounds to play
		vox:SetRadius(400)
		vox:FadeVolume(1.0, 0) -- FIXME TEMP
		--vox:Play(tImpactSounds[math.random(#tImpactSounds)], 0)
		
	end
	
	return ent
	
end

--[[------------------------------------
	imp:Init
--------------------------------------]]
function imp:Init()
	
	setmetatable(self:Table(), imp)
	self:SetMesh(self.msh)
	self:SetTexture(self.tex)
	self:SetAnimation(self.ani, 0, 0)
	self.nVel = math.random(self.nMaxVel)
	local v3Pos = self:Pos()
	self.vox = aud.Voice(v3Pos, 1, 0.0, 1.0)
	
end

--[[------------------------------------
	imp:Tick
--------------------------------------]]
function imp:Tick()
	
	local nOpq = self:Opacity()
	
	if nOpq <= 0.0 then
		
		self:Delete()
		return
		
	end
	
	local nStep = wrp.TimeStep()
	local nDec = math.max(self.nOpacityMinDec * nStep, nOpq - nOpq * self.nOpacityFac ^ nStep) -- FIXME: change pow to a mul
	self:SetOpacity(nOpq - nDec)
	local v3Mov = qua.VecRot(self.nVel * nStep, 0, 0, self:Ori())
	local v3Pos = self:Pos()
	self:SetPos(v3Pos + v3Mov)
	
end

imp.typ = scn.CreateEntityType("impact", imp.Init, imp.Tick, nil, nil, nil)

return imp