-- player.lua
LFV_EXPAND_VECTORS()

local aud = gaud
local con = gcon
local flg = gflg
local hit = ghit
local key = gkey
local qua = gqua
local rnd = grnd
local scn = gscn
local scr = gscr
local vec = gvec
local wrp = gwrp

local bul = scr.EnsureScript("bullet.lua")
local com = scr.EnsureScript("common.lua")
local phy = scr.EnsureScript("physics.lua")
local roc = scr.EnsureScript("rocket.lua")

local EntFlagSet = scn.EntFlagSet
local EntHull = scn.EntHull
local EntHullOri = scn.EntHullOri
local EntPos = scn.EntPos
local EntSetOverlay = scn.EntSetOverlay
local EntSetWorldVisible = scn.EntSetWorldVisible
local FlagSetTrue = flg.FlagSetTrue
local hit_NONE = hit.NONE
local hit_TestHullHull = hit.TestHullHull
local math_abs = math.abs
local math_floor = math.floor
local math_sin = math.sin
local phy_Push = phy.Push
local scn_EntityExists = scn.EntityExists

local plr = {}
plr.__index = plr

-- FIXME: console options that aren't affected by loading; should that be done thru engine's con::Options or a script-side variant?
plr.nPitSensitivity = 0.5 * 1.6 -- FIXME: action sensitivity
plr.nYawSensitivity = 0.5 * 0.8 -- FIXME: action sensitivity
plr.nLookSensitivity = 0.08 -- FIXME: action sensitivity?

plr.iYawCacheType = 3
	-- 1: don't cache
	-- 2: cache torque
	-- 3: cache max-torque time
	-- 4: cache delta, look at future heading

plr.nCancelYawCacheThreshold = 4.0 --[[ Opposite input > this threshold clears torque or time
									cache; set math.huge for no canceling ]]
plr.bBrakeReduceYawCache = true --[[ Fractionize cache when brake reduces yaw ability; delta
							cache only has its velocity affected ]]

plr.hul = hit.HullBox(-4, -4, -4, 4, 4, 4) -- FIXME: hul file, oriented
plr.hulLink = hit.HullBox(-15, -15, -15, 15, 15, 15) -- FIXME: make hul envelop mesh
plr.hulTurb = hit.HullBox(-4, -4, -4, 4, 4, 4) -- FIXME: Want this to be bigger than player's hull but intersections are a problem when flying really close to obstacles
	-- FIXME: Maybe it should be player's hull exactly, with same orientation too

plr.fsetIgnore = flg.FlagSet():Or(I_ENT_TRIGGER)
plr.fsetProbeIgnore = flg.FlagSet():Or(I_ENT_TRIGGER, I_ENT_MOBILE)

plr.mshBody = rnd.EnsureMesh("player.msh")
plr.mshCanopyFrame = rnd.EnsureMesh("plframe.msh")
plr.texBody = rnd.EnsureTexture("player.tex")
plr.texBodyMin = rnd.EnsureTexture("player_min.tex")

plr.mshWindowOuter = rnd.EnsureMesh("plwinout.msh")
plr.texWindowOuter = rnd.EnsureTexture("plwinout.tex")

plr.mshWindowInner = rnd.EnsureMesh("plwinin.msh")
plr.texWindowInner = rnd.EnsureTexture("plwinin.tex")

plr.mshCockpit = rnd.EnsureMesh("plpit.msh")
plr.texCockpit = rnd.EnsureTexture("plpit.tex")
plr.texCockpitMin = rnd.EnsureTexture("plpit_min.tex")

plr.mshHUD = rnd.EnsureMesh("plhud.msh")
plr.texHUD = rnd.EnsureTexture("plhud.tex")

plr.mshSpeed = rnd.EnsureMesh("plspeed.msh")
plr.mshAltitude = rnd.EnsureMesh("plalt.msh")
plr.mshAttitude = rnd.EnsureMesh("platt.msh")
plr.mshCompass = rnd.EnsureMesh("plcomp.msh") -- FIXME: remove
plr.mshBall = rnd.EnsureMesh("plball.msh")
plr.mshDrum = rnd.EnsureMesh("pldrum.msh")
plr.mshGunBar = rnd.EnsureMesh("plgunbar.msh")
plr.mshGunName = rnd.EnsureMesh("plgunnam.msh")
plr.mshHealth = rnd.EnsureMesh("plhealth.msh")
plr.mshWarn = rnd.EnsureMesh("plwarn.msh")

plr.mshPilot = rnd.EnsureMesh("plpilot.msh")
plr.mshHead = rnd.EnsureMesh("plhead.msh")
plr.mshHose = rnd.EnsureMesh("plhose.msh")
plr.aniHoseExtend = plr.mshHose:Animation("extend")
plr.texPilot = rnd.EnsureTexture("plpilot.tex")

plr.nMass = 1.0
plr.ImpactVelocity = phy.ImpactVelocityBounce
plr.nBounceLimit = 50
plr.nBounceFactor = 0.5
plr.nImpactFric = 0.3
plr.nModelPitch = 0.0
plr.nGunPitch = 0.1
plr.iMaxBullets = 999
plr.iMaxRockets = 16
plr.nTrim = 25.5

-- Lift actually stalls in angle range [asin(0.33), asin(0.33 + 1/3)]
	-- nGoodLift has a smaller range so user notices shake sooner
plr.nGoodLiftAngle = 0.33
plr.nGoodLiftGroundAngle = 0.0
plr.nGoodLiftRange = 0.2

plr.nGoodBrakeAngle = 0.336303575
plr.nGoodBrakeGroundAngle = 0.0
plr.nGoodBrakeRange = 0.388960839
plr.nGoodBrakeSpeed = 200.0

local tGunSounds = {
	--[[
	aud.EnsureSound("mg100.wav"),
	aud.EnsureSound("mg112.wav"),
	aud.EnsureSound("mg124.wav"),
	aud.EnsureSound("mg136.wav"),
	aud.EnsureSound("mg148.wav"),
	aud.EnsureSound("mg160.wav"),
	aud.EnsureSound("mg172.wav"),
	aud.EnsureSound("mg184.wav"),
	aud.EnsureSound("mg196.wav"),
	aud.EnsureSound("mg208.wav"),
	aud.EnsureSound("mg196.wav"),
	aud.EnsureSound("mg184.wav"),
	aud.EnsureSound("mg172.wav"),
	aud.EnsureSound("mg160.wav"),
	aud.EnsureSound("mg148.wav"),
	aud.EnsureSound("mg136.wav"),
	aud.EnsureSound("mg124.wav"),
	aud.EnsureSound("mg112.wav")
	--]]
}

local DEG_TO_RAD = 0.01745329251994329576923690768489

local GUN_NONE = 0
local GUN_MINIGUN = 1
local GUN_ROCKET = 2

local LOOK_NORMAL = 1
local LOOK_BACK = 2
local LOOK_DRIFT = 3

--[[------------------------------------
	plr:Init
--------------------------------------]]
function plr:Init()
	
	local v3Pos = self:Pos()
	local q4Ori = self:Ori()
	
	entPlayer = self
	self:Prioritize()
	setmetatable(self:Table(), plr)
	self:SetLinkHull(self.hulLink)
	self:SetHull(self.hul)
	self:FlagSet():Or(I_ENT_MOBILE)
	
	-- Visuals
	-- FIXME: add sockets for canopy, vector-thrust flaps, stabilators, flaps, ailerons (decelerons), ejection seat, cockpit dials, etc.
	local entBody = scn.CreateEntity(nil, v3Pos, q4Ori)
	self:SetChild(entBody)
	self.entBody = entBody
	entBody:SetMesh(self.mshBody)
	entBody:SetTexture(self.texBody)
	
	local entBodyMimic = scn.CreateEntity(nil, entBody:Place())
	self.entBodyMimic = entBodyMimic
	entBody:SetChild(entBodyMimic)
	entBodyMimic:SetAmbientGlass(true)
	entBodyMimic:SetOpacity(1.0)
	entBodyMimic:SetShadow(false)
	entBodyMimic:SetTexture(self.texBodyMin)
	entBodyMimic:MimicMesh(entBody)
	
	local entCanopyFrame = scn.CreateEntity(nil, v3Pos, q4Ori)
	entBodyMimic:SetChild(entCanopyFrame)
	self.entCanopyFrame = entCanopyFrame
	entCanopyFrame:SetMesh(self.mshCanopyFrame)
	entCanopyFrame:SetTexture(self.texBody)
	
	local entWindowOuter = scn.CreateEntity(nil, v3Pos, q4Ori)
	entCanopyFrame:SetChild(entWindowOuter)
	self.entWindowOuter = entWindowOuter
	entWindowOuter:SetMesh(self.mshWindowOuter)
	entWindowOuter:SetTexture(self.texWindowOuter)
	entWindowOuter:SetRegularGlass(true)
	entWindowOuter:SetOpacity(0.5)
	entWindowOuter:SetShadow(false)
	entWindowOuter:SetSubPalette(3)
	
	local entWindowInner = scn.CreateEntity(nil, v3Pos, q4Ori)
	entWindowOuter:SetChild(entWindowInner)
	self.entWindowInner = entWindowInner
	entWindowInner:SetMesh(self.mshWindowInner)
	entWindowInner:SetTexture(self.texWindowInner)
	entWindowInner:SetRegularGlass(true)
	entWindowInner:SetOpacity(0.0) -- FIXME: do spherical reflection shader
	entWindowInner:SetShadow(false)
	
	local entCockpit = scn.CreateEntity(nil, v3Pos, q4Ori)
	entWindowInner:SetChild(entCockpit)
	self.entCockpit = entCockpit
	entCockpit:SetMesh(self.mshCockpit)
	entCockpit:SetTexture(self.texCockpit)
	
	local entCockpitMimic = scn.CreateEntity(nil, entCockpit:Place())
	self.entCockpitMimic = entCockpitMimic
	entCockpit:SetChild(entCockpitMimic)
	entCockpitMimic:SetMinimumGlass(true)
	entCockpitMimic:SetShadow(false)
	entCockpitMimic:SetTexture(self.texCockpitMin)
	entCockpitMimic:MimicMesh(entCockpit)
	
	local entHUD = scn.CreateEntity(nil, v3Pos, q4Ori)
	entCockpitMimic:SetChild(entHUD)
	self.entHUD = entHUD
	entHUD:SetMesh(self.mshHUD)
	entHUD:SetTexture(self.texHUD)
	entHUD:SetRegularGlass(true)
	entHUD:SetWeakGlass(true)
	entHUD:SetSubPalette(2)
	
	local entSpeed = scn.CreateEntity(nil, v3Pos, q4Ori)
	entHUD:SetChild(entSpeed)
	self.entSpeed = entSpeed
	entSpeed:SetMesh(self.mshSpeed)
	entSpeed:SetTexture(self.texHUD)
	entSpeed:SetRegularGlass(true)
	entSpeed:SetWeakGlass(true)
	entSpeed:SetSubPalette(1)
	
	local entAltitude = scn.CreateEntity(nil, v3Pos, q4Ori)
	entSpeed:SetChild(entAltitude)
	self.entAltitude = entAltitude
	entAltitude:SetMesh(self.mshAltitude)
	entAltitude:SetTexture(self.texHUD)
	entAltitude:SetRegularGlass(true)
	entAltitude:SetWeakGlass(true)
	entAltitude:SetSubPalette(2)
	
	local entAltDrum1 = scn.CreateEntity(nil, v3Pos, q4Ori)
	entAltitude:SetChild(entAltDrum1)
	self.entAltDrum1 = entAltDrum1
	entAltDrum1:SetScale(0.0222)
	entAltDrum1:SetMesh(self.mshDrum)
	entAltDrum1:SetTexture(self.texHUD)
	entAltDrum1:SetRegularGlass(true)
	entAltDrum1:SetWeakGlass(true)
	entAltDrum1:SetSubPalette(2)
	
	local entAltDrum2 = scn.CreateEntity(nil, v3Pos, q4Ori)
	entAltDrum1:SetChild(entAltDrum2)
	self.entAltDrum2 = entAltDrum2
	entAltDrum2:SetScale(0.0222)
	entAltDrum2:SetMesh(self.mshDrum)
	entAltDrum2:SetTexture(self.texHUD)
	entAltDrum2:SetRegularGlass(true)
	entAltDrum2:SetWeakGlass(true)
	entAltDrum2:SetSubPalette(2)
	
	local entAltDrum3 = scn.CreateEntity(nil, v3Pos, q4Ori)
	entAltDrum2:SetChild(entAltDrum3)
	self.entAltDrum3 = entAltDrum3
	entAltDrum3:SetScale(0.0222)
	entAltDrum3:SetMesh(self.mshDrum)
	entAltDrum3:SetTexture(self.texHUD)
	entAltDrum3:SetRegularGlass(true)
	entAltDrum3:SetWeakGlass(true)
	entAltDrum3:SetSubPalette(2)
	
	local entBall = scn.CreateEntity(nil, v3Pos, q4Ori)
	entAltDrum3:SetChild(entBall)
	self.entBall = entBall
	entBall:SetMesh(self.mshBall)
	entBall:SetTexture(self.texHUD)
	entBall:SetRegularGlass(true)
	entBall:SetWeakGlass(true)
	entBall:SetOpacity(1)
	entBall:SetSubPalette(2)
	
	local entGunBar = scn.CreateEntity(nil, v3Pos, q4Ori)
	entBall:SetChild(entGunBar)
	self.entGunBar = entGunBar
	entGunBar:SetMesh(self.mshGunBar)
	entGunBar:SetTexture(self.texHUD)
	entGunBar:SetRegularGlass(true)
	entGunBar:SetWeakGlass(true)
	entGunBar:SetSubPalette(2)
	
	local entGunName = scn.CreateEntity(nil, v3Pos, q4Ori)
	entGunBar:SetChild(entGunName)
	self.entGunName = entGunName
	entGunName:SetMesh(self.mshGunName)
	entGunName:SetTexture(self.texHUD)
	entGunName:SetRegularGlass(true)
	entGunName:SetWeakGlass(true)
	entGunName:SetSubPalette(2)
	
	local entGunDrum1 = scn.CreateEntity(nil, v3Pos, q4Ori)
	entGunName:SetChild(entGunDrum1)
	self.entGunDrum1 = entGunDrum1
	entGunDrum1:SetScale(0.0335)
	entGunDrum1:SetMesh(self.mshDrum)
	entGunDrum1:SetTexture(self.texHUD)
	entGunDrum1:SetRegularGlass(true)
	entGunDrum1:SetWeakGlass(true)
	entGunDrum1:SetSubPalette(2)
	
	local entGunDrum2 = scn.CreateEntity(nil, v3Pos, q4Ori)
	entGunDrum1:SetChild(entGunDrum2)
	self.entGunDrum2 = entGunDrum2
	entGunDrum2:SetScale(0.0335)
	entGunDrum2:SetMesh(self.mshDrum)
	entGunDrum2:SetTexture(self.texHUD)
	entGunDrum2:SetRegularGlass(true)
	entGunDrum2:SetWeakGlass(true)
	entGunDrum2:SetSubPalette(2)
	
	local entGunDrum3 = scn.CreateEntity(nil, v3Pos, q4Ori)
	entGunDrum2:SetChild(entGunDrum3)
	self.entGunDrum3 = entGunDrum3
	entGunDrum3:SetScale(0.0335)
	entGunDrum3:SetMesh(self.mshDrum)
	entGunDrum3:SetTexture(self.texHUD)
	entGunDrum3:SetRegularGlass(true)
	entGunDrum3:SetWeakGlass(true)
	entGunDrum3:SetSubPalette(2)
	
	local entHealth = scn.CreateEntity(nil, v3Pos, q4Ori)
	entGunDrum3:SetChild(entHealth)
	self.entHealth = entHealth
	entHealth:SetMesh(self.mshHealth)
	entHealth:SetTexture(self.texHUD)
	entHealth:SetRegularGlass(true)
	entHealth:SetWeakGlass(true)
	entHealth:SetSubPalette(2)
	
	local entWarn = scn.CreateEntity(nil, v3Pos, q4Ori)
	entHealth:SetChild(entWarn)
	self.entWarn = entWarn
	entWarn:SetMesh(self.mshWarn)
	entWarn:SetTexture(self.texHUD)
	entWarn:SetRegularGlass(true)
	--entWarn:SetWeakGlass(true)
	entWarn:SetSubPalette(1)
	
	local entPilot = scn.CreateEntity(nil, v3Pos, q4Ori)
	entWarn:SetChild(entPilot)
	self.entPilot = entPilot
	entPilot:SetMesh(self.mshPilot)
	entPilot:SetTexture(self.texPilot)
	
	local entHead = scn.CreateEntity(nil, v3Pos, q4Ori)
	entPilot:SetChild(entHead)
	self.entHead = entHead
	entHead:SetMesh(self.mshHead)
	entHead:SetTexture(self.texPilot)
	self.q4HeadRel = 0, 0, 0, 1
	
	local entHose = scn.CreateEntity(nil, v3Pos, q4Ori)
	entHead:SetChild(entHose)
	self.entHose = entHose
	entHose:SetMesh(self.mshHose)
	entHose:SetTexture(self.texPilot)
	entHose:SetAnimationSpeed(0)
	entHose:SetAnimation(self.aniHoseExtend, 0, 0)
	
	phy.Init(self)
	
	self.nHealth = 100
	self.tHitTimes = {}
	
	self.v3PrevVel = self.v3Vel
	self.v3Stability = 1, 1, 1
	self.v3OriVel = 0, 0, 0 -- Angular velocity (axis relative to current orientation with magnitude as angle)
	self.nYawCache = 0
	self.nYawTimeCache = 0
	self.nYawDelta, self.nYawDeltaVel, self.nYawFollowVel = 0, 0, 0
	self.nYawCamTemp = 0
	self.nPrevBrakeTurn = 1
	self.nJoltAdded = 0 -- Records how much pitch has been added for thrust/brake feedback
	self.nLagPit, self.nLagYaw = 0, 0
	self.iLookMode = LOOK_NORMAL
	self.nLookTempRol, self.nLookTempPit, self.nLookTempYaw = 0, 0, 0
	self.nAimLookRol, self.nAimLookPit, self.nAimLookYaw = 0, 0, 0
	self.nDriftLookPit, self.nDriftLookYaw = 0, 0
	self.nSlowShake = 0
	self.v2FastShake = 0, 0
	self.v3Turb = 0, 0, 0
	self.nTurbClose = 0
	self.nTurbMiss, self.nTurbMissReserve = 0, 0
	self.tProbes = {0, 0, 0, 0, 0, 0, 0, 0}
	self.v3HeldWeight = 0, 0, 0
	self.v3Bob, self.v3BobVel = 0, 0, 0, 0, 0, 0
	self.v3OriBob, self.v3OriBobVel = 0, 0, 0, 0, 0, 0
	
	self.iGun = GUN_NONE
	self.nFireTime = 0.0
	self.iNumBullets = 30
	self.iNumRockets = 0
	self.nAmmoDisp = 0.0
	
	local voxGun1 = aud.Voice(v3Pos, 1000, 0.0, 1.0)
	voxGun1:SetBackground(true) -- FIXME: only when self.cam is active
	self.voxGun1 = voxGun1
	
	local voxGun2 = aud.Voice(v3Pos, voxGun1:Radius(), 0.0, voxGun1:Volume())
	voxGun2:SetBackground(voxGun1:Background()) -- FIXME: see above
	self.voxGun2 = voxGun2
	
	self.iGunVoice = 1
	self.iGunSound = 1
	self.iTracerCount = 0 -- FIXME TEMP
	
	local nVFOV = math.atan(3.0 / 4.0) * 2.0
	self.cam = scn.Camera(v3Pos, q4Ori, nVFOV) -- 90 deg horizontal at 4:3
	self.camOverlay = scn.Camera(v3Pos, q4Ori, nVFOV)
	scn.GetOverlay(0):SetCamera(self.camOverlay)
	
	if not scn.ActiveCamera() then
		scn.SetActiveCamera(self.cam)
	end
	
