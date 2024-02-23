-- game.lua
LFV_EXPAND_VECTORS()

local aud = gaud
local con = gcon
local key = gkey
local pat = gpat
local rec = grec
local rnd = grnd
local scn = gscn
local scr = gscr
local wrp = gwrp

bp = scr.Breakpoint

nFrameStep = 0.0 -- HACK: Approximate time simulated since last frame
nLastEventFraction = 0.0
dscGlobal = ghit.Descent()
nWldDiag = 0.0

local com = scr.EnsureScript("common.lua")

-- FIXME: remove flag sets, do 32-bit integer flags
I_ENT_SOFT = com.NewEntFlag() -- Safe to physically ignore
		-- FIXME: implement or remove
I_ENT_IGNORES_SOFT = com.NewEntFlag() -- Hint that this entity may ignore soft entities
I_ENT_TRIGGER = com.NewEntFlag() -- Can physically ignore but player calls Touch
		-- FIXME: implement
I_ENT_MOBILE = com.NewEntFlag() -- Small and often moving, don't treat like a fixed obstacle
I_ENT_ENEMY = com.NewEntFlag()

scr.EnsureScript("actions.lua")
ai = scr.EnsureScript("ai.lua")
amo = scr.EnsureScript("ammo.lua")
ban = scr.EnsureScript("bandit.lua")
bla = scr.EnsureScript("blastdoor.lua")
car = scr.EnsureScript("carrier.lua")
cld = scr.EnsureScript("cloud.lua")
ed = scr.EnsureScript("editor.lua")
fre = scr.EnsureScript("freecam.lua")
gua = scr.EnsureScript("guard.lua")
hud = scr.EnsureScript("hud.lua")
lok = scr.EnsureScript("lock.lua")
tmb = scr.EnsureScript("timebomb.lua")
tur = scr.EnsureScript("turret.lua")
psp = scr.EnsureScript("palswap.lua")
pil = scr.EnsureScript("pillar.lua")
plr = scr.EnsureScript("player.lua")
pt = scr.EnsureScript("point.lua")
box = scr.EnsureScript("testbox.lua") -- FIXME TEMP

local ban = ban
local ed = ed
local fre = fre
local gua = gua
local hud = hud
local tur = tur
local pt = pt
local plr = plr
local psp = psp

--[[------------------------------------
	GameInit
--------------------------------------]]
function GameInit()
	
	math.randomseed(wrp.SystemClock())
	rnd.SetPalette(psp.palBase)
	rec.RequestLoad("levels/horse.sav")
	
end

--[[------------------------------------
	GameForeLoad
--------------------------------------]]
function GameForeLoad()
	
	ed.bLoaded = false
	
	lok.ClearLockInfo()
	pt.ClearPointList()
	
	ban.iNumBandits = 0 -- FIXME TEMP
	
	scn.SetActiveCamera(nil)
	fre.bFreeCam = false
	
	iPlayerDieTime = nil
	
	local v3Min, v3Max = scn.WorldBox()
	nWldDiag = gvec.Mag3(v3Max - v3Min)
	
	guard = nil -- FIXME TEMP
	ai.ForeLoad()
	
end

--[[------------------------------------
	GameLoad
--------------------------------------]]
function GameLoad()
	
	local t = rec.GlobalTranscript()
	
	if t["skySphereTexture"] then
		
		-- FIXME: do this in engine?
		--[[ FIXME: sky is mostly fog and glass based now, but how should stars be done (for night and when at high altitudes)?
			-- Equirectangular texture is no good because of the singularity at the top and bottom ]]
		local texSky = rnd.EnsureTexture(t["skySphereTexture"])
		--texSky:SetMip(false) -- Hide seam
		rnd.SetSkySphereTexture(texSky)
		
	end
	
	nWorldYaw = t["yaw"] or 0.0
	nWorldAltitude = t["altitude"] or 0.0
	
	-- FIXME: need to make this more automated, entity-type's script file should take care of this on its own somehow
	ban.fmap = pat.BestFlightMap(-ban.nProbeSize, -ban.nProbeSize, -ban.nProbeSize, ban.nProbeSize, ban.nProbeSize, ban.nProbeSize)
	gua.fmap = pat.BestFlightMap(-gua.nProbeSize, -gua.nProbeSize, -gua.nProbeSize, gua.nProbeSize, gua.nProbeSize, gua.nProbeSize)
	
	-- FIXME TEMP: level file should add this entity itself
	local entHorizon = scn.CreateEntity(nil, 0, 0, 0, 0, 0, 0, 1)
	entHorizon:SetMesh(rnd.EnsureMesh("horse_horizon.msh"))
	entHorizon:SetTexture(rnd.EnsureTexture("desert.tex"))
	local entHorizonMimic = scn.CreateEntity(nil, entHorizon:Place())
	entHorizonMimic:SetTexture(rnd.EnsureTexture("1f.tex"))
	entHorizonMimic:MimicMesh(entHorizon)
	entHorizonMimic:SetAmbientGlass(true)
	entHorizonMimic:SetShadow(false)
	entHorizonMimic:SetOpacity(0.192) -- FIXME: helper function to calculate world-equivalent intensity (after exponent and factor are applied)
	
	-- FIXME TEMP: sun entity needs to be updated when engine's sun's position changes
	local entSun = scn.CreateEntity(nil, 0, 0, 0, 0, 0, 0, 1)
	local v3Sun = scn.SunPos()
	entSun:SetPos(v3Sun * 149600000000.0)
	entSun:SetOri(gqua.QuaPitchYaw(gvec.PitchYaw(-v3Sun)))
	entSun:SetScale(696000000.0 * 13.0 * 7.0)
	entSun:SetMesh(rnd.EnsureMesh("quad.msh"))
	entSun:SetTexture(rnd.EnsureTexture("sun.tex"))
	entSun:SetRegularGlass(true)
	entSun:SetShadow(false)
	
	-- FIXME TEMP
	--[[
	entBox = scn.CreateEntity(scn.FindEntityType("box"), -1500, -2800, -150, 0, 0, 0, 1)
	entBox.iBox = 1
	entBox.xVel = -200
	scn.CreateEntity(scn.FindEntityType("box"), -1700, -2800, -150, 0, 0, 0, 1).iBox = 2
	scn.CreateEntity(scn.FindEntityType("box"), -1748, -2780, -130, gqua.QuaEuler(0.3, 0.3, 0.6)).iBox = 3
	--scn.CreateEntity(scn.FindEntityType("box"), -1733, -2700, -150, 0, 0, 0, 1).iBox = 3
	scn.CreateEntity(scn.FindEntityType("box"), -1766, -2800, -150, 0, 0, 0, 1).iBox = 4
	--]]
	
