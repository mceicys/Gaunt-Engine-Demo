-- hud.lua
-- FIXME: improve gui system before doing real work on this
LFV_EXPAND_VECTORS()

local flg = gflg
local gui = ggui
local hit = ghit
local qua = gqua
local rnd = grnd
local scn = gscn
local scr = gscr
local vec = gvec
local wrp = gwrp

local com = scr.EnsureScript("common.lua")
local phy = scr.EnsureScript("physics.lua")

local EntFinalPos = scn.EntFinalPos
local hit_ALL = hit.ALL
local hit_LineTest = hit.LineTest
local hit_NONE = hit.NONE
local ImgSetFrame = gui.ImgSetFrame
local ImgSetPos = gui.ImgSetPos
local ImgSetScale = gui.ImgSetScale
local ImgSetTexture = gui.ImgSetTexture
local ImgSetVisible = gui.ImgSetVisible
local ImgVisible = gui.ImgVisible
local qua_VecRot = qua.VecRot
local vec_MagSq3 = vec.MagSq3

local hud = {}

local texFont = rnd.EnsureTexture("font_small.tex")
local texHUD = rnd.EnsureTexture("hud.tex") -- FIXME: crosshair texture
local texHit = rnd.EnsureTexture("hudhit.tex")
local texRadCir = rnd.EnsureTexture("radcir.tex")
local texRadPing = rnd.EnsureTexture("radping.tex")
local texBogie = rnd.EnsureTexture("bogie.tex")
local texMirror = rnd.EnsureTexture("mirror.tex")
local texMirrorBorder = rnd.EnsureTexture("mirrorborder.tex")

hud.texHUD = texHUD
hud.texHit = texHit
hud.texRadCir = texRadCir
hud.texRadPing = texRadPing
hud.texMirror = texMirror
hud.texMirrorBorder = texMirrorBorder

hud.imgAimer = gui.Image(texHUD)
hud.imgAimer:SetFrame(0)
hud.imgAimer:SetScale(1)

hud.imgVelocity = gui.Image(texHUD)
hud.imgVelocity:SetFrame(1)
hud.imgVelocity:SetScale(1)

hud.txtSpeed = gui.Text(texFont, 8)
hud.txtSpeed:SetScale(3)

local tHitImages = {
	gui.Image(texHit),
	gui.Image(texHit),
	gui.Image(texHit),
	gui.Image(texHit)
}

hud.tHitImages = tHitImages

hud.imgRadarCircle = gui.Image(texRadCir)
hud.imgMirrorBorder = gui.Image(texMirrorBorder)

hud.fsetPingIgnore = flg:FlagSet():Or(I_ENT_SOFT, I_ENT_TRIGGER, I_ENT_MOBILE)

local iNumTempImages = 0
local tTempImages = {} -- Reuse image objects instead of recreating them every frame
local tPingedEnts = {}

local camMirror = scn.Camera(0, 0, 0, 0, 0, 0, 1, math.atan(3.0 / 8.0) * 2.0)
camMirror:SetLerpPos(false)
camMirror:SetLerpOri(false)

