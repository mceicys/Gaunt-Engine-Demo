-- rocket.lua
LFV_EXPAND_VECTORS()

local aud = gaud
local flg = gflg
local hit = ghit
local qua = gqua
local rnd = grnd
local scn = gscn
local scr = gscr
local vec = gvec
local wrp = gwrp

local com = scr.EnsureScript("common.lua")
local exp = scr.EnsureScript("explosion.lua")
local imp = scr.EnsureScript("impact.lua")
local phy = scr.EnsureScript("physics.lua")

local roc = {}
roc.__index = roc

roc.msh = rnd.EnsureMesh("tracer.msh")
roc.tex = rnd.EnsureTexture("tracer.tex")
roc.ani = roc.msh:Animation("stretch")
--roc.sndThrust = aud.EnsureSound("rocket.wav")
roc.hulTest = hit.HullBox(-2, -2, -2, 2, 2, 2)
roc.nStretchMove = 40.0
roc.fsetIgnore = flg.FlagSet():Or(I_ENT_TRIGGER)
roc.nWorldBound = 8192.0
roc.nOpacity = 1.0
roc.iRadarFrame = 2

local tRes = {}

--[[------------------------------------
	roc.CreateRocket
--------------------------------------]]
function roc.CreateRocket(v3Pos, q4Ori, entOwner, v3FireVel, v3ShooterVel, nRadius, nDmg, nFrc)
	
	-- FIXME: gradually change to velocity's orientation instead of instantly
	local v3Vel = v3FireVel + v3ShooterVel
	local q4Ori = qua.QuaPitchYaw(vec.PitchYaw(v3Vel, 0.0))
	
	local ent = scn.CreateEntity(roc.typ, v3Pos, q4Ori)
	local nFwdFire = math.max(0.0, vec.Dot3(v3FireVel, qua.Dir(q4Ori)))
	ent:SetAnimationSpeed(nFwdFire / ent.nStretchMove)
	ent:SetAnimation(ent.ani, 0, 0)
	ent.entOwner = entOwner
	ent.v3Vel = v3Vel
	ent.nRadius = nRadius
	ent.nDmg = nDmg
	ent.nFrc = nFrc
	
end

--[[------------------------------------
	roc:Init
--------------------------------------]]
function roc:Init()
	
	setmetatable(self:Table(), roc)
	self:SetMesh(self.msh)
	self:SetTexture(self.tex)
	self:SetShadow(false)
	self:SetRegularGlass(true)
	self:SetOpacity(self.nOpacity)
	self:SetOldOpacity(self.nOpacity)
	self:SetSubPalette(3)
	local v3Pos = self:Pos()
	self.blb = scn.CreateBulb(v3Pos, 200, 1.0, 1.5, 4)
	local voxThrust = aud.Voice(v3Pos, 1000, 0, 0.5)
	voxThrust:SetLogarithmic(true)
	voxThrust:SetLoop(true)
	--voxThrust:Play(self.sndThrust, 0)
	self.voxThrust = voxThrust
	
end

--[[------------------------------------
	roc:Term
--------------------------------------]]
function roc:Term()
	
	self.blb:Delete()
	self.voxThrust:Play(nil, 0)
	-- FIXME: should be able to delete voices explicitly
	
end

--[[------------------------------------
	roc:Tick
--------------------------------------]]
function roc:Tick()
	
	local nStep = wrp.TimeStep()
	local entOwner = self.entOwner
	local v3Pos = self:Pos()
	local v3New = v3Pos + self.v3Vel * nStep
	
	local iContact, nTF, nTL, v3Nrm, entHit = hit.HullTest(self.hulTest, v3Pos, v3New,
		0, 0, 0, 1, hit.ALL_ALT, self.fsetIgnore, entOwner)
	
	if iContact ~= hit.NONE then
		
		self:DoHit(v3Pos, v3New, iContact, nTF, nTL, v3Nrm, entHit)
		self:Delete()
		return
		
	end
	
	if com.OutsideWorld(v3New, roc.nWorldBound) then
		
		self:Delete()
		return
		
	end
	
	self:SetPos(v3New)
	self.blb:SetPos(v3New)
	self.voxThrust:SetPos(v3New)
	--self.zVel = self.zVel + phy.zGravity * nStep
	
end

--[[------------------------------------
	roc:DoHit

OUT	v3Hit
--------------------------------------]]
function roc:DoHit(v3Pos, v3New, iContact, nTF, nTL, v3Nrm, entHit)
	
	-- Direct hit damage
	local entHurt = entHit
	
	if entHit and entHurt.Hurt then
		entHurt:Hurt(self.entOwner, self.nDmg, self.v3Vel, entHit)
	end
	
	-- Explosion
	local v3Hit = hit.RespondStop(v3Pos, v3New, iContact, nTF, nTL, v3Nrm)
	exp.CreateExplosion(self.entOwner, entHit, v3Hit, self.nRadius, self.nDmg, self.nFrc, v3Nrm)
	
	return v3Hit
	
end

roc.typ = scn.CreateEntityType("rocket", roc.Init, roc.Tick, roc.Term, nil, nil)

return roc