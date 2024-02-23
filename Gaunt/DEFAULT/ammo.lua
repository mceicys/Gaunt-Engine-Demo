-- ammo.lua
LFV_EXPAND_VECTORS()

local hit = ghit
local qua = gqua
local rnd = grnd
local scn = gscn
local scr = gscr
local wrp = gwrp

local psp = scr.EnsureScript("palswap.lua")

local EntDelete = scn.EntDelete
local EntOri = scn.EntOri
local EntSetOri = scn.EntSetOri
local qua_Product = qua.Product
local qua_QuaYaw = qua.QuaYaw
local wrp_TimeStep = wrp.TimeStep

local amo = {}
amo.__index = amo

amo.mshAmmo = rnd.EnsureMesh("ammo.msh")

amo.tTextures = {
	ammobullet = rnd.EnsureTexture("ambullet.tex"),
	ammorocket = rnd.EnsureTexture("amrocket.tex")
}

amo.tSubPalettes = {
	ammobullet = 4,
	ammorocket = 1
}

amo.tGetFuncs = {}

amo.hul = hit.HullBox(-16, -16, -16, 16, 16, 16)
amo.nSpinRate = 2.8

--[[------------------------------------
	amo:Init
--------------------------------------]]
function amo:Init()
	
	setmetatable(self:Table(), amo)
	local sType = self:TypeName()
	self:FlagSet():Or(I_ENT_TRIGGER)
	self:SetHull(self.hul)
	self:SetScale(8)
	self:SetMesh(self.mshAmmo)
	self:SetTexture(self.tTextures[sType])
	self:SetRegularGlass(true)
	self:SetShadow(false)
	self:SetSubPalette(self.tSubPalettes[sType])
	
end

--[[------------------------------------
	amo:Tick
--------------------------------------]]
function amo:Tick()
	
	local nStep = wrp_TimeStep()
	local nSpin = self.nSpinRate * nStep
	local q4Spin = qua_QuaYaw(nSpin)
	local q4Ori = EntOri(self)
	EntSetOri(self, qua_Product(q4Spin, q4Ori))
	
end

--[[------------------------------------
	amo.Touch

OUT	v3Pos, v3Vel
--------------------------------------]]
function amo.Touch(entOther, tRes, v3P, v3PP, v3V, v3PV, nTime)
	
	local ent = tRes[7]
	
	if ent.tGetFuncs[ent:TypeName()](ent, entOther) then
		
		psp.iGetTime = wrp.Time() + 80
		EntDelete(ent)
		
	end
	
	return v3P, v3V
	
end

--[[------------------------------------
	amo.tGetFuncs.ammobullet

OUT	bGot
--------------------------------------]]
function amo.tGetFuncs.ammobullet(ent, entGetter)
	
	if entGetter.AddBullets then
		return entGetter:AddBullets(10) > 0
	end
	
	return false
	
end

--[[------------------------------------
	amo.tGetFuncs.ammorocket

OUT	bGot
--------------------------------------]]
function amo.tGetFuncs.ammorocket(ent, entGetter)
	
	if entGetter.AddRockets then
		return entGetter:AddRockets(1) > 0
	end
	
	return false
	
end

amo.typAmmoBullet = scn.CreateEntityType("ammobullet", amo.Init, amo.Tick, nil, nil, nil)
amo.typAmmoRocket = scn.CreateEntityType("ammorocket", amo.Init, amo.Tick, nil, nil, nil)

return amo