end

--[[------------------------------------
	plr:Level
--------------------------------------]]
function plr:Level()
	
end

--[[------------------------------------
	plr:Tick
--------------------------------------]]
function plr:Tick()
	 
	if self.nHealth <= 0.0 then
		
		-- FIXME TEMP
		self:Delete()
		return
		
	end
	
	local nStep = wrp.TimeStep()
	local v3OldVel = self.v3Vel
	local v3OldPos = EntPos(self)
	
	-- Check camera
	local cam, camOverlay = self.cam, self.camOverlay
	local bCam = scn.ActiveCamera() == cam
	local bLookBack = key.Down(ACT_LOOK_BACK)
	
	if bCam then
		
		local iFwdOverlay = bLookBack and -1 or 0 -- Show if not looking back
			-- FIXME: this needs to check iLookMode AFTER most of the tick is done; make this stuff a separate function
		local iOverlay = 0
			-- Ship is drawn as overlay so it never clips through outside geometry
			
			-- FIXME FIXME FIXME: glass isn't stenciled out by overlay
		
		EntSetWorldVisible(self.entBody, false)
		EntSetOverlay(self.entBody, iOverlay)
		EntSetWorldVisible(self.entCanopyFrame, false)
		EntSetOverlay(self.entCanopyFrame, iOverlay)
		EntSetWorldVisible(self.entWindowOuter, false)
		--EntSetOverlay(self.entWindowOuter, iOverlay) -- Not visible from inside anyway
		EntSetWorldVisible(self.entWindowInner, false)
		EntSetOverlay(self.entWindowInner, iOverlay)
		EntSetWorldVisible(self.entCockpit, false)
		EntSetOverlay(self.entCockpit, iOverlay)
		EntSetWorldVisible(self.entHUD, false)
		EntSetOverlay(self.entHUD, iOverlay)
		EntSetWorldVisible(self.entSpeed, false)
		EntSetOverlay(self.entSpeed, iOverlay)
		EntSetWorldVisible(self.entAltitude, false)
		EntSetOverlay(self.entAltitude, iOverlay)
		EntSetWorldVisible(self.entAltDrum1, false)
		EntSetOverlay(self.entAltDrum1, iOverlay)
		EntSetWorldVisible(self.entAltDrum2, false)
		EntSetOverlay(self.entAltDrum2, iOverlay)
		EntSetWorldVisible(self.entAltDrum3, false)
		EntSetOverlay(self.entAltDrum3, iOverlay)
		EntSetWorldVisible(self.entBall, false)
		EntSetOverlay(self.entBall, iOverlay)
		EntSetWorldVisible(self.entGunBar, false)
		EntSetOverlay(self.entGunBar, iOverlay)
		EntSetWorldVisible(self.entGunName, false)
		EntSetOverlay(self.entGunName, iOverlay)
		EntSetWorldVisible(self.entGunDrum1, false)
		EntSetOverlay(self.entGunDrum1, iOverlay)
		EntSetWorldVisible(self.entGunDrum2, false)
		EntSetOverlay(self.entGunDrum2, iOverlay)
		EntSetWorldVisible(self.entGunDrum3, false)
		EntSetOverlay(self.entGunDrum3, iOverlay)
		EntSetWorldVisible(self.entHealth, false)
		EntSetOverlay(self.entHealth, iOverlay)
		EntSetWorldVisible(self.entWarn, false)
		EntSetOverlay(self.entWarn, iOverlay)
		EntSetWorldVisible(self.entPilot, false)
		EntSetOverlay(self.entPilot, iFwdOverlay)
		EntSetWorldVisible(self.entHead, false)
		--EntSetOverlay(self.entHead, -1)
		EntSetWorldVisible(self.entHose, false)
		EntSetOverlay(self.entHose, iFwdOverlay)
		
	else
		
		if self.entBody:WorldVisible() ~= true or self.entBody:Overlay() ~= -1 then
			
			EntSetWorldVisible(self.entBody, true)
			EntSetOverlay(self.entBody, -1)
			EntSetWorldVisible(self.entCanopyFrame, true)
			EntSetOverlay(self.entCanopyFrame, -1)
			EntSetWorldVisible(self.entWindowOuter, true)
			--EntSetOverlay(self.entWindowOuter, -1)
			EntSetWorldVisible(self.entWindowInner, true)
			EntSetOverlay(self.entWindowInner, -1)
			EntSetWorldVisible(self.entCockpit, true)
			EntSetOverlay(self.entCockpit, -1)
			EntSetWorldVisible(self.entHUD, true)
			EntSetOverlay(self.entHUD, -1)
			EntSetWorldVisible(self.entSpeed, true)
			EntSetOverlay(self.entSpeed, -1)
			EntSetWorldVisible(self.entAltitude, true)
			EntSetOverlay(self.entAltitude, -1)
			EntSetWorldVisible(self.entAltDrum1, true)
			EntSetOverlay(self.entAltDrum1, -1)
			EntSetWorldVisible(self.entAltDrum2, true)
			EntSetOverlay(self.entAltDrum2, -1)
			EntSetWorldVisible(self.entAltDrum3, true)
			EntSetOverlay(self.entAltDrum3, -1)
			EntSetWorldVisible(self.entBall, true)
			EntSetOverlay(self.entBall, -1)
			EntSetWorldVisible(self.entGunBar, true)
			EntSetOverlay(self.entGunBar, -1)
			EntSetWorldVisible(self.entGunName, true)
			EntSetOverlay(self.entGunName, -1)
			EntSetWorldVisible(self.entGunDrum1, true)
			EntSetOverlay(self.entGunDrum1, -1)
			EntSetWorldVisible(self.entGunDrum2, true)
			EntSetOverlay(self.entGunDrum2, -1)
			EntSetWorldVisible(self.entGunDrum3, true)
			EntSetOverlay(self.entGunDrum3, -1)
			EntSetWorldVisible(self.entHealth, true)
			EntSetOverlay(self.entHealth, -1)
			EntSetWorldVisible(self.entWarn, true)
			EntSetOverlay(self.entWarn, -1)
			EntSetWorldVisible(self.entPilot, true)
			EntSetOverlay(self.entPilot, -1)
			EntSetWorldVisible(self.entHead, true)
			--EntSetOverlay(self.entHead, -1)
			EntSetWorldVisible(self.entHose, true)
			EntSetOverlay(self.entHose, -1)
			
		end
		
	end
	
	local bControl = true --bCam -- FIXME TEMP
	local bAimingLook = key.Down(ACT_LOOK)
	
	-- Get movement input
	local nThrust = 0.0
	local nBrake = 0.0
	
	if bControl then
		
		if key.Down(ACT_THROTTLE) then nThrust = nThrust + 1.0 end
		if key.Down(ACT_BRAKE) then nBrake = nBrake + 1.0 end
		
	end
	
	local q4Ori = self:Ori()
	local v3Vel = v3OldVel
	
	-- Check for ground effect
	-- FIXME: separate function
	local nGroundEffect = 0.0
	local v3Up = qua.Up(q4Ori)
	local v3VelDir = vec.Normalized3(v3Vel)
	local nUpPar = vec.Dot3(v3VelDir, v3Up)
	local v3GroundDir, bErr = vec.Normalized3(v3VelDir * nUpPar - v3Up)
	
	if not bErr then
		
		local v3GroundTest = v3GroundDir * 32-- + v3Vel * 0.1
		
		-- FIXME: make this a hull test once the player has a precise oriented hull
		local iGrdC, nGrdTF, _, v3GrdNorm = hit.LineTest(v3OldPos, v3OldPos + v3GroundTest,
			hit.ALL, self.fsetProbeIgnore, self)
		
		if iGrdC == hit.HIT then
			
			nGroundEffect = math.min(1.0 - nGrdTF + 0.125, 1.0) -- FIXME TEMP: added constant compensates for line test starting in hull
			nGroundEffect = nGroundEffect ^ 1.5
			local nVel = vec.Mag3(v3Vel)
			nGroundEffect = nGroundEffect * math.min(nVel / 200.0, 1.0) -- Reduce effect at low speed
			local nUpFac = com.Clamp(1.0 - (math_abs(nUpPar) - 0.7) / 0.3, 0, 1) -- Disable effect if player's up is parallel with velocity
			nGroundEffect = nGroundEffect * nUpFac
			
			--[[
			-- Partially align to surface to make it feel like a cushion and help lift-turns
			local v3Fwd = qua.Dir(q4Ori)
			local nTrimFac = self.nTrim / math.max(100.0, nVel)
			local v3GrdAxis, nGrdAngle = vec.DirDelta(v2Up + v2Fwd * nTrimFac, zUp, v3GrdNorm)
				-- Some trim is applied so alignment doesn't send player towards surface
			
			if vec.Dot3(v3GrdAxis, qua.Left(q4Ori)) > 0.0 then
				v3GrdAxis = v3GrdAxis * 0.0 -- Pitch down less so surfaces don't feel sticky
			end
			
			local nDot = vec.Dot3(v3GrdAxis, v3VelDir)
			v3GrdAxis = v3GrdAxis - v3VelDir * nDot -- Make axis perpendicular to velocity
			v3GrdAxis = qua.VecRotInv(v3GrdAxis, q4Ori)
			local nAngFac = com.Clamp(1.0 - (nGrdAngle - 0.75) / 0.8, 0, 1) -- Don't rotate if surface angle is extreme
			local nOriVelAdd = nGrdAngle * nAngFac * 20.0 * nGroundEffect * nStep
			self.v3OriVel = self.v3OriVel + v3GrdAxis * nOriVelAdd
			]]
			
		end
		
	end
	
	self.nGroundEffect = nGroundEffect
	
	v3Vel, self.v3OriVel, self.zStability = self:AfterBrake(v3OldVel, self.v3OriVel,
		self.zStability, q4Ori, nThrust, nBrake)
	
	-- FIXME: have OrientAttitude output changes instead of setting them so it can be used for prediction like AfterBrake and NewVelocity
		-- Will need a bPrediction parameter so it doesn't change tProbes
		-- Or just scrap the prediction requirement and simplify the functions, AI is not using it right now
	local nAddPit, nAddYaw = self:OrientAttitude(bControl, bAimingLook, nThrust, nBrake, v3Vel)
	q4Ori = self:Ori()
	
	-- FIXME: most of these return values aren't needed anymore
	local nGoodLift, nGoodAOA, nLiftFwdDot, nLiftUpDot, v3BefThrust, v3AftThrust
	v3Vel, nGoodLift, nGoodAOA, nLiftFwdDot, nLiftUpDot, v3BefThrust, v3AftThrust =
		self:NewVelocity(v3Vel, q4Ori, nThrust, nBrake)
	
	self:SetOri(q4Ori)
	
	-- Calculate experienced weight (ignoring impacts), m/s^2
	local v3Weight = (v3Vel - v3OldVel) / nStep
	zWeight = zWeight - phy.zGravity
		-- Assume we're fighting off gravity, velocity delta will cancel this out in free fall
	
	-- Move
	v3Vel = phy.Push(self, v3Vel, nStep)
	self.v3Vel = v3Vel
	local v3Pos = self:Pos()
	
	-- Update aim
		-- Aim is orientation + slight additions for effect
	-- FIXME: either remove this or rename this, "aim" is being used for aim look
	--[[local q4AimRel = qua.QuaPitchYaw(self.v2Bump * nBumpFac)
	local q4Aim = qua.Product(q4Ori, q4AimRel)]]
	local q4Aim = q4Ori
	
	-- Update guns
	-- FIXME: w/ non-variable ticks, projectiles spawn at different orientation from camera when rotating
	local q4GunRel = qua.QuaPitchYaw(self.nGunPitch, 0.0)
	local q4Gun = qua.Product(q4Aim, q4GunRel)
	self.q4Gun = q4Gun -- For HUD
	
	if key.Pressed(ACT_NO_GUN) then
		self.iGun = GUN_NONE
	elseif key.Pressed(ACT_MINIGUN) then
		self.iGun = GUN_MINIGUN
	elseif key.Pressed(ACT_ROCKET) then
		self.iGun = GUN_ROCKET
	end
	
	local iGun = self.iGun
	
	if self.nFireTime <= 0.0 then
		
		-- FIXME: ammo
		local nFireTimeSet = 0.0
		
		if key.Down(ACT_FIRE_ONE) then
			
			if iGun == GUN_MINIGUN then
				
				nFireTimeSet = self.nFireTime + 0.055
				
				if self.iNumBullets > 0 then
					
					self.iNumBullets = self.iNumBullets - 1
					
					local q4BadGun = qua.Product(q4Gun, qua.QuaPitchYaw((math.random() - 0.5) * 0.04,
						(math.random() - 0.5) * 0.04))
					
					local v3Gun = qua.VecRot(2, 0, -1, q4BadGun)
					local v3Fire = qua.VecRot(1500, 0, 0, q4BadGun)
					
					local entBul = bul.CreateBullet(v3OldPos + v3Gun, q4BadGun, self, v3Fire, v3Vel,
						10, 0, 1.0, false)
					
					--[[
					if self.iTracerCount ~= 0 then
						entBul:SetVisible(false)
					end
					
					self.iTracerCount = (self.iTracerCount + 1) % 4
					--]]
					
					local voxGunCur = self.voxGun1
					
					--[[
					local voxGunCur, voxGunOther
					
					if self.iGunVoice == 1 then
						voxGunCur, voxGunOther, self.iGunVoice = self.voxGun1, self.voxGun2, 2
					else
						voxGunCur, voxGunOther, self.iGunVoice = self.voxGun2, self.voxGun1, 1
					end
					
					--voxGunCur:FadeVolume(1.0, 0.0)
					--]]
					
					--voxGunCur:Play(tGunSounds[self.iGunSound % #tGunSounds + 1])
					self.iGunSound = self.iGunSound + math.random(3)
					--voxGunOther:FadeVolume(0.0, 0.05) -- FIXME: need mid-mix volume fading for this to work without popping
					
				else
					-- FIXME: play no-bullet sound
				end
				
			elseif iGun == GUN_ROCKET then
				
				-- FIXME: make rockets start with shooter's velocity and accelerate over time?
					-- bullet-like explosive can be some other weapon
				nFireTimeSet = self.nFireTime + 1.0
				
				if self.iNumRockets > 0 then
					
					self.iNumRockets = self.iNumRockets - 1
					local v3Gun = qua.VecRot(4, 0, -1.5, q4Gun)
					local v3Fire = qua.VecRot(500, 0, 0, q4Gun)
					roc.CreateRocket(v3OldPos + v3Gun, q4Gun, self, v3Fire, v3Vel, 100, 50, 200)
					
				else
					-- FIXME: play no-rocket sound
				end
				
			end
			
		end
		
		self.nFireTime = nFireTimeSet
		
	end
	
	self.nFireTime = self.nFireTime - nStep
	
	-- Have "expected" weight catch up over time
	local v3HeldWeight = self.v3HeldWeight
	local v3RelWeight = qua.VecRotInv(v3Weight, q4Ori)
	local v3WeightDif = v3RelWeight - v3HeldWeight
	local nWeightDif = vec.Mag3(v3WeightDif)
	local v3HeldWeightDel = 0, 0, 0
	local nBobStep = math.min(nStep, 0.1)
	
	if nWeightDif > 0.0 then
		
		local _, nHeldWeightDel = com.Smooth(nWeightDif, 0, 8.0, nil, nil, nBobStep)
		nHeldWeightDel = math_abs(nHeldWeightDel)
		local nMul = nHeldWeightDel / nWeightDif
		v3HeldWeightDel = v3WeightDif * nMul
		v3HeldWeight = v3HeldWeight + v3HeldWeightDel
		
	end
	
	self.v3HeldWeight = v3HeldWeight
	
	-- Update head bob
	local v3Bob, v3BobVel = self:UpdateBob(self.v3Bob, self.v3BobVel, v3HeldWeight,
		v3HeldWeightDel, nBobStep)
	
	self.v3Bob, self.v3BobVel = v3Bob, v3BobVel
	
	local v3OriBob, v3OriBobVel = self:UpdateOriBob(self.v3OriBob, self.v3OriBobVel,
		v3HeldWeight, v3HeldWeightDel, nBobStep)
	
	self.v3OriBob, self.v3OriBobVel = v3OriBob, v3OriBobVel
	
	-- Update look angles
	self.nYawCamTemp = com.Smooth(self.nYawCamTemp, 0.0, 10, 0.02)
	self:OrientAimLook(bAimingLook)
	self:OrientDriftLook(v3Vel, q4Ori)
	local q4Look = qua.QuaEuler(self:UpdateLookMode())
	
	-- Update cameras
	local q4Bob = qua.QuaAngularVelocity(v3OriBob)
	local q4Cam = qua.Product(q4Aim, q4Bob)
	q4Cam = qua.Product(q4Cam, q4Look)
	local v3BobRel = qua.VecRot(v3Bob, q4Ori)
	cam:SetOri(q4Cam)
	cam:SetPos(v3Pos + v3BobRel)
	camOverlay:SetOri(q4Cam)
	camOverlay:SetPos(v3Pos + v3BobRel)
	
	-- Update visuals -- FIXME: make function that's also called on init
	self:UpdateShake()
	
	-- Lag visual orientation based on change in heading
	-- FIXME: I want to remove this but the player needs feedback on input; wish the flight stick was visible...
	local nLagPit = com.ClampMag(com.Smooth(self.nLagPit, 0.0, 10.0) - self.yOriVel * nStep * 0.2, math.pi * 0.3)
	local nLagYaw = com.ClampMag(com.Smooth(self.nLagYaw, 0.0, 10.0) - self.zOriVel * nStep * 0.2, math.pi * 0.3)
	nLagPit, nLagYaw = 0, 0 -- FIXME TEMP
	
	self.nLagPit, self.nLagYaw = nLagPit, nLagYaw
	local nSlowShakeFac = 1.0 - math.min(nGoodAOA / 0.1, 1.0)
	local nFastShakeFac = 1.0 - nGoodLift
	local q4Lag = qua.QuaEuler(nLagYaw, nLagPit + self.nModelPitch + self.xFastShake * nFastShakeFac, nLagYaw + self.nSlowShake * nSlowShakeFac)
	local q4Body = qua.Product(q4Aim, q4Lag)
	local v3AbsShake = qua.VecRot(0.0, self.yFastShake * nFastShakeFac, self.nSlowShake * nSlowShakeFac, q4Body)
	--local nBumpFac = math.min(vec.Mag3(v3Vel) / 400.0, 1.0)
	--local v3AbsBump = qua.VecRot(0.0, self.v2Bump * nBumpFac, q4Body)
	local v3Body = v3Pos + v3AbsShake
	self.entBody:SetPlace(v3Body, q4Body)
	self.entBodyMimic:MimicMesh(self.entBody)
	self.entCanopyFrame:SetPlace(v3Body, q4Body)
	self.entWindowOuter:SetPlace(v3Body, q4Body)
	self.entWindowInner:SetPlace(v3Body, q4Body)
	self.entCockpit:SetPlace(v3Body, q4Body)
	self.entCockpitMimic:MimicMesh(self.entCockpit)
	
	local entHUD = self.entHUD
	entHUD:SetPlace(v3Body, q4Body)
	local mshHUD = entHUD:Mesh()
	
	local v3Part, q4Part = mshHUD:Socket("speed"):PlaceEntity(entHUD, 1.0)
	q4Part = qua.Product(q4Part, qua.QuaEuler(vec.Mag3(v3Vel) * (-4.2586033748661641676938054751122 / 600.0), 0.0, 0.0))
	self.entSpeed:SetPlace(v3Part, q4Part)
	
	local nShownAltitude = zPos + nWorldAltitude
	v3Part, q4Part = mshHUD:Socket("alt"):PlaceEntity(entHUD, 1.0)
	q4Part = qua.Product(q4Part, qua.QuaEuler(nShownAltitude * -2.0 * math.pi / 1000.0, 0.0, 0.0))
	self.entAltitude:SetPlace(v3Part, q4Part)
	
	local nClampAbs = math.max(0.0, nShownAltitude)
	
	v3Part, q4Part = mshHUD:Socket("altdrum1"):PlaceEntity(entHUD, 1.0)
	q4Part = qua.Product(q4Part, qua.QuaEuler(0.0, nClampAbs / 1000.0 * -2.0 * math.pi, 0.0))
	self.entAltDrum1:SetPlace(v3Part, q4Part)
	
	v3Part, q4Part = mshHUD:Socket("altdrum2"):PlaceEntity(entHUD, 1.0)
	local nDrum = nClampAbs / 10000.0
	local nDrumRem = math.fmod(nDrum, 0.1)
	q4Part = qua.Product(q4Part, qua.QuaEuler(0.0, (nDrum - nDrumRem + (nDrumRem > 0.09 and (nDrumRem - 0.09) * 10.0 or 0.0)) * -2.0 * math.pi, 0.0))
	self.entAltDrum2:SetPlace(v3Part, q4Part)
	
	v3Part, q4Part = mshHUD:Socket("altdrum3"):PlaceEntity(entHUD, 1.0)
	nDrum = nClampAbs / 100000.0
	nDrumRem = math.fmod(nDrum, 0.1)
	q4Part = qua.Product(q4Part, qua.QuaEuler(0.0, (nDrum - nDrumRem + (nDrumRem > 0.099 and (nDrumRem - 0.099) * 100.0 or 0.0)) * -2.0 * math.pi, 0.0))
	self.entAltDrum3:SetPlace(v3Part, q4Part)
	
	--[[
	if zPos >= 0.0 then
		
		self.entAltitude:SetSubPalette(0)
		self.entAltDrum1:SetSubPalette(0)
		self.entAltDrum2:SetSubPalette(0)
		self.entAltDrum3:SetSubPalette(0)
		
	else
		
		self.entAltitude:SetSubPalette(1)
		self.entAltDrum1:SetSubPalette(1)
		self.entAltDrum2:SetSubPalette(1)
		self.entAltDrum3:SetSubPalette(1)
		
	end
	--]]
	
	local v3Att, q4Att = mshHUD:Socket("att"):PlaceEntity(entHUD, 1.0)
	local nPartRol, nPartPit, nPartYaw = qua.Euler(q4Body)
	q4Part = qua.Product(q4Att, qua.QuaEuler(nPartRol, 0.0, 0.0))
	q4Part = qua.Product(q4Part, qua.QuaEuler(0.0, -nPartPit, 0.0))
	q4Part = qua.Product(q4Part, qua.QuaEuler(0.0, 0.0, nPartYaw - 1.5707963267948966192313216916398 + nWorldYaw)) -- Subtraction aligns north with y+ FIXME: rotate model?
	self.entBall:SetPlace(v3Att, q4Part)
	
	local iAmmoGoal, iAmmoMax = 0, 1
	
	if iGun == GUN_MINIGUN then
		iAmmoGoal, iAmmoMax = self.iNumBullets, self.iMaxBullets
	elseif iGun == GUN_ROCKET then
		iAmmoGoal, iAmmoMax = self.iNumRockets, self.iMaxRockets
	end
	
	if iAmmoGoal > 999 then
		iAmmoGoal = 999
	end
	
	if iAmmoMax > 999 then
		iAmmoMax = 999
	end
	
	-- FIXME: limit how quickly each digit can turn so hundred digit doesn't snap instantly when switching weapon?
	-- FIXME: the gun bar should also be smoothed, make a [0, 1] version of nAmmoDisp so iAmmoMax change doesn't make it jump
	local nAmmoDisp = com.Smooth(self.nAmmoDisp, iAmmoGoal, 25.0, 1.0)
	self.nAmmoDisp = nAmmoDisp
	
	local entGunBar = self.entGunBar
	entGunBar:SetPlace(v3Body, q4Body)
	entGunBar:SetUV(0.0, (1.0 - iAmmoGoal / iAmmoMax) * 51.0 / 256.0)
	
	local entGunName = self.entGunName
	entGunName:SetPlace(v3Body, q4Body)
	entGunName:SetUV(0.0, iGun * 8.0 / 256.0)
	
	v3Part, q4Part = mshHUD:Socket("gundrum1"):PlaceEntity(entHUD, 1.0)
	q4Part = qua.Product(q4Part, qua.QuaEuler(0.0, nAmmoDisp / 10.0 * -2.0 * math.pi, 0.0))
	self.entGunDrum1:SetPlace(v3Part, q4Part)
	
	v3Part, q4Part = mshHUD:Socket("gundrum2"):PlaceEntity(entHUD, 1.0)
	nDrum = nAmmoDisp / 100.0
	nDrumRem = math.fmod(nDrum, 0.1)
	q4Part = qua.Product(q4Part, qua.QuaEuler(0.0, (nDrum - nDrumRem + (nDrumRem > 0.09 and (nDrumRem - 0.09) * 10.0 or 0.0)) * -2.0 * math.pi, 0.0))
	self.entGunDrum2:SetPlace(v3Part, q4Part)
	
	v3Part, q4Part = mshHUD:Socket("gundrum3"):PlaceEntity(entHUD, 1.0)
	nDrum = nAmmoDisp / 1000.0
	nDrumRem = math.fmod(nDrum, 0.1)
	q4Part = qua.Product(q4Part, qua.QuaEuler(0.0, (nDrum - nDrumRem + (nDrumRem > 0.099 and (nDrumRem - 0.099) * 100.0 or 0.0)) * -2.0 * math.pi, 0.0))
	self.entGunDrum3:SetPlace(v3Part, q4Part)
	
	v3Part, q4Part = mshHUD:Socket("health"):PlaceEntity(entHUD, 1.0)
	q4Part = qua.Product(q4Part, qua.QuaEuler(math.max(0, self.nHealth) * (-math.pi / 100.0), 0.0, 0.0))
	self.entHealth:SetPlace(v3Part, q4Part)
	
	local entWarn = self.entWarn
	entWarn:SetPlace(v3Body, q4Body)
	entWarn:SetVisible(self.nHealth <= 25.0)
	
	local entPilot = self.entPilot
	entPilot:SetPlace(v3Body, q4Body)
	
	-- FIXME: clamping doesn't seem right when combining big ship lag with looking around, might just be limits of euler rotation
		-- qua.AngleClamp was bugged because HalfAngleSet was bugged, might work better now
		-- FIXME: also jitters roll sometimes
			-- FIXME: would be cool to clamp using quaternions
	local mshPilot = entPilot:Mesh()
	v3Part, q4Part = mshPilot:Socket("head"):PlaceEntity(entPilot, 1.0)
	local q4HeadRelGoal = qua.LocalDelta(q4Part, q4Cam)
	local nHeadRol, nHeadPit, nHeadYaw = qua.Euler(q4HeadRelGoal)
	nHeadRol = com.ClampMag(nHeadRol, 0.1)
	nHeadPit = com.Clamp(nHeadPit, -0.6, 0.2)
	nHeadYaw = com.ClampMag(nHeadYaw, 1.3)
	q4HeadRelGoal = qua.QuaEuler(nHeadRol, nHeadPit, nHeadYaw)
	local q4HeadRel = self.q4HeadRel
	local q4HeadRelDel = qua.LocalDelta(q4HeadRel, q4HeadRelGoal)
	q4HeadRelDel = qua.AngleClamp(q4HeadRelDel, 18.0 * nStep)
	q4HeadRel = qua.Product(q4HeadRel, q4HeadRelDel)
	self.q4HeadRel = q4HeadRel
	local entHead = self.entHead
	entHead:SetPlace(v3Part, qua.Product(q4Part, q4HeadRel))
	
	v3Part, q4Part = mshPilot:Socket("bodyhose"):PlaceEntity(entPilot, 1.0)
	local v3Connect = entHead:Mesh():Socket("maskhose"):PlaceEntity(entHead, 1.0)
	local v3HoseDif = v3Connect - v3Part
	local nHosePit, nHoseYaw = vec.PitchYaw(qua.VecRotInv(v3HoseDif, q4Part))
	q4Part = qua.Product(q4Part, qua.QuaPitchYaw(nHosePit, nHoseYaw))
	local entHose = self.entHose
	entHose:SetPlace(v3Part, q4Part)
	local nHoseDif = vec.Mag3(v3HoseDif)
	entHose:SetAnimation(self.aniHoseExtend, 14.0 * (nHoseDif / 0.7), nStep)
	
	-- Update other stuff
	self.voxGun1:SetPos(v3Pos)
	self.voxGun2:SetPos(v3Pos)
	
	self.v3PrevVel = v3Vel
	
	--self:ShowNoise()
	
end

--[[------------------------------------
	plr:Term

FIXME: delete child entities
--------------------------------------]]
function plr:Term()
	
end

--[[------------------------------------
	plr:AfterBrake

OUT	v3Vel, v3OriVel, zStability

Doesn't update anything, can be used for predictions. Should be called before OrientAttitude and
NewVelocity since this affects both rotational and displacement velocities (want effect to be
immediate).
--------------------------------------]]
function plr:AfterBrake(v3OldVel, v3OldOriVel, zStability, q4Ori, nThrust, nBrake, nStep)
	
	--[[
	if nBrake <= 0.0 then
		return v3OldVel, v3OldOriVel, zStability
	end
	]]
	
	nStep = nStep or wrp.TimeStep()
	
	-- Calculate some lift-related vars since braking requires smooth air
	local v3Fwd = qua.Dir(q4Ori)
	local v3Up = qua.Up(q4Ori)
	local nOldVel = vec.Mag3(v3OldVel)
	local v3BaseDir = v3OldVel
	
	if nOldVel > 0.0 then
		v3BaseDir = v3BaseDir / nOldVel
	end
	
	local nLiftUpDot = vec.Dot3(v3BaseDir, v3Up)
	local v3WingDir = vec.Normalized3(v3BaseDir - nLiftUpDot * v3Up)
	local nLiftFwdDot = vec.Dot3(v3WingDir, v3Fwd)
	
	-- Brake
	local nGoodSpeed = math.min(nOldVel / self.nGoodBrakeSpeed, 1.0)
	local nAngleOfAttack = math_abs(math.asin(com.Clamp(nLiftUpDot, -1, 1)))
	local nGoodAttack = 1.0 - com.Clamp((nAngleOfAttack - self.nGoodBrakeAngle - self.nGoodBrakeGroundAngle * self.nGroundEffect) / self.nGoodBrakeRange, 0, 1)
	local nBrakeEffect = nGoodAttack * nBrake * nGoodSpeed * nGoodSpeed
	local nBrakeVelEffect = nBrakeEffect * com.Lerp(1.0, 0.7, nThrust)
	local nBrakeOriEffect = nBrakeEffect * com.Lerp(1.0, 0.25, nThrust)
	local nFwdDotNorm = (nLiftFwdDot + 1.0) * 0.5
	
	nBrakeVelEffect = nBrakeVelEffect * nFwdDotNorm * nFwdDotNorm
	local v3Vel = phy.Friction(v3OldVel, 1.5 * nBrakeVelEffect, nil, 100, nStep)
	
	nBrakeOriEffect = nBrakeOriEffect * nFwdDotNorm --com.Clamp(nLiftFwdDot, 0, 1)
	local v3Axis, nMaxAngle = vec.DirDelta(v3WingDir, v3Fwd)
		-- FIXME: the relative axis is always z = 1, calling DirDelta is expensive just to figure out whether we're facing left or right
	--local nAngSpeed = math.min(nMaxAngle, 1.0) * nBrakeOriEffect * 25.0
	--local nAngSpeed = 25.0 * nBrakeOriEffect * math.min(nMaxAngle, 0.2) / 0.2
	local nAngSpeed = 25.0 * nBrakeOriEffect * (1.0 - com.Clamp((nLiftFwdDot - 0.985) / 0.015, 0, 1))
	local nAngDelta = nAngSpeed * nStep
	local v3RelAxis = qua.VecRotInv(v3Axis, q4Ori)
	local v3OriVel = v3OldOriVel + v3RelAxis * nAngDelta
	
	--local nGoodStability = nGoodAttack * com.Clamp(nLiftFwdDot, 0, 1)
	--local zNewStability = com.Lerp(com.Lerp(1.0, com.Lerp(0.4, 0.8, nThrust), nBrake), 1.0, nGoodStability)
	local zNewStability = math.max(0.7, zStability - nAngDelta * 0.5)
	--local zNewStability = com.Lerp(1.0, com.Lerp(0.4, 0.8, nThrust), nBrake) -- Fun with 15 nAngSpeed and slow stability recovery
	--local zNewStability = 1.0 -- FIXME TEMP
	
	-- FIXME HACK: Thrust torque (rename this function or put in OrientAttitude [which would make it based on post-brake velocity] or in a new function)
	local v3Left = qua.Left(q4Ori)
	local nLeftDot = vec.Dot3(v3BaseDir, v3Left)
	local nThrustDot = nLeftDot
	nThrustDot = com.Approach(nThrustDot, 0.0, 0.3) / 0.7
	--nThrustDot = nThrustDot * math_abs(nThrustDot)
	local nThrustOriDel = -nThrustDot * com.Clamp(nThrust - nBrake, 0, 1) * nOldVel * 0.007 * nStep
	zOriVel = zOriVel + nThrustOriDel
	
	return v3Vel, v3OriVel, math.min(zStability, zNewStability)
	
end

--[[------------------------------------
	plr:OrientAttitude

OUT	nAddPit, nAddYaw

FIXME: Disabled fly-by-wire mode:
	Only roll and pitch control
	Static instability; rotation velocity does not dissipate
		Should lift create pitch, too? Player would have to pitch down constantly
	Disable rebound

FIXME: Add turbulence inside and near clouds
FIXME: Add turbulence inside and near explosions

FIXME: Reduce general turbulence with altitude

FIXME: Increase probe distance with speed?
--------------------------------------]]
local tProbesTemp = {}
local tProbeDirsTemp = {}

function plr:OrientAttitude(bControl, bAimingLook, nThrust, nBrake, v3Vel)
	
	local nStep = wrp.TimeStep()
	local nInRol = 0 -- [-1, 1]
	local nAddPit, nAddYaw = 0, 0 -- [-inf, inf], radians
	local v3VelDir = vec.Normalized3(v3Vel)
	local q4Ori = self:Ori()
	local v3Trimmed = v2Vel, zVel + self.nTrim -- Constant creates approximate trim for maintaining attitude in zero-roll flight
	local v3TrimNorm = vec.Normalized3(v3Trimmed)
	local v3Fwd1 = qua.Dir(q4Ori)
	
	--[[
	-- Modify orientation ability based on current orientation (less effective control surfaces at extreme angles)
		--[=[ FIXME: This feels cool but if it's going to be implemented, it needs to impose
		actual limitations rather than only sensitivity changes so it doesn't feel like the game
		is just messing with the player's controls. Flying is hard enough already, not sure more
		time should be spent on this. If all euler controls get limits like yaw does now, this
		might be worth revisiting ]=]
	local nFlyAngle = com.SafeArcCos(vec.Dot3(v3VelDir, v3Fwd1))
	local nFlyStraight = 1.0 - com.Clamp((nFlyAngle - 0.33161255787892261961550124601284) / 1.2391837689159739996158204456269, 0, 1)
	local nOrientStability = com.Lerp(0.8, 1.0, nFlyStraight)
		-- FIXME: Reducing stability like this messes with lift turns, especially when a partial yaw increases nFlyAngle
			-- Try reducing sensitivity only, but that might interfere with the feel of stall resistance too
	--gcon.LogF("stab %g", nOrientStability)
	local nOrientSensitivity = com.Lerp(0.5, 1.0, nFlyStraight)
	gcon.LogF("sens %g", nOrientSensitivity)
	]]
	
	local nFlyAngle = com.SafeArcCos(vec.Dot3(v3VelDir, v3Fwd1))
	local nFlyStraight = 1.0 - com.Clamp((nFlyAngle - 0.2) / 1.3707963267948966192313216916398, 0, 1)
	local zOrientStability = com.Lerp(0.4, 1.0, nFlyStraight * nFlyStraight)
	local zOrientAbility = zOrientStability --com.Lerp(0.4, 1.0, nFlyStraight * nFlyStraight)
	
	-- Reduce delta/cache if brake strength increased
	--[[ FIXME: Is there a way to generalize this, at least for torque caching?
		zOrientAbility changes could use cache modification too, but maybe there's a way to avoid having to manage it
			Cache time for full right or left yaw acceleration based on sensitivity/acc ratio?
				If less time cached than nStep, that's a partial acceleration
	]]
	-- FIXME: Test how this feels with analog brake trigger
	local BRAKE_YAW_DECREASE = 0.7
	local nBrakeTurn = 1.0 - com.Clamp(nBrake - nThrust * 0.45, 0, 1) * BRAKE_YAW_DECREASE
	local nPrevBrakeTurn = self.nPrevBrakeTurn
	self.nPrevBrakeTurn = nBrakeTurn
	
	-- FIXME: make bBrakeReduceYawCache a factor Number that determines the strength of this effect?
	if self.bBrakeReduceYawCache and nBrakeTurn < nPrevBrakeTurn then
		
		local nMul = nPrevBrakeTurn ~= 0.0 and nBrakeTurn / nPrevBrakeTurn or 1.0
		self.nYawCache = self.nYawCache * nMul
		self.nYawTimeCache = self.nYawTimeCache * nMul
		--[[local nOldYawDelta = self.nYawDelta
		self.nYawDelta = self.nYawDelta * nMul -- Feels bad
		self.nYawCamTemp = self.nYawCamTemp + (nOldYawDelta - self.nYawDelta)]]
		self.nYawDeltaVel = self.nYawDeltaVel * nMul
		self.nYawFollowVel = self.nYawFollowVel * nMul
		
	end
	
	-- Handle orientation inputs
	local v3CurStability = self.v3Stability
	zCurStability = zCurStability * zOrientStability
	local v3OriFric = v3CurStability * 6.0
	local nYawAccLimit = 18 * nBrakeTurn * zOrientAbility
	local nYawAddLimit = self.bCheatUnlimitedYaw and math.huge or nYawAccLimit * nStep
	
	-- FIXME: implement analog actions so joystick can be used, sensitivity can be adjusted, etc.
	
	if bControl then
		
		if key.Down(ACT_ROLL_CW) then nInRol = nInRol + 1.0 end -- FIXME: analog actions that are clamped here
		if key.Down(ACT_ROLL_CCW) then nInRol = nInRol - 1.0 end
		
		local v2Mouse = key.MouseTickDelta()
		
		if not bAimingLook then
			
			local nPitInput = yMouse + ((key.Down(ACT_PITCH_DOWN) and 1.0 or 0.0) - (key.Down(ACT_PITCH_UP) and 1.0 or 0.0))
				-- FIXME TEMP: using this until generic pitch action
			nAddPit = nPitInput * DEG_TO_RAD * self.nPitSensitivity
			local nYawInput = -xMouse + ((key.Down(ACT_YAW_LEFT) and 1.0 or 0.0) - (key.Down(ACT_YAW_RIGHT) and 1.0 or 0.0))
				-- FIXME TEMP: using this until generic yaw action
			local nWantAddYaw = nYawInput * DEG_TO_RAD * self.nYawSensitivity * nBrakeTurn * zOrientAbility
				--[[ FIXME: Should delta cache not have any sensitivity changes since it's based on ship orientation, not cam orientation?
					Maybe it should have emulated orientation physics: sensitivity and its own stability based on current cache vs velocity direction
				]]
			local iYawCacheType = self.iYawCacheType
			
			if iYawCacheType <= 1 then
				nAddYaw = nWantAddYaw
			elseif iYawCacheType == 2 then
				
				-- FIXME: make canceling use generic action input value
				if math_abs(nYawInput) > self.nCancelYawCacheThreshold and
				(nYawInput >= 0) ~= (self.nYawCache >= 0) then
					self.nYawCache = 0 -- Cancel current cache
				end
				
				self.nYawCache = self.nYawCache + nWantAddYaw
				
			elseif iYawCacheType == 3 then
				
				if math_abs(nYawInput) > self.nCancelYawCacheThreshold and
				(nYawInput >= 0) ~= (self.nYawTimeCache >= 0) then
					self.nYawTimeCache = 0
				end
				
				if nYawAccLimit > 0.0 and nYawAddLimit < math.huge then
					self.nYawTimeCache = self.nYawTimeCache + nWantAddYaw / nYawAccLimit
				else
					nAddYaw = nWantAddYaw
				end
				
			else -- if iYawCacheType >= 4 then
				self.nYawDeltaVel = self.nYawDeltaVel + nWantAddYaw
			end
			
		end
		
	end
	
	-- Note: It turns out torque caching and delta caching are equivalent when stability is not changing
	local nCacheTake = com.ClampMag(self.nYawCache, nYawAddLimit)
	self.nYawCache = self.nYawCache - nCacheTake
	nAddYaw = nAddYaw + nCacheTake
	
	local nTimeCacheTake = com.ClampMag(self.nYawTimeCache, nStep)
	self.nYawTimeCache = self.nYawTimeCache - nTimeCacheTake
	nAddYaw = nAddYaw + nYawAccLimit * nTimeCacheTake
	
	self.nYawDelta = self.nYawDelta + self.nYawDeltaVel * nStep
	self.nYawDeltaVel = com.Smooth(self.nYawDeltaVel, 0.0, zOriFric)
	local nWantFollow = zOriFric * self.nYawDelta + self.nYawDeltaVel
	local nOldFollowVel = self.nYawFollowVel
	self.nYawFollowVel = com.Approach(nOldFollowVel, nWantFollow, nYawAddLimit)
	nAddYaw = nAddYaw + self.nYawFollowVel - nOldFollowVel
	self.nYawDelta = self.nYawDelta - self.nYawFollowVel * nStep
	self.nYawFollowVel = com.Smooth(self.nYawFollowVel, 0.0, zOriFric)
	
	self.xOriVel = self.xOriVel + nInRol * 20.0 * nStep
	self.yOriVel = self.yOriVel + nAddPit
	self.zOriVel = self.zOriVel + com.ClampMag(nAddYaw, nYawAddLimit)
	
	-- Resist additions to angle of attack so it's harder to stall accidentally
	local v3Up1 = qua.Up(q4Ori)
	local nAttackFac, nYawAttackFac, nLiftUpDot = self:LiftEfficiency(v3TrimNorm, v3Up1, v3Fwd1)
	local v3LiftAxis = vec.Normalized3(vec.Cross(v3Up1, v3TrimNorm))
	local v3LiftAxisRel = qua.VecRotInv(v3LiftAxis, q4Ori)
	
	if nLiftUpDot <= 0.0 then
		
		local nLiftFac = nAttackFac * nYawAttackFac
		local nBaseResist = nLiftFac * 6.0 * nStep -- FIXME: stall resistance option?
		local v2Resist = self.v2OriVel
		if (xLiftAxisRel >= 0.0) == (xResist >= 0.0) then xResist = 0.0 else xResist = com.ClampMag(xResist * math_abs(xLiftAxisRel), nBaseResist) end
		if (yLiftAxisRel >= 0.0) == (yResist >= 0.0) then yResist = 0.0 else yResist = com.ClampMag(yResist * math_abs(yLiftAxisRel), nBaseResist) end
		self.v2OriVel = self.v2OriVel - v2Resist * v2CurStability
		
	end
	
	-- Apply angular velocity
	local q4Add = qua.QuaAngularVelocity(self.v3OriVel, nStep)
	q4Ori = qua.Product(q4Ori, q4Add)
	
	-- Stabilize angular velocity
	self.xOriVel = com.Smooth(self.xOriVel, 0.0, xOriFric, 0.1 * xCurStability)
	self.yOriVel = com.Smooth(self.yOriVel, 0.0, yOriFric)
	self.zOriVel = com.Smooth(self.zOriVel, 0.0, zOriFric)
	self.xStability = com.Approach(self.xStability, 1.0, 0.3 * nStep)
	self.yStability = com.Approach(self.yStability, 1.0, 0.3 * nStep)
	self.zStability = com.Approach(self.zStability, 1.0, 0.3 * nStep)
	
	-- Add/subtract some jolt pitch if movement input has changed
	local v3TempFwd = qua.Dir(q4Ori)
	local nJoltTar = nBrake * math.max(0.0, vec.Dot3(v3TempFwd, vec.Normalized3(v3Vel))) * 0.05 - nThrust * 0.02
	local nJoltDel
	self.nJoltAdded, nJoltDel = com.Smooth(self.nJoltAdded, nJoltTar, 14.0, nil, 0.03)
	q4Ori = qua.Product(q4Ori, qua.QuaPitchYaw(nJoltDel, 0.0))
	
	-- Provide feedback on velocity direction using orientation
		--[[ Real artificial stability would try to keep the attitude set by the pilot, but this
		feels cool ]]
	local v3PullAxis, nPullAngle = qua.LookDeltaAxisAngle(q4Ori, v3Trimmed)
	local nVel = vec.Mag3(v3Vel)
	
		-- Realign forward axis with velocity
	v3TempFwd = qua.Dir(q4Ori)
	local nReboundDot = vec.Dot3(v3TempFwd, v3TrimNorm)
	local nReboundAngle = nPullAngle
	
	if nReboundDot < 0.0 then
		
		nReboundDot = -nReboundDot
		nReboundAngle = nReboundAngle - math.pi
		
	end
	
	--[[ FIXME: bad, remove
	local nFwdAngle = math.acos(com.Clamp(vec.Dot3(v3TempFwd, v3VelDir), 0, 1))
	local nMore = com.Clamp((nFwdAngle - 0.13) / 0.2, 0, 1)
	local nLess = com.Clamp((nFwdAngle - 0.33) / 0.4, 0, 1)
	local nReboundFactor = 0.002 + (nMore * nMore - nLess * nLess) * 0.005
	]]
	
	local _
	_, nReboundAngle = com.Smooth(-nReboundAngle, 0.0, nReboundDot * nVel * 0.002)
	local q4Feedback = qua.QuaAxisAngle(v3PullAxis, nReboundAngle)
	q4Ori = qua.Product(q4Feedback, q4Ori)
	
	-- React to scraping a surface
	local v3Surface = self.v3AlignSurface
	
	if xSurface then
		
		local v3SurfaceRel = qua.VecRotInv(v3Surface, q4Ori)
		local v3FlatAxis = vec.Normalized3(vec.Cross(0, 0, 1, v3SurfaceRel))
		local nFlatAngle = com.SafeArcCos(zSurfaceRel)
		
		if zSurfaceRel < 0.0 then
			v3FlatAxis, nFlatAngle = -v3FlatAxis, math.pi - nFlatAngle
		end
		
		local q4Flat = qua.QuaAxisAngle(v3FlatAxis, math.min(nFlatAngle, nFlatAngle * 2.0 * nStep))
		q4Ori = qua.Product(q4Ori, q4Flat)
		self.v3AlignSurface = nil, nil, nil
		
	end
	
	-- Turbulence
	local v3Turb = self.v3Turb
	local v3TurbPrev = v3Turb
	local v3Pos = self:Pos()
	local SIMPLEX_SCALE = 0.015
	v3Turb = com.SimplexNoise3(v3Pos * SIMPLEX_SCALE, 0.57), com.SimplexNoise3(v3Pos * SIMPLEX_SCALE, 0.95), com.SimplexNoise3(v3Pos * SIMPLEX_SCALE, 0.27)
	local nTurbGeneral = com.Clamp((com.SimplexNoise3(v3Pos * 0.005, 0.13) / 0.3 + 1.0) * 0.5, 0, 1) * 0.007
	
	local PROBE_DIST = 100
	local CLOSE_PROBE_POW, CLOSE_PROBE_FAC, CLOSE_PROBE_SUB = 3.0, 0.01, 0.4
	local MISS_PROBE_POW, MISS_PROBE_FAC = 2.0, 0.02
	local MAX_TURB_FAC = 0.03
	local v3Left = qua.Left(q4Ori)
	local v3Up2 = qua.Up(q4Ori)
	local nVelUpDot = vec.Dot3(v3VelDir, v3Up2)
	local v3VelUp = vec.Normalized3(v3Up2 - v3VelDir * nVelUpDot)
	local v3VelLeft = vec.Normalized3(vec.Cross(v3Up2, v3VelDir))
	local v3VelUpLeft = (v3VelUp + v3VelLeft) * 0.70710678118654752440084436210485
	local v3VelUpRight = (v3VelUp - v3VelLeft) * 0.70710678118654752440084436210485
	local v3VelLeftAbs = math_abs(xVelLeft), math_abs(yVelLeft), math_abs(zVelLeft)
	local v3VelUpAbs = math_abs(xVelUp), math_abs(yVelUp), math_abs(zVelUp)
	local TurbProbe = self.TurbProbe
	
	tProbeDirsTemp[1], tProbeDirsTemp[2], tProbeDirsTemp[3] = -v3VelLeft
	tProbeDirsTemp[4], tProbeDirsTemp[5], tProbeDirsTemp[6] = v3VelUpRight
	tProbeDirsTemp[7], tProbeDirsTemp[8], tProbeDirsTemp[9] = v3VelUp
	tProbeDirsTemp[10], tProbeDirsTemp[11], tProbeDirsTemp[12] = v3VelUpLeft
	tProbeDirsTemp[13], tProbeDirsTemp[14], tProbeDirsTemp[15] = v3VelLeft
	tProbeDirsTemp[16], tProbeDirsTemp[17], tProbeDirsTemp[18] = -v3VelUpRight
	tProbeDirsTemp[19], tProbeDirsTemp[20], tProbeDirsTemp[21] = -v3VelUp
	tProbeDirsTemp[22], tProbeDirsTemp[23], tProbeDirsTemp[24] = -v3VelUpLeft
	
	local tProbes = self.tProbes
	local nTurbCloseTar = 0.0
	local nTurbMissReserve = self.nTurbMissReserve
	
	for i = 1, 8 do
		
		local iStart = (i - 1) * 3
		local nProbe = TurbProbe(self, v3Pos, tProbeDirsTemp[iStart + 1], tProbeDirsTemp[iStart + 2], tProbeDirsTemp[iStart + 3], PROBE_DIST)
		nProbe = nProbe
		tProbesTemp[i] = nProbe
		local nDif = math.max(0.0, (nProbe ^ MISS_PROBE_POW - tProbes[i] ^ MISS_PROBE_POW) * MISS_PROBE_FAC)
		
		if nDif / nStep > 0.08 then
			nTurbMissReserve = nTurbMissReserve + nDif
		end
		
		tProbes[i] = nProbe
		
	end
	
	for i = 1, 4 do
		
		nTurbCloseTar = nTurbCloseTar + CLOSE_PROBE_FAC * math.abs(
				math.max(0, (tProbesTemp[i] - CLOSE_PROBE_SUB) / (1 - CLOSE_PROBE_SUB)) ^ CLOSE_PROBE_POW -
				math.max(0, (tProbesTemp[i + 4] - CLOSE_PROBE_SUB) / (1 - CLOSE_PROBE_SUB)) ^ CLOSE_PROBE_POW
			)
		
	end
	
	local nTurbClose = self.nTurbClose
	nTurbClose = com.UnevenApproach(nTurbClose, nTurbCloseTar, 0.5 * nStep, 0.1 * nStep)
	self.nTurbClose = nTurbClose
	
	local nTurbMiss = self.nTurbMiss
	local TURB_MISS_LIMIT = MISS_PROBE_FAC
	local TURB_MISS_RESERVE_LIMIT = 0.1
	
	if nTurbMissReserve > 0.0 then
		
		local nAddReserve = math.min(0.38 * nStep, nTurbMissReserve)
		nTurbMiss = nTurbMiss + nAddReserve
		nTurbMissReserve = nTurbMissReserve - nAddReserve
		
	end
	
	if nTurbMiss > TURB_MISS_LIMIT then
		nTurbMiss = TURB_MISS_LIMIT
	end
	
	if nTurbMissReserve > TURB_MISS_RESERVE_LIMIT then
		nTurbMissReserve = TURB_MISS_RESERVE_LIMIT
	end
	
	self.nTurbMissReserve = nTurbMissReserve
	local nTurbFac = math.min(nTurbGeneral + nTurbClose + nTurbMiss, MAX_TURB_FAC)
	v3Turb = v3Turb * nTurbFac
	self.v3Turb = v3Turb
	local q4TurbDel = qua.QuaAngularVelocity(v3Turb - v3TurbPrev)
	q4Ori = qua.Product(q4TurbDel, q4Ori)
	
	nTurbMiss = com.Smooth(nTurbMiss, 0, 1.5, 0.01)
	self.nTurbMiss = nTurbMiss
	
	-- Add/subtract some ground effect AoA
	--[[
	local nGroundTar = self.nGroundEffect * -0.1
	
	local nGroundAdded, nGroundDel = com.Smooth(self.nGroundAdded, nGroundTar, 4.0, nil, nil,
		nStep)
	
	self.nGroundAdded = nGroundAdded
	local q4GroundDel = qua.QuaAxisAngle(v3VelLeft, nGroundDel)
	q4Ori = qua.Product(q4GroundDel, q4Ori)
	]]
	
	-- Done
	self:SetOri(q4Ori)
	return nAddPit, nAddYaw
	
end

--[[------------------------------------
	plr:TurbProbe

OUT	nTurbFrac
--------------------------------------]]
function plr:TurbProbe(v3Pos, v3ProbeDir, nProbeDist)
	
	local iC, nTF = hit.HullTest(self.hulTurb, v3Pos, v3Pos + nProbeDist * v3ProbeDir, 0, 0, 0, 1, hit.ALL, self.fsetIgnore, self)
	return iC == hit.NONE and 0.0 or (iC == hit.HIT and (1.0 - nTF) or 1.0)
	
end

--[[------------------------------------
	plr:NewVelocity

OUT	v3Vel, nGoodLift, nGoodAOA, nLiftFwdDot, nLiftUpDot, v3BefThrust, v3AftThrust

This function doesn't update anything so it can be used to get a velocity prediction. Also call
AfterBrake beforehand.

nGoodLift [0, 1] is used for visuals/sounds to indicate whether the player is stalling. A value
of 1 means air is passing the wings smoothly, but not necessarily that the player actually has
lift (e.g. flying upside down). Likewise, a lower value doesn't mean the player isn't cancelling
out gravity (e.g. gliding almost backwards with a good angle of attack).

nGoodAOA [0, 1] approaches 0 as the angle of attack approaches the stall threshold.
--------------------------------------]]
function plr:NewVelocity(v3OldVel, q4Ori, nThrust, nBrake, nStep)
	
	nStep = nStep or wrp.TimeStep()
	local v3Pos = EntPos(self)
	local v3OldPos = v3Pos
	local v3Vel = v3OldVel
	local nOldVel = vec.Mag3(v3OldVel)
	local v3OldDir = v3OldVel
	
	if nOldVel > 0.0 then
		v3OldDir = v3OldVel / nOldVel
	end
	
	local v3OldRel = qua.VecRotInv(v3OldVel, q4Ori)
	
	-- Get direction vectors
	local v3Fwd = qua.Dir(q4Ori)
	local v3Side = qua.VecRot(0, 1, 0, q4Ori)
	local v3Up = qua.VecRot(0, 0, 1, q4Ori)
	
	local v3Thrust = v3Fwd * nThrust
	
	-- Altitude fraction
	-- FIXME: make more effective at limiting altitude
	local MAX_ALTITUDE = 27000.0
	--local nAltitudeFrac = (MAX_ALTITUDE - com.Clamp(zPos, 0.0, MAX_ALTITUDE)) / MAX_ALTITUDE
	local nAltitudeFrac = 1.0
	--gcon.LogF("altitude frac %g", nAltitudeFrac)
	
	-- Drag deceleration
	local v3RelNorm = v3OldRel
	
	if nOldVel > 0.0 then
		v3RelNorm = v3RelNorm / nOldVel
	end
	
	local nDrag = 0.01 -- Base air resistance
	
	-- Sound barrier
	nDrag = nDrag
		+ 0.035 * com.Clamp((nOldVel - 330) / 13, 0, 1)
		+ 0.04 * com.Clamp((nOldVel - 370) / 18, 0, 1)
		- 0.06 * com.Clamp((nOldVel - 390) / 20, 0, 1)
	
	-- Reduce drag during ground effect
	local nGroundEffect = self.nGroundEffect
	local nDragReduce = math.min(nDrag * 0.5 * nGroundEffect, 0.02)
	nDrag = nDrag - nDragReduce
	
	local nDec
	v3Vel, nDec = phy.Friction(v3Vel, nDrag, nil, nil, nStep)
	
	-- Apply gravity
	local nBefGravity = vec.Mag3(v3Vel)
	zVel = zVel + phy.zGravity * nStep
	
	-- Calculate "base clamp" that limits gravity's acceleration ability at high speeds so player can't fall thru sound barrier
	local nGravResist = 0.87 * com.Clamp((nBefGravity - 310) / 33, 0, 1)
	local nAftGravity = vec.Mag3(v3Vel)
	local nNoAccelGrav = com.Lerp(nBefGravity, math.min(nBefGravity, nAftGravity), math.max(0, zOldDir))
		-- Also limit gravity's deceleration if not going straight up so there isn't a big loss of energy when going up and then down
			-- Important to prevent speed loss during 45-deg-roll lift-turns
	local nBaseClamp = nGravResist == 0.0 and nAftGravity or com.Lerp(nAftGravity, nNoAccelGrav, nGravResist)
	
	--[[ Thrust; unrealistic model that feels good to play but is affected by "drag" (speed
	limit) and creates its own "lift" (caused by spherical speed clamp) ]]
	
	local nGoodBrake = com.Lerp(1.0, math.min(nOldVel / self.nGoodBrakeSpeed, 1.0), nBrake)
	local v3BefThrust = v3Vel
	local nThrustAnalog = math.min(1.0, vec.Mag3(v3Thrust))
	local nThrustInitMul = nAltitudeFrac * nThrustAnalog * nGoodBrake * nGoodBrake
	local nThrustMul = nThrustInitMul + 0.15 * nGroundEffect
		-- Separate from InitMul so ground effect doesn't affect thrusters' turn ability
	
	v3Vel = phy.SprintThrust(v3Vel, nBaseClamp, v3Thrust, 1.0,
		math.max(0.0, 1.0 - math_abs(yRelNorm)) ^ 2, 200, nStep,
		320 * nThrustInitMul, 10,
		
		150 * nThrustMul, 100,
		90 * nThrustMul, 200,
		50 * nThrustMul, 245,
		36 * nThrustMul, 310,
		24 * nThrustMul, 410,
		14 * nThrustMul
		)
	
	local v3AftThrust = v3Vel
	
	--[[ Lift remainder; calculate more realistic lift and only apply whatever the thrust model
	did not provide. Allows player to glide and turn with more control. ]]
	
	local v3LiftBase = v3OldVel
	local nLiftBase = nOldVel -- Squared in real life, this feels better
	local v3BaseDir = v3OldDir
	
	-- Calculate lift coefficient based on altitude, angle of attack, velocity's relative yaw
	local nAttackFac, nYawAttackFac, nLiftUpDot, nLiftFwdDot =
		self:LiftEfficiency(v3BaseDir, v3Up, v3Fwd)
	
	local nLiftCoef = nAltitudeFrac * nAttackFac * nYawAttackFac *
		com.Clamp((nGoodBrake - 0.8) / 0.2, 0, 1)
		-- FIXME: test if the 0.2 range for brake-lift cancelling feels right on controllers
	
	-- Lift direction is ship's up perpendicular to velocity
	local v3LiftDir = vec.Normalized3(v3Up - v3BaseDir * nLiftUpDot)
	
	--[[ Get thrust's lift: acceleration perpendicular to wanted thrust direction, projected
	onto lift direction ]]
	local v3ThrustDif = v3Vel - v3BefThrust
	local v3ThrustDir = vec.Normalized3(v3Thrust)
	local nWantedThrust = vec.Dot3(v3ThrustDif, v3ThrustDir)
	local v3PerpThrust = v3ThrustDif - v3ThrustDir * nWantedThrust
	local nThrustLift = vec.Dot3(v3LiftDir, v3PerpThrust)
	
	-- Add lift remainder
	local nLiftAcc = nLiftCoef * nLiftBase * (1.0 + nGroundEffect * 1.0)
	local nLift = nLiftAcc * nStep
	local nRemainder = 0.0
	
	if nLift > 0.0 then
		nRemainder = math.max(0.0, nLift - math.max(0.0, nThrustLift))
	elseif nLift < 0.0 then
		nRemainder = math.min(nLift - math.min(nThrustLift, 0.0), 0.0)
	end
	
	v3Vel = v3Vel + v3LiftDir * nRemainder
	
	--[[ Sideforce remainder; like lift but yaw makes the angle of attack ]]
	
	local v3Left = qua.Left(q4Ori)
	local nSideAttackFac, nLiftLeftDot = self:SideForceEfficiency(v3BaseDir, v3Left, v3Fwd)
	local nSideCoef = 0.1 * nAltitudeFrac * nSideAttackFac
	local v3SideDir = vec.Normalized3(v3Left - v3BaseDir * nLiftLeftDot)
	local nThrustSide = vec.Dot3(v3SideDir, v3PerpThrust)
	local nSideAcc = nSideCoef * nLiftBase
	local nSide = nSideAcc * nStep
	local nSideRemainder = 0.0
	
	if nSide > 0.0 then
		nSideRemainder = math.max(0.0, nSide - math.max(0.0, nThrustSide))
	elseif nSide < 0.0 then
		nSideRemainder = math.min(nSide - math.min(nThrustSide, 0.0), 0.0)
	end
	
	v3Vel = v3Vel + v3SideDir * nSideRemainder
	
	-- Calculate nGoodLift
	local nFakeLiftUpDot = nLiftUpDot
	
	if nLiftBase > 0.0 then
		
		local nExtraLift
		
		if nLift > 0.0 then
			nExtraLift = math.max(0.0, math.max(0.0, nThrustLift) - nLift)
		elseif nLift < 0.0 then
			nExtraLift = math.min(math.min(nThrustLift, 0.0) - nLift, 0.0)
		else
			nExtraLift = nThrustLift
		end
		
		nExtraLift = math_abs(nExtraLift)
		local nExtraLiftDot = nExtraLift / nStep / nLiftBase / 3.0
		nFakeLiftUpDot = com.Approach(nFakeLiftUpDot, 0.0, nExtraLiftDot)
			--[[ Act like angle of attack is reduced because thrust is doing some lift work by
			itself, so the angle of attack "limit" is effectively more tolerant ]]
		
	end
	
	local nFakeAngleOfAttack = math_abs(math.asin(com.Clamp(nFakeLiftUpDot, -1, 1)))
	
	local nGoodLift = 1.0 - com.Clamp((nFakeAngleOfAttack - self.nGoodLiftAngle - self.nGoodLiftGroundAngle * nGroundEffect) / self.nGoodLiftRange, 0, 1)
	
	if nLiftFwdDot < 0.0 then
		nGoodLift = nGoodLift * (1.0 + nLiftFwdDot)
	end
	
	if nGoodLift < 1.0 then
		nGoodLift = 1.0 + (nGoodLift - 1.0) * math.min(nOldVel / 200.0, 1.0)
	end
	
	local nAngleOfAttack = math_abs(math.asin(com.Clamp(nLiftUpDot, -1, 1)))
	local nGoodAOA = 1.0 - com.Clamp(nAngleOfAttack / self.nGoodLiftAngle, 0, 1)
	
	return v3Vel, nGoodLift, nGoodAOA, nLiftFwdDot, nLiftUpDot, v3BefThrust, v3AftThrust
	
end

--[[------------------------------------
	plr:LiftEfficiency

OUT	nAttackFac, nYawAttackFac, nLiftUpDot, nLiftFwdDot

Returns lift coefficient factors based on angle of attack and velocity's relative yaw. Also
returns some useful intermediary calculations.
--------------------------------------]]
function plr:LiftEfficiency(v3BaseDir, v3Up, v3Fwd)
	
	local nLiftUpDot = vec.Dot3(v3BaseDir, v3Up)
	local v3WingDir = vec.Normalized3(v3BaseDir - nLiftUpDot * v3Up) -- Project onto wing plane
	local nLiftFwdDot = vec.Dot3(v3WingDir, v3Fwd)
	local nAttackFac
	
	if nLiftUpDot <= 0.0 then
		
		-- Air is hitting bottom of wings
		-- ~19 degrees up makes an nAttackFac close to 1.0, "perfect" lift
		-- Commented out code has ground effect add ~10 degrees to limit
			-- In reality, ground effect makes stall angle smaller, but that would be annoying
		local nLimit = 1 -- + 0.44 * nGroundEffect
		
		nAttackFac = com.Clamp(-nLiftUpDot * 3.0, 0, nLimit) -
			com.Clamp((-nLiftUpDot - 0.33 --[[- 0.15 * nGroundEffect]]) * 3.0, 0, nLimit)
		
	else
		
		-- Air is hitting top of wings
		nAttackFac = 0.0 -- No downward lift
			-- Makes shooting at ground easier, ship isn't instantly pulled downward
				-- Prevents similar problem aiming up while going backwards
					-- ...if player were to have lift when going backwards
			-- Makes upside down flight harder and upside down gliding impossible
		
		--[[
		nAttackFac = com.Clamp(-nLiftUpDot * 3.0, -1, 0) -
			com.Clamp((-nLiftUpDot + 0.33) * 3.0, -1, 0)
		--]]
		
	end
	
	local LIFT_FAC_FWD = 1.0
	local LIFT_FAC_BACK = 0.0
	local LIFT_FAC_SIDE = 0.5
	local nYawAttackFac
	
	if nLiftFwdDot >= 0.0 then
		nYawAttackFac = LIFT_FAC_SIDE + (LIFT_FAC_FWD - LIFT_FAC_SIDE) * nLiftFwdDot
	else
		nYawAttackFac = LIFT_FAC_SIDE + (LIFT_FAC_BACK - LIFT_FAC_SIDE) * -nLiftFwdDot
	end
	
	return nAttackFac, nYawAttackFac, nLiftUpDot, nLiftFwdDot
	
end

--[[------------------------------------
	plr:SideForceEfficiency

OUT	nSideAttackFac, nLiftLeftDot
--------------------------------------]]
function plr:SideForceEfficiency(v3BaseDir, v3Left, v3Fwd)
	
	local nLiftLeftDot = vec.Dot3(v3BaseDir, v3Left)
	local v3PitchDir = vec.Normalized3(v3BaseDir - nLiftLeftDot * v3Left) -- Project onto pitch plane
	local nFwdDot = vec.Dot3(v3PitchDir, v3Fwd)
	local nSideAttackFac
	local MULTIPLIER, SUBTRAHEND, LOSE_MUL = 1.3691296853329244199339805665732, 0.730391, 3.8
		-- Roughly matches the peak of orientation rebound
	
	if nLiftLeftDot >= 0.0 then
		
		-- Air is hitting left side of aircraft
		nSideAttackFac = com.Clamp(-nLiftLeftDot * MULTIPLIER, -1, 0) -
			com.Clamp((-nLiftLeftDot + SUBTRAHEND) * LOSE_MUL, -1, 0)
			
	else
		
		-- Air is hitting right side of aircraft
		nSideAttackFac = com.Clamp(-nLiftLeftDot * MULTIPLIER, 0, 1) -
			com.Clamp((-nLiftLeftDot - SUBTRAHEND) * LOSE_MUL, 0, 1)
		
	end
	
	-- Reduce sideforce as air hits top or bottom of aircraft
	nSideAttackFac = nSideAttackFac * math.max(0.0, nFwdDot)
	
	return nSideAttackFac, nLiftLeftDot
	
end

--[[------------------------------------
	plr:OrientAimLook
--------------------------------------]]
function plr:OrientAimLook(bAimingLook)
	
	-- Orient look
	local nAimLookRol, nAimLookPit, nAimLookYaw = self.nAimLookRol, self.nAimLookPit, self.nAimLookYaw
	
	if bAimingLook then
		
		local v2Mouse = key.MouseTickDelta()
		local nFac = DEG_TO_RAD * self.nLookSensitivity
		local nLookAddPit, nLookAddYaw = yMouse * nFac, -xMouse * nFac
		nAimLookPit, nAimLookYaw = nAimLookPit + nLookAddPit, nAimLookYaw + nLookAddYaw
		
		-- FIXME: clamping feels lame, hide body when its pose doesn't make sense instead?
			-- FIXME: or move camera to in front of head
		nAimLookPit = com.Clamp(nAimLookPit, -math.pi * 0.5, math.max(0.0, 1.2 - math_abs(com.WrapAngle(nAimLookYaw)) * 0.4))
		
	elseif nAimLookRol ~= 0.0 or nAimLookPit ~= 0.0 or nAimLookYaw ~= 0.0 then
		
		nAimLookRol, nAimLookPit, nAimLookYaw = com.SmoothResetEuler(com.WrapAngle(nAimLookRol),
			com.WrapAngle(nAimLookPit), com.WrapAngle(nAimLookYaw), 10, 0.02)
		
	end
	
	self.nAimLookRol, self.nAimLookPit, self.nAimLookYaw = nAimLookRol, nAimLookPit, nAimLookYaw
	
end

--[[------------------------------------
	plr:OrientDriftLook
--------------------------------------]]
function plr:OrientDriftLook(v3Vel, q4Ori)
	
	-- Orient drift look
	local v3VelRel = qua.VecRotInv(v3Vel, q4Ori)
	local nDriftLookPit, nDriftLookYaw = self.nDriftLookPit, self.nDriftLookYaw
	local nDriftPitTar, nDriftYawTar = vec.PitchYaw(v3VelRel, nDriftLookYaw)
	local nDifPit, nDifYaw = nDriftPitTar - nDriftLookPit, com.WrapAngle(nDriftYawTar - nDriftLookYaw)
	local nDifRem = vec.Mag2(nDifPit, nDifYaw)
	local STIFF_ANGLE = 0.05
	local _, nDel = com.Smooth(nDifRem, 0.0, 10.0, nil, nDifRem >= STIFF_ANGLE and nDifRem or nDifRem * nDifRem / STIFF_ANGLE)
	local nDelPit, nDelYaw = vec.Resized2(nDifPit, nDifYaw, math_abs(nDel))
	nDriftLookPit, nDriftLookYaw = nDriftLookPit + nDelPit, com.WrapAngle(nDriftLookYaw + nDelYaw)
	self.nDriftLookPit, self.nDriftLookYaw = nDriftLookPit, nDriftLookYaw
	
end

--[[------------------------------------
	plr:LookEuler

OUT	nLookRol, nLookPit, nLookYaw
--------------------------------------]]
function plr:LookEuler(iLookMode)
	
	if iLookMode == LOOK_NORMAL then
		return self.nAimLookRol, self.nAimLookPit, com.WrapAngle(self.nAimLookYaw + self.nYawDelta + self.nYawCamTemp)
	elseif iLookMode == LOOK_BACK then
		return 0, 0, com.WrapAngle(math.pi + self.nYawDelta + self.nYawCamTemp)
	elseif iLookMode == LOOK_DRIFT then
		return 0, self.nDriftLookPit, self.nDriftLookYaw
	else
		return 0, 0, 0
	end
	
end

--[[------------------------------------
	plr:UpdateLookMode

OUT	nLookRol, nLookPit, nLookYaw
--------------------------------------]]
function plr:UpdateLookMode()
	
	local iOldMode = self.iLookMode
	local nOldRol, nOldPit, nOldYaw = self:LookEuler(iOldMode)
	
	local iLookMode = (key.Down(ACT_LOOK_BACK) and LOOK_BACK) or
		(key.Down(ACT_LOOK_DRIFT) and LOOK_DRIFT) or
		LOOK_NORMAL
	
	self.iLookMode = iLookMode
	local nLookRol, nLookPit, nLookYaw
	
	if iLookMode == iOldMode then
		nLookRol, nLookPit, nLookYaw = nOldRol, nOldPit, nOldYaw
	else
		
		nLookRol, nLookPit, nLookYaw = self:LookEuler(iLookMode)
		
		if iLookMode == LOOK_BACK or iOldMode == LOOK_BACK then
			self.nLookTempRol, self.nLookTempPit, self.nLookTempYaw = 0, 0, 0
		else
			
			self.nLookTempRol = self.nLookTempRol + nOldRol - nLookRol
			self.nLookTempPit = self.nLookTempPit + nOldPit - nLookPit
			self.nLookTempYaw = com.WrapAngle(self.nLookTempYaw + nOldYaw - nLookYaw)
			
		end
		
	end
	
	self.nLookTempRol, self.nLookTempPit, self.nLookTempYaw = com.SmoothResetEuler(self.nLookTempRol, self.nLookTempPit, self.nLookTempYaw, 10, 0.02)
	return nLookRol + self.nLookTempRol, nLookPit + self.nLookTempPit, com.WrapAngle(nLookYaw + self.nLookTempYaw)
	
end

--[[------------------------------------
	plr:UpdateShake
--------------------------------------]]
function plr:UpdateShake()
	
	-- FIXME: sub-millisecond timing
	local iTime = wrp.SimTime()
	self.nSlowShake = 0.004 * math.cos(iTime / 1000.0 * 25.0)
	self.v2FastShake = 0.006 * math.sin(iTime / 1000.0 * 46.0), 0.006 * math.sin(iTime / 1000.0 * 33.0)
	
end

--[[------------------------------------
	plr:UpdateBob

OUT	v3Bob, v3BobVel
--------------------------------------]]
function plr:UpdateBob(v3Bob, v3BobVel, v3HeldWeight, v3HeldWeightDel, nStep)
	
	local v3BobAdd = -v3HeldWeightDel
	xBobAdd = com.SubMag(xBobAdd, 200 * nStep)
	yBobAdd = com.SubMag(yBobAdd, 100 * nStep)
	zBobAdd = com.SubMag(zBobAdd, 300 * nStep)
	yBobAdd = yBobAdd - com.SubMag(yHeldWeight, 50) * 12.0 * nStep
	v3BobAdd = xBobAdd * 0.0025, yBobAdd * 0.004, zBobAdd * 0.001
	v3BobVel = v3BobVel + v3BobAdd
	v3Bob = v3Bob + v3BobVel * nStep
	
	local v3BOB_LIMIT = 0.25, 0.25, 0.15 -- FIXME: check these again once interior glass canopy is visible
	if math_abs(xBob) > xBOB_LIMIT then xBob, xBobVel = xBob > 0.0 and xBOB_LIMIT or -xBOB_LIMIT, 0 end
	if math_abs(yBob) > yBOB_LIMIT then yBob, yBobVel = yBob > 0.0 and yBOB_LIMIT or -yBOB_LIMIT, 0 end
	if math_abs(zBob) > zBOB_LIMIT then zBob, zBobVel = zBob > 0.0 and zBOB_LIMIT or -zBOB_LIMIT, 0 end
	
	local DAMPER = 8.0
	local STIFFNESS = 150.0
	xBobVel = com.Smooth(xBobVel, 0, DAMPER, nil, nil, nStep)
	yBobVel = com.Smooth(yBobVel, 0, DAMPER, nil, nil, nStep)
	zBobVel = com.Smooth(zBobVel, 0, DAMPER, nil, nil, nStep)
	local nStiffFac = STIFFNESS * nStep
	local v3Stiff = -v3Bob * nStiffFac
	v3BobVel = v3BobVel + v3Stiff
	
	return v3Bob, v3BobVel
	
end

--[[------------------------------------
	plr:UpdateOriBob

OUT	v3Bob, v3BobVel
--------------------------------------]]
function plr:UpdateOriBob(v3Bob, v3BobVel, v3HeldWeight, v3HeldWeightDel, nStep)
	
	local v3BobAdd = yHeldWeightDel, -xHeldWeightDel, 0
	xBobAdd = com.SubMag(xBobAdd, 100 * nStep)
	yBobAdd = com.SubMag(yBobAdd, 200 * nStep)
	xBobAdd = xBobAdd + com.SubMag(yHeldWeight, 50) * 12.0 * nStep
	v2BobAdd = xBobAdd * 0.004, yBobAdd * 0.0025
	v3BobVel = v3BobVel + v3BobAdd
	v3Bob = v3Bob + v3BobVel * nStep
	
	local ORI_BOB_LIMIT = 0.3
	if math_abs(xBob) > ORI_BOB_LIMIT then xBob, xBobVel = xBob > 0.0 and ORI_BOB_LIMIT or -ORI_BOB_LIMIT, 0 end
	if math_abs(yBob) > ORI_BOB_LIMIT then yBob, yBobVel = yBob > 0.0 and ORI_BOB_LIMIT or -ORI_BOB_LIMIT, 0 end
	if math_abs(zBob) > ORI_BOB_LIMIT then zBob, zBobVel = zBob > 0.0 and ORI_BOB_LIMIT or -ORI_BOB_LIMIT, 0 end
	
	local DAMPER = 16.0
	local STIFFNESS = 150.0
	xBobVel = com.Smooth(xBobVel, 0, DAMPER, nil, nil, nStep)
	yBobVel = com.Smooth(yBobVel, 0, DAMPER, nil, nil, nStep)
	zBobVel = com.Smooth(zBobVel, 0, DAMPER, nil, nil, nStep)
	local nStiffFac = STIFFNESS * nStep
	local v3Stiff = -v3Bob * nStiffFac
	v3BobVel = v3BobVel + v3Stiff
	
	return v3Bob, v3BobVel
	
end

--[[------------------------------------
	plr:Hurt

OUT	bKill
--------------------------------------]]
function plr:Hurt(entAttacker, nDmg, v3Dir, entHit)
	
	self.nHealth = self.nHealth - nDmg
	local iTime = wrp.SimTime()
	self:UpdateHitTime(iTime, v3Dir)
	
	if self.nHealth <= 0.0 then
		
		iPlayerDieTime = iTime
		return true
		
	end
	
	return false
	
end

--[[------------------------------------
	plr:UpdateHitTime

FIXME: Some bullets get the wrong indicator, not sure if it's a problem in bullet.lua or here
	Might be related to relative velocity?
		If a bullet moving away is crashed into, indicator should show position of bullet, not its movement direction
			I think hit indicator should be based on a hit position/origin vector passed into Hurt
--------------------------------------]]
function plr:UpdateHitTime(iTime, v3Dir)
	
	local iHitDir
	local v3Incoming = vec.Normalized3(-v3Dir)
	local q4Ori = self:Ori()
	local nFwdDot = vec.Dot3(v3Incoming, qua.Dir(q4Ori))
	local nFwdAbs = math_abs(nFwdDot)
	local nLeftDot = vec.Dot3(v3Incoming, qua.Left(q4Ori))
	local nLeftAbs = math_abs(nLeftDot)
	local nUpDot = vec.Dot3(v3Incoming, qua.Up(q4Ori))
	
	if nFwdAbs > nLeftAbs then
		
		if nFwdAbs > math_abs(nUpDot) then
			iHitDir = nFwdDot >= 0 and 2 or 4
		else
			iHitDir = 1
		end
		
	elseif nLeftAbs > math_abs(nUpDot) then
		iHitDir = nLeftDot >= 0 and 3 or 1
	else
		iHitDir = nUpDot >= 0 and 2 or 4
	end
	
	self.tHitTimes[iHitDir] = iTime
	
end

--[[------------------------------------
	plr:Move

OUT	v3Pos, v3Vel

Note, trigger order of overlapping entities is unpredictable since it depends on entity leaf
link order which can change each tick.
--------------------------------------]]
function plr:Move(tRes, v3Pos, v3PrevPos, v3Vel, v3PrevVel, nTime)
	
	local v3Save = v3Pos
	local v3SaveVel = v3Vel
	
	-- FIXME: Test triggers when dead? Allow killed player to be revived by falling into a health pack
	if self.nHealth > 0 then
		
		-- Retrace movement to Touch I_ENT_TRIGGER entities.
		local hulEnt = EntHull(self)
		local I_ENT_TRIGGER = I_ENT_TRIGGER
		local tEnts, tTR = com.TempTable(), com.TempTable()
		local iNumEnts = hit.DescentEntities(tEnts, hulEnt, v3PrevPos, v3Pos, 0, 0, 0, 1)
		
		-- Hit-test and Touch collected entities
		for i = 1, iNumEnts do
			
			local entTch = tEnts[i]
			local hulTch
			local v3Tch, q4Tch
			
			-- Ignore self and deleted entities (might happen due to effects of Touch)
			if entTch == self or not scn_EntityExists(entTch) then
				goto continue
			end
			
			hulTch = EntHull(entTch)
			local Touch = entTch.Touch
			
			if not hulTch or not Touch or not FlagSetTrue(EntFlagSet(entTch), I_ENT_TRIGGER) then
				goto continue
			end
			
			v3Tch = EntPos(entTch)
			q4Tch = EntHullOri(entTch)
			
			tTR[1], tTR[2], tTR[3], tTR[4], tTR[5], tTR[6] = hit_TestHullHull(
				hulEnt, v3PrevPos, v3Save, 0, 0, 0, 1, hulTch, v3Tch, q4Tch
			)
			
			if tTR[1] ~= hit_NONE then
				
				tTR[7] = entTch
				
				-- FIXME: position/velocity effect will be undone if another entity is touched in the same tick
					-- FIXME: take the one with the smallest nTF?
						-- FIXME: sort by nTF and cancel further Touch's if one of them changes v3Pos?
				v3Pos, v3Vel = Touch(self, tTR, v3Save, v3PrevPos, v3Vel, v3PrevVel, nTime, false)
				
			end
			
			::continue::
			
		end
		
		com.FreeTempTable(tEnts)
		com.FreeTempTable(tTR)
		
	end
	
	-- FIXME: support INTERSECTs by using a normalized MTV?
	if tRes[1] == hit.HIT and xPos == xSave and yPos == ySave and zPos == zSave then
		
		v3Vel = self:Impact(tRes, v3Pos, v3PrevVel, v3Vel, tRes[4], tRes[5], tRes[6], nTime,
			tRes[7])
		
	end
	
	return v3Pos, v3Vel
	
end

--[[------------------------------------
	plr:Impact

OUT	v3Vel, nAway

FIXME: Pushing a box causes continual bouncing, can't stick to it; I thought it worked before...
--------------------------------------]]
function plr:Impact(tMoveRes, v3Pos, v3ImpactVel, v3DefaultVel, v3Normal, nTime, entOther,
	v3OtherImpactVel)
	
	local v3Vel, nAway = phy.Impact(self, tMoveRes, v3Pos, v3ImpactVel, v3DefaultVel, v3Normal,
		nTime, entOther, v3OtherImpactVel)
	
	self:AddImpactBob(v3Vel - v3ImpactVel)
	
	if nAway < 0.0 then
		self:Hurt(nil, math.max(0, -nAway) * 0.5, 0, 0, 0, nil)
	end
	
	-- Orientation
	if nAway < 0.0 then
		
		if nAway < -self.nBounceLimit then
			
			local SPHERE_RADIUS = 4.0
			local q4Ori = self:Ori()
			local v3RelVel = v3ImpactVel
			
			if xOtherImpactVel then
				v3RelVel = v3RelVel - v3OtherImpactVel
			end
			
			local nDot = vec.Dot3(v3Normal, v3RelVel)
			local v3Proj = v3RelVel - v3Normal * nDot
			local v3Spin = vec.Cross(v3Normal, v3Proj)
			local v3Want = qua.VecRotInv(v3Spin, q4Ori)
			v3Want = v3Want / SPHERE_RADIUS -- Required angular velocity to keep sphere's contact point at rest
			local nRelVel = vec.Mag3(v3RelVel)
			
			if nRelVel > 0.0 then
				
				self.v3Stability = 0.0, 0.0, 0.0 -- Don't auto stabilize for a while
				local nNormFrac = math.max(0.0, -nAway) / nRelVel
				local nTorqueFrac = 0.7 * nNormFrac -- Reduce torque, especially if normal force is weak
				self.v3OriVel = self.v3OriVel + (v3Want - self.v3OriVel) * nTorqueFrac
				
			end
			
		else
			self.v3AlignSurface = v3Normal -- OrientAttitude will take care of this next tick
		end
		
	end
	
	return v3Vel, nAway
	
end

--[[------------------------------------
	plr:AddImpactBob
--------------------------------------]]
function plr:AddImpactBob(v3VelDelta)
	
	local q4Ori = self:Ori()
	local v3ImpactWeight = qua.VecRotInv(v3VelDelta * 0.02, q4Ori)
	self.v3BobVel = self.v3BobVel - v3ImpactWeight
	local v3Up = qua.Up(q4Ori)
	local v3BobAxis = vec.Cross(v3Up, vec.Normalized3(v3VelDelta))
	v3BobAxis = qua.VecRotInv(-v3BobAxis, q4Ori)
	local nOriAdd = vec.Mag3(v3VelDelta) * 0.03
	self.v3OriBobVel = self.v3OriBobVel + v3BobAxis * nOriAdd
	
end

--[[------------------------------------
	plr:AddBullets

OUT	iNumAdded
--------------------------------------]]
function plr:AddBullets(iNum)
	
	local iOld = self.iNumBullets
	self.iNumBullets = math.min(self.iMaxBullets, iOld + iNum)
	return self.iNumBullets - iOld
	
end

--[[------------------------------------
	plr:AddRockets

OUT	iNumAdded
--------------------------------------]]
function plr:AddRockets(iNum)
	
	local iOld = self.iNumRockets
	self.iNumRockets = math.min(self.iMaxRockets, iOld + iNum)
	return self.iNumRockets - iOld
	
end

--[[------------------------------------
	plr:ShowNoise

Test of 3D simplex noise
--------------------------------------]]
function plr:ShowNoise()
	
	local v3_GRID_SIZE = 25, 25, 0
	local DOT_SCALE = 10
	local tDots = self.tDots
	local iNum = 1
	
	if not tDots then
		
		tDots = {}
		self.tDots = tDots
		
		for xDot = -x_GRID_SIZE, x_GRID_SIZE do
			
			for yDot = -y_GRID_SIZE, y_GRID_SIZE do
				
				for zDot = -z_GRID_SIZE, z_GRID_SIZE do
					
					local entDot = scn.CreateEntity(nil, 0, 0, 0, 0, 0, 0, 1)
					entDot:SetMesh(rnd.EnsureMesh("bolt.msh"))
					entDot:SetTexture(rnd.EnsureTexture("tracer.tex"))
					entDot:SetRegularGlass(true)
					entDot:SetShadow(false)
					entDot:SetScale(4.0)
					entDot:SetSubPalette(3)
					tDots[iNum] = entDot
					iNum = iNum + 1
					
				end
				
			end
			
		end
		
	end
	
	local Noise3 = com.SimplexNoise3
	local EntSetPos = scn.EntSetPos
	local EntSetOpacity = scn.EntSetOpacity
	local v3Pos = self:Pos()
	iNum = 1
	
	for xDot = -x_GRID_SIZE, x_GRID_SIZE do
		
		for yDot = -y_GRID_SIZE, y_GRID_SIZE do
			
			for zDot = -z_GRID_SIZE, z_GRID_SIZE do
				
				local entDot = tDots[iNum]
				local v3DotPos = v3Pos + v3Dot * DOT_SCALE
				EntSetPos(entDot, v3DotPos)
				EntSetOpacity(entDot, Noise3(v3DotPos * 0.01, 0.57))
				iNum = iNum + 1
				
			end
			
		end
		
	end
	
end

--[[------------------------------------
	plr:HurtGod

OUT	bKill
--------------------------------------]]
function plr:HurtGod(entAttacker, nDmg, v3Dir, entHit)
	
	self:UpdateHitTime(wrp.SimTime(), v3Dir)
	return false
	
end

--[[------------------------------------
	plr.CheatGod
--------------------------------------]]
function plr.CheatGod()
	
	local ent = entPlayer
	
	if not ent then
		return
	end
	
	ent.bCheatGod = not ent.bCheatGod
	
	if ent.bCheatGod then
		ent.Hurt = plr.HurtGod
	else
		ent.Hurt = plr.Hurt
	end
	
	con.LogF("God mode %s", ent.bCheatGod)
	
end

--[[------------------------------------
	plr.CheatNoTarget
--------------------------------------]]
function plr.CheatNoTarget()
	
	local ent = entPlayer
	
	if not ent then
		return
	end
	
	ent.bCheatNoTarget = not ent.bCheatNoTarget
	con.LogF("No-target mode %s", ent.bCheatNoTarget)
	
end

--[[------------------------------------
	plr.CheatUnlimitedYaw
--------------------------------------]]
function plr.CheatUnlimitedYaw()
	
	local ent = entPlayer
	
	if not ent then
		return
	end
	
	ent.bCheatUnlimitedYaw = not ent.bCheatUnlimitedYaw
	con.LogF("Unlimited-yaw mode %s", ent.bCheatUnlimitedYaw)
	
end

con.CreateCommand("god", plr.CheatGod)
con.CreateCommand("no_target", plr.CheatNoTarget)
con.CreateCommand("unlimited_yaw", plr.CheatUnlimitedYaw)

plr.typ = scn.CreateEntityType("player", plr.Init, plr.Tick, plr.Term, nil, plr.Level)

return plr