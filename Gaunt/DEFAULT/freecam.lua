-- freecam.lua
LFV_EXPAND_VECTORS()

local con = gcon
local hit = ghit
local key = gkey
local qua = gqua
local scn = gscn
local scr = gscr
local vec = gvec
local wrp = gwrp

local com = scr.EnsureScript("common.lua")

local fre = {}

fre.bFreeCam = false
fre.camFree = scn.Camera(0, 0, 0, 0, 0, 0, 1, math.atan(3.0 / 4.0) * 2.0)
fre.nPit, fre.nYaw = 0, 0
fre.nViewSensitivity = 0.16
entLook = nil

local DEG_TO_RAD = 0.01745329251994329576923690768489

--[[------------------------------------
	fre.FreeCam
--------------------------------------]]
function fre.FreeCam(bForce)
	
	if bForce ~= nil and fre.bFreeCam == bForce then
		return
	end
	
	fre.bFreeCam = not fre.bFreeCam
	con.LogF("Free cam mode %s", fre.bFreeCam and "on" or "off")
	local entPlayer = entPlayer
	
	if fre.bFreeCam then
		
		local camFree = fre.camFree
		scn.SetActiveCamera(camFree)
		
		if entPlayer then
			
			local camPlayer = entPlayer.cam
			camFree:SetPos(camPlayer:Pos())
			camFree:SetOri(camPlayer:Ori())
			fre.nPit, fre.nYaw = qua.PitchYaw(camFree:Ori())
			camFree:SetOldPos(camFree:Pos())
			camFree:SetOldOri(camFree:Ori())
			
		end
		
	else
		scn.SetActiveCamera(entPlayer and entPlayer.cam)
	end
	
end

con.CreateCommand("free_cam", fre.FreeCam)

--[[------------------------------------
	fre.Tick
--------------------------------------]]
function fre.Tick()
	
	if not fre.bFreeCam then
		return
	end
	
	local camFree = fre.camFree
	
	-- Aim
	local xMouse, yMouse = key.MouseEventDelta()
	local nSensitivity = fre.nViewSensitivity
	
	if not key.Down(ACT_LOOK) then -- FIXME TEMP: doing this to test head orientation visual
		
		fre.nYaw = com.WrapAngle(fre.nYaw - xMouse * DEG_TO_RAD * nSensitivity)
		fre.nPit = com.WrapAngle(fre.nPit + yMouse * DEG_TO_RAD * nSensitivity)
		
	end
	
	camFree:SetOri(qua.QuaPitchYaw(fre.nPit, fre.nYaw))
	
	-- Move
	local v3In = 0.0, 0.0, 0.0
	if key.Down(ACT_FREE_CAM_FORWARD) then xIn = xIn + 1.0 end -- FIXME: analog input
	if key.Down(ACT_FREE_CAM_BACK) then xIn = xIn - 1.0 end
	if key.Down(ACT_FREE_CAM_LEFT) then yIn = yIn + 1.0 end
	if key.Down(ACT_FREE_CAM_RIGHT) then yIn = yIn - 1.0 end
	if key.Down(ACT_FREE_CAM_UP) then zIn = zIn + 1.0 end
	if key.Down(ACT_FREE_CAM_DOWN) then zIn = zIn - 1.0 end
	v3In = vec.Normalized3(v3In)
	v3In = qua.VecRot(v3In, camFree:Ori())
	local v3Pos = camFree:Pos()
	camFree:SetPos(v3Pos + v3In * 320.0 * wrp.TimeStep())
	
	local v3Dir = qua.Dir(camFree:Ori())
	local _
	_, _, _, _, _, _, entLook = hit.LineTest(v3Pos, v3Pos + v3Dir * 1000, hit.ALL, nil)
	
end

--[[------------------------------------
	fre.RemVec
	
OUT	v3Vec
--------------------------------------]]
function fre.RemVec()
	
	return vec.VecPitchYaw(fre.nPit, fre.nYaw)
	
end

function fre.RemVec1() fre.v3Rem1 = fre.RemVec() end
function fre.RemVec2() fre.v3Rem2 = fre.RemVec() end

--[[------------------------------------
	fre.RemVecAngle
--------------------------------------]]
function fre.RemVecAngle()
	
	local _, _, _, nAngle = vec.DirDelta(fre.v3Rem1, fre.v3Rem2)
	con.LogF("angle: %g rad, %g deg", nAngle, nAngle * 180.0 / math.pi)
	
end

con.CreateCommand("remvec1", fre.RemVec1)
con.CreateCommand("remvec2", fre.RemVec2)
con.CreateCommand("remvecangle", fre.RemVecAngle)

return fre