end

--[[------------------------------------
	GamePostLoad
--------------------------------------]]
function GamePostLoad()
	
end

--[[------------------------------------
	GameTick
--------------------------------------]]
function GameTick()
	
	nFrameStep = nFrameStep + (1.0 - nLastEventFraction) * wrp.TimeStep()
	nLastEventFraction = 0.0
	fre.Tick()
	
	if iPlayerDieTime and wrp.SimTime() - iPlayerDieTime > 1500 then
		rec.RequestLoad(rec.CurrentLevel())
	end
	
	if key.Pressed(ACT_QUICKLOAD) then
		rec.RequestLoad(rec.CurrentLevel())
	end
	
	if camFollow then
		
		local v3Pos = camFollow:Pos()
		v3Pos = v3Pos + v3FollowCamVel * wrp.TimeStep()
		camFollow:SetPos(v3Pos)
		
	end
	
	if key.Pressed(ACT_NEW_CAM) then
		
		local entPlayer = entPlayer
		
		if camFollow then
			
			camFollow = nil
			scn.SetActiveCamera(entPlayer and entPlayer.cam)
			
		else
			
			v3FollowCamVel = gqua.VecRot(entPlayer.v2Vel * 0.98, 0.0, gqua.QuaYaw(0.05))
			local v3Pos = entPlayer:Pos()
			local nPlayerYaw = gqua.Yaw(entPlayer:Ori())
			local v3Offset = -10, 30, 20
			v3Offset = gqua.VecRot(v3Offset, gqua.QuaYaw(nPlayerYaw))
			local nCamPit, nCamYaw = 0.4, nPlayerYaw - 0.7
			local q4Cam = gqua.QuaPitchYaw(nCamPit, nCamYaw)
			camFollow = scn.Camera(v3Pos + v3Offset, q4Cam, math.atan(3.0 / 4.0) * 2.0)
			scn.SetActiveCamera(camFollow)
			
		end
		
	end
	
end

--[[------------------------------------
	GamePostTick
--------------------------------------]]
function GamePostTick()
	
	-- FIXME: memory grows every tick
	--[[
	gcon.LogF("lua kbytes %g", collectgarbage("count"))
	]]
	
end

--[[------------------------------------
	GameFrame
--------------------------------------]]
function GameFrame()
	
	--[[ Frame step is the span of simulated time from the last frame to this one, it's not the
	time since the last frame from the user's perspective. Use it for timed effects that need to
	approximately sync with ticks (though precision error will accumulate fast). ]]
	if wrp.VarTickRate() ~= 0 then
		nFrameStep = wrp.TimeStep()
	else
		nFrameStep = nFrameStep + (wrp.Fraction() - nLastEventFraction) * wrp.TimeStep()
	end
	
	UpdateLightCurve()
	ed.Frame()
	hud.Frame()
	psp.Frame()
	
	nFrameStep = 0.0
	nLastEventFraction = wrp.Fraction()
	
end

--[[------------------------------------
	UpdateLightCurve

FIXME: This light curve was based around rnd_ramp_add being ~-2 (when dithering did that effect automatically)
	Without that value, the ugly "gamma correct" lighting is very apparent
	Either keep that rnd_ramp_add value or adjust the light curve again to get rid of the ugly falloff
		Might also want to adjust some textures to have more contrast
--------------------------------------]]
function UpdateLightCurve()
	
	local nMaxIntensity = rnd.MaxIntensity()
	
	if nCurveIntensity == nMaxIntensity then
		return
	end
	
	nCurveIntensity = nMaxIntensity
	rnd.ClearCurvePoints()
	rnd.SetCurvePoint(0, 0)
	rnd.SetCurvePoint(0.3, 0.12)
	rnd.SetCurvePoint(1, 1)
	rnd.SetCurvePoint(3, 6)
	rnd.SetCurvePoint(4, 7)
	rnd.SetCurvePoint(5, 7.4)
	rnd.SetCurvePoint(nMaxIntensity, nMaxIntensity) -- Designed for max intensity of 8.0
	
end