--[[------------------------------------
	hud.Frame

FIXME: scale 1 frames have texels from other frames showing up sometimes, depending on packing
FIXME: render HUD in 3D cockpit?
--------------------------------------]]
function hud.Frame()
	
	local iTime = wrp.SimTime()
	local ent = entPlayer
	local cam = scn.ActiveCamera()
	
	for i = 1, #tPingedEnts do tPingedEnts[i] = nil end
	for i = 1, iNumTempImages do ImgSetTexture(tTempImages[i], nil) end
	iNumTempImages = 0
	
	local imgAimer = hud.imgAimer
	local imgVelocity = hud.imgVelocity
	local txtSpeed = hud.txtSpeed
	local imgRadarCircle = hud.imgRadarCircle
	local imgMirrorBorder = hud.imgMirrorBorder
	
	if scn.EntityExists(ent) and cam == ent.cam then
		
		local v2Vid = wrp.VideoWidth(), wrp.VideoHeight()
		local v3CamPos = cam:FinalPos()
		
		local v3GunRel = qua.VecRot(1, 0, 0, ent.q4Gun)
		local v3GunUser = rnd.VecWorldToUser(v2Vid, cam, v3CamPos + v3GunRel)
		ImgSetPos(imgAimer, v2GunUser, zGunUser >= 0.0 and 0.1 or -1.0)
		ImgSetVisible(imgAimer, true)
		
		local v3Vel = ent.v3Vel
		
		--[[
		-- Disabled velocity HUD; it helps but causes player to panic when it's way off center
			-- Have to mentally switch from using it to physical reference points
		-- FIXME: enable with cheat
		local TIME_AHEAD = 0.025
		local v3VelWorld = v3CamPos + v3Vel * TIME_AHEAD
		local v3VelUser = rnd.VecWorldToUser(v2Vid, cam, v3VelWorld)
		ImgSetPos(imgVelocity, v2VelUser, zVelUser <= 1.0 and 0.11 or -1.0) -- FIXME: don't set z based on clip planes, do a separate dot product check
		ImgSetVisible(imgVelocity, true)
		--]]
		
		txtSpeed:SetPos(xVid - 68, 20, 0.12)
		txtSpeed:SetString(string.format("%.0f", vec.Mag3(v3Vel)))
		txtSpeed:SetVisible(false) -- FIXME: make this a debug option
		
		local tHitTimes = ent.tHitTimes
		
		for i = 1, 4 do
			
			local imgHit = tHitImages[i]
			local iHitAge = tHitTimes[i] and iTime - tHitTimes[i]
			
			if iHitAge and iHitAge < 300 then
				
				imgHit:SetPos(v2GunUser, 0.13)
				imgHit:SetFrame((iHitAge < 30 or ImgVisible(imgHit) == false) and i - 1 or i + 3)
				ImgSetVisible(imgHit, true)
				
			else
				ImgSetVisible(imgHit, false)
			end
			
		end
		
		-- FIXME TEMP
		--[[
		local v2Radar = 132, 132
		ImgSetVisible(imgRadarCircle, false)
		ImgSetPos(imgRadarCircle, v2Radar, 0.14)
		
		local v3EntPos = EntFinalPos(ent)
		local q4Radar = qua.QuaYaw(-qua.Yaw(ent.q4Aim) + math.pi * 0.5)
		local nRadarRadius = 1000.0
		local nRadarFactor = 128.0 / nRadarRadius
		local iNumEnts = hit.SphereEntities(tPingedEnts, v3EntPos, nRadarRadius)
		
		for i = 1, iNumEnts do
			
			local entPing = tPingedEnts[i]
			local iRadarFrame = entPing.iRadarFrame
			
			if iRadarFrame then
				
				iNumTempImages = iNumTempImages + 1
				
				if not tTempImages[iNumTempImages] then
					tTempImages[iNumTempImages] = gui.Image()
				end
				
				local imgPing = tTempImages[iNumTempImages]
				ImgSetTexture(imgPing, texRadPing)
				ImgSetVisible(imgPing, true)
				ImgSetFrame(imgPing, iRadarFrame)
				
				local v3PingWorld = EntFinalPos(entPing)
				local v3Dif = qua_VecRot(v3PingWorld - v3EntPos, q4Radar)
				ImgSetPos(imgPing, v2Radar + v2Dif * nRadarFactor, 0.15)
				
			end
			
		end
		--]]
		
		--
		--	BOGIES
		--
		
		--[[
		local nRadarRadius = 1000.0
		local q4Cam = cam:FinalOri()
		local iNumEnts = hit.SphereEntities(tPingedEnts, v3CamPos, nRadarRadius)
		local v2Center = v2Vid * 0.5
		local v2MaxBound = v2Vid
		local v2MinBound = 0, yVid * 0.25
		local ARROW_OFFSET = 16
		local ARROW_SCALE = 2 -- FIXME TEMP
		local fsetPingIgnore = hud.fsetPingIgnore
		
		for i = 1, iNumEnts do
			
			local entPing = tPingedEnts[i]
			
			if not entPing:FlagSet():True(I_ENT_ENEMY) then
				goto continue
			end
			
			local v3BogieWorld = EntFinalPos(entPing)
			
			local iC = hit_LineTest(v3CamPos, v3BogieWorld, hit_ALL, fsetPingIgnore, ent,
				entPing)
			
			if iC ~= hit_NONE then
				goto continue
			end
			
			--[=[
			local v3BogieUser = rnd.VecWorldToUser(v2Vid, cam, v3BogieWorld)
			
			if zBogieUser >= 1.0 then
				v2BogieUser = -v2BogieUser -- Behind camera
			elseif xBogieUser >= xMinBound and xBogieUser <= xMaxBound and
			yBogieUser >= yMinBound and yBogieUser <= yMaxBound then
				goto continue -- Within bounds
			end
			]=]
			
			local v3BogieRel = qua.VecRotInv(v3BogieWorld - v3CamPos, q4Cam)
			local v2UserDir = vec.Normalized2(-yBogieRel, zBogieRel)
			local nTR = xUserDir ~= 0.0 and (xMaxBound - xCenter) / xUserDir or -1.0
			local nTL = xUserDir ~= 0.0 and (xMinBound - xCenter) / xUserDir or -1.0
			local nTU = yUserDir ~= 0.0 and (yMaxBound - yCenter) / yUserDir or -1.0
			local nTD = yUserDir ~= 0.0 and (yMinBound - yCenter) / yUserDir or -1.0
			local nT = math.huge
			local iFrame = nil
			local v2Offset = 0, 0
			if nTR >= 0.0 and nTR < nT then nT, iFrame, v2Offset = nTR, 0, -ARROW_OFFSET, 0 end
			if nTL >= 0.0 and nTL < nT then nT, iFrame, v2Offset = nTL, 2, ARROW_OFFSET, 0 end
			if nTU >= 0.0 and nTU < nT then nT, iFrame, v2Offset = nTU, 1, 0, -ARROW_OFFSET end
			if nTD >= 0.0 and nTD < nT then nT, iFrame, v2Offset = nTD, 3, 0, ARROW_OFFSET end
			
			if not iFrame then
				
				-- Bogie is probably directly behind, show a downward arrow
				v2UserDir = 0, -1
				nT, iFrame, v2Offset = yMinBound - yCenter, 3, 0, ARROW_OFFSET
				
			end
			
			local iFireTime = entPing.iFireTime
			
			if iFireTime and iTime - iFireTime < 100 then
				iFrame = iFrame + 4
			end
			
			iNumTempImages = iNumTempImages + 1
			
			if not tTempImages[iNumTempImages] then
				tTempImages[iNumTempImages] = gui.Image()
			end
			
			local imgBogie = tTempImages[iNumTempImages]
			ImgSetTexture(imgBogie, texBogie)
			ImgSetVisible(imgBogie, true)
			ImgSetFrame(imgBogie, iFrame)
			ImgSetPos(imgBogie, v2Center + v2UserDir * nT + v2Offset * ARROW_SCALE, 0.14)
			imgBogie:SetScale(ARROW_SCALE)
			
			::continue::
			
		end
		]]
		
		--
		--	MIRROR
		--
		
		--[=[
		local MIRROR_SCALE = 2
		local v2MirrorSize = 160 * MIRROR_SCALE, 60 * MIRROR_SCALE
		local v2MirrorOrigin = xVid * 0.5 - xMirrorSize * 0.5, yVid - yMirrorSize
		ImgSetPos(imgMirrorBorder, xVid * 0.5, yVid - yMirrorSize * 0.5, 0.14) -- FIXME: would be nice to have textures with optional bottom-left origins
		ImgSetScale(imgMirrorBorder, MIRROR_SCALE)
		camMirror:SetPos(v3CamPos)
		local q4Cam = cam:FinalOri()
		local q4Mirror = qua.Product(q4Cam, qua.QuaPitchYaw(0.0, math.pi))
		camMirror:SetOri(q4Mirror)
		local nMirrorRadius = 1000.0
		local iNumEnts = hit.SphereEntities(tPingedEnts, v3CamPos, nMirrorRadius)
		local fsetPingIgnore = hud.fsetPingIgnore
		local bShowMirror = false
		
		for i = 1, iNumEnts do
			
			local entPing = tPingedEnts[i]
			
			if not entPing.iMirrorFrame then
				goto continue
			end
			
			local v3BogieWorld = EntFinalPos(entPing)
			
			local iC = hit_LineTest(v3CamPos, v3BogieWorld, hit_ALL, fsetPingIgnore, ent,
				entPing)
			
			if iC ~= hit_NONE then
				goto continue
			end
			
			local v3BogieMirror, nLinDepth = rnd.VecWorldToUser(v2MirrorSize, camMirror, v3BogieWorld)
			
			if nLinDepth <= 0.0 or xBogieMirror < 0.0 or xBogieMirror > xMirrorSize or
			yBogieMirror < 0.0 or yBogieMirror > yMirrorSize then
				goto continue -- Not in mirror
			end
			
			local iScale = math.floor(math.max(1, 320.0 * MIRROR_SCALE / nLinDepth))
			local v2BogieUser = xMirrorSize - xBogieMirror + xMirrorOrigin, yBogieMirror + yMirrorOrigin
			local iFrame = 0
			iNumTempImages = iNumTempImages + 1
			
			if not tTempImages[iNumTempImages] then
				tTempImages[iNumTempImages] = gui.Image()
			end
			
			local imgBogie = tTempImages[iNumTempImages]
			ImgSetTexture(imgBogie, texMirror)
			ImgSetVisible(imgBogie, true)
			ImgSetFrame(imgBogie, entPing.iMirrorFrame)
			ImgSetPos(imgBogie, v2BogieUser, 0.15)
			ImgSetScale(imgBogie, iScale)
			bShowMirror = true -- FIXME: make it disappear after a few seconds of no threats
				-- FIXME: some projectiles like bolts should only be considered threats if moving towards camera/player
			
			::continue::
			
		end
		
		ImgSetVisible(imgMirrorBorder, true)
		]=]
		
	else
		
		--ImgSetVisible(imgAimer, false)
		ImgSetVisible(imgAimer, false)
		local v2Vid = wrp.VideoWidth(), wrp.VideoHeight()
		ImgSetPos(imgAimer, v2Vid * 0.5, 0.1)
		
		ImgSetVisible(imgVelocity, false)
		ImgSetVisible(imgRadarCircle, false)
		txtSpeed:SetVisible(false)
		ImgSetVisible(imgMirrorBorder, false)
		
		for i = 1, 4 do
			ImgSetVisible(tHitImages[i], false)
		end
		
	end
	
end

return hud