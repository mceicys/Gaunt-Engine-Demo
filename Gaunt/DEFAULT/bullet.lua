-- bullet.lua
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

local bul = {}
bul.__index = bul

bul.msh = rnd.EnsureMesh("tracer.msh")
bul.tex = rnd.EnsureTexture("tracer.tex")
bul.ani = bul.msh:Animation("stretch")
bul.hulTest = hit.HullBox(-1, -1, -1, 1, 1, 1)
bul.nStretchMove = 40.0
bul.fsetIgnore = flg.FlagSet():Or(I_ENT_TRIGGER)
bul.nWorldBound = 8192.0
bul.nOpacity = 1.0
bul.mshTrail = rnd.EnsureMesh("trail.msh")
bul.texTrail = rnd.EnsureTexture("trail.tex")
bul.aniTrail = bul.mshTrail:Animation("stretch")
bul.nTrailMaxDist = 4096.0
bul.nTrailMaxOpacity = 0.5
bul.fsetTrailIgnore = flg.FlagSet():Or(I_ENT_TRIGGER, I_ENT_MOBILE)

--[[------------------------------------
	bul.CreateBullet

OUT	entBullet
--------------------------------------]]
function bul.CreateBullet(v3Pos, q4Ori, entOwner, v3FireVel, v3ShooterVel, nDmg, nFrcFac,
	nSideFric, bShowTrail)
	
	--[[ -- FIXME: gradually change to velocity's orientation?
	local v3Vel = v3FireVel + v3ShooterVel
	local q4Ori = qua.QuaPitchYaw(vec.PitchYaw(v3Vel, 0.0))
	--]]
	
	local ent = scn.CreateEntity(bul.typ, v3Pos, q4Ori)
	local nFwdFire = math.max(0.0, vec.Dot3(v3FireVel, qua.Dir(q4Ori)))
	ent:SetAnimationSpeed(nFwdFire / ent.nStretchMove)
	ent:SetAnimation(ent.ani, 0, 0)
	ent.entOwner = entOwner
	ent.v3Vel = v3FireVel + v3ShooterVel
	ent.nDmg = nDmg
	ent.nFrcFac = nFrcFac
	ent.nSideFric = nSideFric
	
	if bShowTrail then
		
		local q4Go = qua.QuaPitchYaw(vec.PitchYaw(ent.v3Vel, 0.0))
		local entTrail = scn.CreateEntity(nil, v3Pos, q4Go)
		ent.entTrail = entTrail
		entTrail:SetRegularGlass(true)
		entTrail:SetShadow(false)
		entTrail:SetSubPalette(1)
		entTrail:SetMesh(ent.mshTrail)
		entTrail:SetTexture(ent.texTrail)
		entTrail:SetAnimationSpeed(0)
		entTrail:SetOpacity(ent.nTrailMaxOpacity)
		
		local v3Dir = vec.Normalized3(ent.v3Vel)
		
		local iContact, nTF = hit.HullTest(ent.hulTest, v3Pos, v3Pos + v3Dir * ent.nTrailMaxDist,
			0, 0, 0, 1, hit.ALL_ALT, ent.fsetTrailIgnore, entOwner)
		
		entTrail:SetAnimation(ent.aniTrail, iContact == hit.HIT and nTF or 1.0, 0)
		
	end
	
	return ent
	
end

--[[------------------------------------
	bul:Init
--------------------------------------]]
function bul:Init()
	
	setmetatable(self:Table(), bul)
	self:SetMesh(self.msh)
	self:SetTexture(self.tex)
	self:SetShadow(false)
	self:SetRegularGlass(true)
	self:SetOpacity(self.nOpacity)
	self:SetOldOpacity(self.nOpacity)
	self:SetSubPalette(1)
	
end

--[[------------------------------------
	bul:Tick
--------------------------------------]]
function bul:Tick()
	
	local nStep = wrp.TimeStep()
	local entTrail = self.entTrail
	
	if not self:Visible() then
		
		-- Bullet is gone, fade trail out and delete
		if entTrail then
			
			entTrail:SetOpacity(math.max(0.0, entTrail:Opacity() - 3.0 * nStep))
			
			if entTrail:Opacity() <= 0.0 then
				
				entTrail:Delete()
				entTrail, self.entTrail = nil
				
			end
			
		end
		
		if not entTrail then
			self:Delete()
		end
		
		return
		
	end
	
	-- Bullet is still going, move it
	local entOwner = self.entOwner
	local v3Pos = self:Pos()
	local v3Vel = self.v3Vel
	local v3New = v3Pos + v3Vel * nStep
	
	local iContact, nTF, nTL, v3Nrm, entHit = hit.HullTest(self.hulTest, v3Pos, v3New,
		0, 0, 0, 1, hit.ALL_ALT, self.fsetIgnore, entOwner)
	
	if iContact ~= hit.NONE then
		
		v3New = self:DoHit(v3Pos, v3New, iContact, nTF, nTL, v3Nrm, entHit)
		self:SetVisible(false)
		return
		
	end
	
	if com.OutsideWorld(v3New, bul.nWorldBound) then
		
		self:SetVisible(false)
		return
		
	end
	
	self:SetPos(v3New)
	
	-- FIXME: add gravity if going slow
	--zVel = zVel + phy.zGravity * nStep
	
	-- Sideways drag
	if self.nSideFric > 0.0 then
		
		local v3Dir = qua.Dir(self:Ori())
		local nDot = vec.Dot3(v3Dir, v3Vel)
		local v3FwdVel = v3Dir * nDot
		local v3SideVel = v3Vel - v3FwdVel
		v3SideVel = phy.Friction(v3SideVel, self.nSideFric)
		v3Vel = v3FwdVel + v3SideVel
		
	end
	
	self.v3Vel = v3Vel
	
	-- Make trail glow brighter the closer the bullet is to camera
	local cam = scn.ActiveCamera()
	
	if entTrail and cam then
		
		local v3Cam = cam:Pos()
		local nOpacity = self.nTrailMaxOpacity * (1.0 - math.min(vec.Mag3(v3New - v3Cam) / 1000.0, 1.0))
		entTrail:SetOpacity(nOpacity)
		
		if nOpacity > 0.0 then
			entTrail:SetVisible(true)
		else
			entTrail:SetVisible(false) -- Don't recolor if not glowing
		end
		
	end
	
end

--[[------------------------------------
	bul:DoHit

OUT	v3Hit
--------------------------------------]]
function bul:DoHit(v3Pos, v3New, iContact, nTF, nTL, v3Nrm, entHit)
	
	local entHurt = entHit
	
	if entHit then
		
		if phy.Enabled(entHurt) then
			entHurt.v3Vel = entHurt.v3Vel + self.v3Vel * self.nFrcFac
		end
		
		if entHurt.Hurt then
			entHurt:Hurt(self.entOwner, self.nDmg, self.v3Vel, entHit)
		end
		
	end
	
	local v3Hit = hit.RespondStop(v3Pos, v3New, iContact, nTF, nTL, v3Nrm)
	
	-- FIXME: CreateImpact should take position and direction, not collision results
	imp.CreateImpact(v3Pos, v3New, iContact, nTF, nTL, v3Nrm, entHurt)
	return v3Hit
	
end

bul.typ = scn.CreateEntityType("bullet", bul.Init, bul.Tick, nil, nil, nil)

return bul