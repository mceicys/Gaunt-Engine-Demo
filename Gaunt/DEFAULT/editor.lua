-- editor.lua
LFV_EXPAND_VECTORS()

local con = gcon
local gui = ggui
local hit = ghit
local key = gkey
local qua = gqua
local rec = grec
local rnd = grnd
local scn = gscn
local vec = gvec
local wrp = gwrp

local blb_Pos, blb_Next = scn.BlbPos, scn.BlbNext
local hit_NONE, hit_INTERSECT = hit.NONE, hit.INTERSECT
local hit_TestLineHull = hit.TestLineHull
local key_ApplyKeyBufferFrame = key.ApplyKeyBufferFrame
local math_abs, math_huge = math.abs, math.huge
local scn_BulbExists = scn.BulbExists
local string_byte = string.byte
local vec_MagSq3 = vec.MagSq3

local edit = {}

edit.bEditing = false
edit.bLoaded = false -- false means the editor needs to reload the level file
edit.tNearby = {}

edit.tNumVarGetters = {
	["intensity"] = scn.BlbIntensity,
	["subPalette"] = scn.BlbSubPalette,
	["colorSmooth"] = scn.BlbColorSmooth,
	["colorPriority"] = scn.BlbColorPriority,
	["radius"] = scn.BlbRadius,
	["exponent"] = scn.BlbExponent,
	["outer"] = scn.BlbOuter,
	["inner"] = scn.BlbInner
}

edit.tNumVarSetters = {
	["intensity"] = scn.BlbSetIntensity,
	["subPalette"] = scn.BlbSetSubPalette,
	["colorSmooth"] = scn.BlbSetColorSmooth,
	["colorPriority"] = scn.BlbSetColorPriority,
	["radius"] = scn.BlbSetRadius,
	["exponent"] = scn.BlbSetExponent,
	["outer"] = scn.BlbSetOuter,
	["inner"] = scn.BlbSetInner
}

edit.tNumVarOldSetters = {
	["intensity"] = scn.BlbSetOldIntensity,
	["subPalette"] = nil,
	["colorSmooth"] = nil,
	["colorPriority"] = nil,
	["radius"] = scn.BlbSetOldRadius,
	["exponent"] = scn.BlbSetOldExponent,
	["outer"] = scn.BlbSetOldOuter,
	["inner"] = scn.BlbSetOldInner
}

edit.sTypedValue = ""
edit.tBulbNames = {}
edit.iNextName = 0

-- Settings
edit.iNormalColor = 18
edit.iTargetedColor = 2
edit.iSelectedColor = 31
edit.iXColor = 68
edit.iYColor = edit.iXColor + 24
edit.iZColor = edit.iXColor + 48
edit.iRotateColor = 31
edit.nNearbyRadius = 2000
edit.nTargetRadius = 8
edit.hulBulb = hit.HullBox(-2, -2, -2, 2, 2, 2)
edit.hulTarget = hit.HullBox(-edit.nTargetRadius, -edit.nTargetRadius, -edit.nTargetRadius,
				 edit.nTargetRadius, edit.nTargetRadius, edit.nTargetRadius)
edit.nRotateStep = math.pi / 36.0

-- UI
edit.texFont = rnd.EnsureTexture("font_small.tex")
edit.txtMode = gui.Text(edit.texFont, 8)
edit.tInfoTexts = {}
edit.txtSet = gui.Text(edit.texFont, 8)

for i = 1, 10 do
	edit.tInfoTexts[i] = gui.Text(edit.texFont, 8)
end

--[[------------------------------------
	edit.Frame
--------------------------------------]]
function edit.Frame()
	
	if not edit.bEditing then
		return
	end
	
	if not edit.EnsureLoaded() then
		
		edit.ToggleEditor(false)
		return
		
	end
	
	local cam = scn.ActiveCamera()
	
	if not cam then
		return
	end
	
	if key.PressedFrame(ACT_EDITOR_CANCEL) or not edit.sMode then
		edit.ChangeMode("SelectMode")
	elseif edit.sMode == "SelectMode" then
		
		if key.PressedFrame(ACT_EDITOR_CREATE_BULB) then
			edit.SpawnBulb()
		elseif key.PressedFrame(ACT_EDITOR_DELETE) then
			edit.ChangeMode("DeleteMode")
		elseif key.PressedFrame(ACT_EDITOR_MOVE) then
			edit.ChangeMode("MoveMode")
		elseif key.PressedFrame(ACT_EDITOR_ROTATE) then
			edit.ChangeMode("RotateMode")
		elseif key.PressedFrame(ACT_EDITOR_INTENSITY) then
			edit.ChangeToNumberMode("intensity", 0.1, 0.0, math.huge)
		elseif key.PressedFrame(ACT_EDITOR_SUB_PALETTE) then
			edit.ChangeToNumberMode("subPalette", 1, 0, math.huge)
		elseif key.PressedFrame(ACT_EDITOR_COLOR_SMOOTH) then
			edit.ChangeToNumberMode("colorSmooth", 0.05, 0.0, 1.0)
		elseif key.PressedFrame(ACT_EDITOR_COLOR_PRIORITY) then
			edit.ChangeToNumberMode("colorPriority", 0.1, 0.0, math.huge)
		elseif key.PressedFrame(ACT_EDITOR_RADIUS) then
			edit.ChangeToNumberMode("radius", 16.0, 0.0, math.huge)
		elseif key.PressedFrame(ACT_EDITOR_EXPONENT) then
			edit.ChangeToNumberMode("exponent", 0.2, 0.0, math.huge)
		elseif key.PressedFrame(ACT_EDITOR_OUTER_ANGLE) then
			edit.ChangeToNumberMode("outer", edit.nRotateStep, 0.0, math.pi * 0.5, nil, math.pi, math.rad, math.deg)
		elseif key.PressedFrame(ACT_EDITOR_INNER_ANGLE) then
			edit.ChangeToNumberMode("inner", edit.nRotateStep, 0.0, math.pi * 0.5, nil, math.pi, math.rad, math.deg)
		end
		
	end
	
	local tNearby = edit.tNearby
	local v3Cam = cam:FinalPos()
	edit.NearbyBulbs(tNearby, v3Cam, edit.nNearbyRadius)
	edit[edit.sMode]()
	local blbSelected = edit.blbSelected
	local tInfoTexts = edit.tInfoTexts
	local v2Vid = wrp.VideoWidth(), wrp.VideoHeight()
	
	if scn_BulbExists(blbSelected) then
		
		edit.DrawBulb(blbSelected, edit.iSelectedColor, 3)
		tInfoTexts[1]:SetString(string.format("\"%s\"", edit.BulbName(blbSelected)))
		
		tInfoTexts[2]:SetString(string.format("[%s] x %.2f y %.2f z %.2f",
				key.ActionBoundString(ACT_EDITOR_MOVE), blbSelected:Pos()))
		
		local nPit, nYaw = qua.PitchYaw(blbSelected:Ori())
		
		tInfoTexts[3]:SetString(string.format("[%s] pitch %.2f yaw %.2f",
				key.ActionBoundString(ACT_EDITOR_ROTATE), math.deg(nPit), math.deg(nYaw)))
		
		tInfoTexts[4]:SetString(string.format("[%s] intensity %.2f",
				key.ActionBoundString(ACT_EDITOR_INTENSITY), blbSelected:Intensity()))
		
		tInfoTexts[5]:SetString(string.format("[%s] sub-palette %d",
				key.ActionBoundString(ACT_EDITOR_SUB_PALETTE), blbSelected:SubPalette()))
		
		tInfoTexts[6]:SetString(string.format("[%s] color smooth %.2f",
				key.ActionBoundString(ACT_EDITOR_COLOR_SMOOTH), blbSelected:ColorSmooth()))
		
		tInfoTexts[7]:SetString(string.format("[%s] color priority %.2f",
				key.ActionBoundString(ACT_EDITOR_COLOR_PRIORITY), blbSelected:ColorPriority()))
		
		tInfoTexts[8]:SetString(string.format("[%s] radius %.2f",
				key.ActionBoundString(ACT_EDITOR_RADIUS), blbSelected:Radius()))
		
		tInfoTexts[9]:SetString(string.format("[%s] exponent %.2f",
				key.ActionBoundString(ACT_EDITOR_EXPONENT), blbSelected:Exponent()))
		
		tInfoTexts[10]:SetString(string.format("[%s] outer %.2f [%s] inner %.2f",
				key.ActionBoundString(ACT_EDITOR_OUTER_ANGLE), math.deg(blbSelected:Outer()),
				key.ActionBoundString(ACT_EDITOR_INNER_ANGLE), math.deg(blbSelected:Inner())))
		
		for i = 1, #tInfoTexts do
			
			local iLen = tInfoTexts[i]:StringLength()
			tInfoTexts[i]:SetPos(xVid - iLen * 8, yVid * 0.5 - (i - 5) * 10, 1.0)
			tInfoTexts[i]:SetVisible(true)
			
		end
		
	else
		
		for i = 1, #tInfoTexts do
			tInfoTexts[i]:SetVisible(false)
		end
		
	end
	
	local txtMode = edit.txtMode
	txtMode:SetString(edit.sMode)
	txtMode:SetPos(xVid - txtMode:StringLength() * 8, yVid * 0.5 + 60, 1.0)
	local txtSet = edit.txtSet
	txtSet:SetPos(xVid - txtSet:StringLength() * 8, yVid * 0.5 - 70, 1.0)
	
	edit.DrawNearbyBulbs()
	
end

--[[------------------------------------
	edit.ToggleEditor
--------------------------------------]]
function edit.ToggleEditor(bForce)
	
	edit.bEditing = bForce == nil and not edit.bEditing or bForce
	local tInfoTexts = edit.tInfoTexts
	
	if edit.bEditing then
		
		if not edit.EnsureLoaded() then
			
			con.LogF("Editor could not initialize")
			edit.bEditing = false
			return
			
		end
		
		con.LogF("Editor on")
		edit.ChangeMode("SelectMode")
		edit.txtMode:SetVisible(true)
		edit.txtSet:SetVisible(true)
		
	else
		
		con.LogF("Editor off")
		edit.blbSelected = nil
		edit.txtMode:SetVisible(false)
		
		for i = 1, #tInfoTexts do
			tInfoTexts[i]:SetVisible(false)
		end
		
		edit.txtSet:SetVisible(false)
		
	end
	
end

--[[------------------------------------
	edit.EnsureLoaded

OUT	bLoaded
--------------------------------------]]
function edit.EnsureLoaded()
	
	local sLevel = rec.CurrentLevel()
	
	if sLevel then
		
		if edit.bLoaded then
			return true
		end
		
	else
		
		con.LogF("No current level to edit")
		edit.bLoaded = false
		return false
		
	end
	
	local tBulbNames = edit.tBulbNames
	for k in pairs(tBulbNames) do tBulbNames[k] = nil end
	edit.iNextName = 0
	edit.sLevelEdit = sLevel
	edit.tFile = rec.LoadJSON(edit.sLevelEdit)
	
	if edit.tFile then
		
		local tFile = edit.tFile
		
		if not tFile["bulbs"] then
			tFile["bulbs"] = {}
		end
		
		for k in pairs(tFile["bulbs"]) do
			
			local blb = scn.BulbFromRecordID(k)
			tBulbNames[blb] = k
			tBulbNames[k] = blb
			
		end
		
		edit.bLoaded = true
		
	else
		edit.bLoaded = false
	end
	
	return edit.bLoaded
	
end

--[[------------------------------------
	edit.SaveEdits
--------------------------------------]]
function edit.SaveEdits()
	
	if not edit.bEditing then
		
		-- To make sure edit state exists and isn't from a previous level
		con.LogF("Must be in editor mode to save edits")
		return
		
	end
	
	rec.SaveJSON(edit.sLevelEdit, edit.tFile)
	
end

--[[------------------------------------
	edit.DrawBulb
--------------------------------------]]
local hulDrawBulb = hit.HullBox(-1, -1, -1, 1, 1, 1)

function edit.DrawBulb(blb, iColor, nSize)
	
	hulDrawBulb:SetBox(-nSize, -nSize, -nSize, nSize, nSize, nSize)
	local v3Selected = blb:Pos()
	hulDrawBulb:DrawBoxWire(v3Selected, 0, 0, 0, 1, iColor)
	
end

--[[------------------------------------
	edit.NearbyBulbs
--------------------------------------]]
function edit.NearbyBulbs(tNearby, v3Pos, nRadius)
	
	for k in pairs(tNearby) do tNearby[k] = nil end
	local blbCur = scn.FirstBulb()
	local nRadiusSq = nRadius * nRadius
	local iNum = 0
	local tBulbNames = edit.tBulbNames
	
	-- Linear search for bulbs to detect those that have no radius or are not in a zone
	while scn_BulbExists(blbCur) do
		
		local v3Blb
		
		if not tBulbNames[blbCur] then
			goto continue
		end
		
		v3Blb = blb_Pos(blbCur)
		
		if vec_MagSq3(v3Blb - v3Pos) > nRadiusSq then
			goto continue
		end
		
		iNum = iNum + 1
		tNearby[iNum] = blbCur
		
		::continue::
		
		blbCur = blb_Next(blbCur)
		
	end
	
end

--[[------------------------------------
	edit.DrawNearbyBulbs
--------------------------------------]]
function edit.DrawNearbyBulbs()
	
	local tNearby = edit.tNearby
	local edit_DrawBulb = edit.DrawBulb
	local blbSelected = edit.blbSelected
	local iNormalColor = edit.iNormalColor
	
	for i = 1, #tNearby do
		
		-- FIXME: draw darker color if out of LOS
		local blb = tNearby[i]
		
		if scn_BulbExists(blb) and blb ~= blbSelected then
			edit_DrawBulb(blb, iNormalColor, 2)
		end
		
	end
	
end

--[[------------------------------------
	edit.TargetedBulb

OUT	blbTargeted
--------------------------------------]]
function edit.TargetedBulb(v3Pos, v3Dir)
	
	local nWldDiag = nWldDiag
	local v3End = v3Pos + v3Dir * nWldDiag
	local hulTarget = edit.hulTarget
	local blbSelected = edit.blbSelected
	
	local tNearby = edit.tNearby
	local nBestTime = math_huge
	local blbBest = nil
	local bAllowEqual = false
	
	for i = 1, #tNearby do
		
		local blb = tNearby[i]
		local v3Blb = blb_Pos(blb)
		local iC, nTF = hit_TestLineHull(v3Pos, v3End, hulTarget, v3Blb, 0, 0, 0, 1)
		
		if iC == hit_NONE then
			nTF = nil
		elseif iC == hit_INTERSECT then
			nTF = 0.0
		end
		
		local bClippingSelect = false
		
		if bAllowEqual and nTF == nBestTime then
			
			bClippingSelect = true
			bAllowEqual = false
			
		end
		
		if nTF and (nTF < nBestTime or bClippingSelect) then
			
			nBestTime = nTF
			blbBest = blb
			
		end
		
		-- FIXME: allow clip-targeting at similar position, within some radius
		-- FIXME: cycle needs to wrap
		if blb == blbSelected then
			bAllowEqual = true -- Allow targeting of next bulb in list at same position
		end
		
	end
	
	return blbBest
	
end

--[[------------------------------------
	edit.BulbName

OUT	sName
--------------------------------------]]
function edit.BulbName(blb)
	
	return edit.tBulbNames[blb] or edit.NewBulbName(blb)
	
end

--[[------------------------------------
	edit.NewBulbName

OUT	sName
--------------------------------------]]
function edit.NewBulbName(blb)
	
	local i = edit.iNextName
	local tBulbNames = edit.tBulbNames
	local sName
	
	while true do
		
		sName = tostring(i)
		i = i + 1
		
		if not tBulbNames[sName] then
			break
		end
		
	end
	
	edit.iNextName = i
	tBulbNames[blb] = sName
	tBulbNames[sName] = blb
	return sName
	
end

--[[------------------------------------
	edit.BulbTable

OUT	tBulb
--------------------------------------]]
function edit.BulbTable(blb)
	
	local tFile = edit.tFile
	local sName = edit.BulbName(blb)
	local tBulb = tFile["bulbs"][sName]
	
	if not tBulb then
		
		tBulb = {}
		tFile["bulbs"][sName] = tBulb
		
	end
	
	return tBulb
	
end

--[[------------------------------------
	edit.UpdateTypedValue

OUT	bNewLine
--------------------------------------]]
function edit.UpdateTypedValue()
	
	local sTypedValue = edit.sTypedValue
	local iKBOff = 0
	local sKB = key.KeyBufferFrame()
	local bNewLine = false
	
	while true do
		
		sTypedValue, iKBOff = key_ApplyKeyBufferFrame(sTypedValue, "\n-", ".0123456789", iKBOff)
		local iDelim = string_byte(sKB, iKBOff + 1)
		
		if iDelim == 45 --[[ '-' ]] then
			
			if string_byte(sTypedValue, 1) == 45 --[[ '-' ]] then
				sTypedValue = sTypedValue:sub(2)
			else
				sTypedValue = '-' .. sTypedValue
			end
			
			iKBOff = iKBOff + 1
			
		elseif iDelim == 10 --[[ '\n' ]] then
			
			bNewLine = true
			break
			
		else -- End of key buffer
			break
		end
		
	end
	
	edit.sTypedValue = sTypedValue
	return bNewLine
	
end

--[[------------------------------------
	edit.SpawnBulb

OUT	blbNew
--------------------------------------]]
function edit.SpawnBulb()
	
	local cam = scn.ActiveCamera()
	local v3Cam = cam:FinalPos()
	local v3Dir = qua.Dir(cam:FinalOri())
	local blbNew = scn.CreateBulb(v3Cam + v3Dir * 256, 256, 2, 1, 0)
	local tBulb = edit.BulbTable(blbNew)
	local v3Pos = blbNew:Pos()
	local q4Ori = blbNew:Ori()
	tBulb["pos"] = {xPos, yPos, zPos}
	tBulb["ori"] = {qxOri, qyOri, qzOri, qwOri}
	tBulb["radius"] = blbNew:Radius()
	--tBulb["exponent"] = blbNew:Exponent() -- Uncomment if not equal to engine defaults
	--tBulb["intensity"] = blbNew:Intensity()
	edit.blbSelected = blbNew
	return blbNew
	
end

--[[------------------------------------
	edit.ChangeMode
--------------------------------------]]
function edit.ChangeMode(sWantMode, bFinish)
	
	if not edit[sWantMode] then
		
		con.LogF("No editor mode '%s'", sWantMode)
		edit.ChangeMode("SelectMode", bFinish)
		return
		
	end
	
	local sOldMode = edit.sMode
	
	if sOldMode then
		
		if bFinish then
			
			local Finish = edit["Finish" .. sOldMode]
			
			if Finish then
				Finish()
			end
			
		else
			
			local Cancel = edit["Cancel" .. sOldMode]
			
			if Cancel then
				Cancel()
			end
			
		end
		
	end
	
	local Start = edit["Start" .. sWantMode]
	
	if Start then
		Start()
	end
	
	edit.sMode = sWantMode
	
end

--[[------------------------------------
	edit.SelectMode
--------------------------------------]]
function edit.SelectMode()
	
	local cam = scn.ActiveCamera()
	local v3Cam = cam:FinalPos()
	local v3Dir = qua.Dir(cam:FinalOri())
	local blbTargeted = edit.TargetedBulb(v3Cam, v3Dir)
	
	if blbTargeted then
		
		if key.PressedFrame(ACT_EDITOR_CONFIRM) then
			edit.blbSelected = blbTargeted
		else
			edit.DrawBulb(blbTargeted, edit.iTargetedColor, edit.nTargetRadius)
		end
		
	end
	
end

--[[------------------------------------
	edit.StartDeleteMode
--------------------------------------]]
function edit.StartDeleteMode()
	
	edit.txtSet:SetString(nil)
	local blbSelected = edit.blbSelected
	
	if not scn.BulbExists(blbSelected) then
		
		edit.ChangeMode("SelectMode")
		return
		
	end
	
	edit.txtSet:SetString(string.format("Delete '%s'? OK [%s] / Cancel [%s]",
		edit.BulbName(blbSelected), key.ActionBoundString(ACT_EDITOR_CONFIRM),
		key.ActionBoundString(ACT_EDITOR_CANCEL)))
	
end

--[[------------------------------------
	edit.CancelDeleteMode
--------------------------------------]]
function edit.CancelDeleteMode()
	
	edit.txtSet:SetString(nil)
	
end

--[[------------------------------------
	edit.FinishDeleteMode
--------------------------------------]]
function edit.FinishDeleteMode()
	
	edit.txtSet:SetString(nil)
	local blbSelected = edit.blbSelected
	
	if not scn.BulbExists(blbSelected) then
		return
	end
	
	local sName = edit.BulbName(blbSelected)
	local tBulbs = edit.tFile["bulbs"]
	
	if tBulbs then
		tBulbs[sName] = nil
	end
	
	edit.tBulbNames[blbSelected] = nil
	edit.tBulbNames[sName] = true -- Reserve the name, but let the bulb be garbage collected
	blbSelected:Delete()
	edit.blbSelected = nil
	
end

--[[------------------------------------
	edit.DeleteMode
--------------------------------------]]
function edit.DeleteMode()
	
	if key.PressedFrame(ACT_EDITOR_CONFIRM) then
		edit.ChangeMode("SelectMode", true)
	end
	
end

--[[------------------------------------
	edit.StartMoveMode
--------------------------------------]]
function edit.StartMoveMode()
	
	edit.txtSet:SetString(nil)
	local blbSelected = edit.blbSelected
	
	if not scn.BulbExists(blbSelected) then
		
		edit.ChangeMode("SelectMode")
		return
		
	end
	
	local cam = scn.ActiveCamera()
	local v3Cam = cam:FinalPos()
	local v3Dir = qua.Dir(cam:FinalOri())
	local v3Abs = math_abs(xDir), math_abs(yDir), math_abs(zDir)
	
	if xAbs > yAbs then
		
		if xAbs > zAbs then
			edit.iDefaultAxes = 6
		else
			edit.iDefaultAxes = 3
		end
		
	elseif yAbs > zAbs then
		edit.iDefaultAxes = 5
	else
		edit.iDefaultAxes = 3
	end
	
	edit.iCustomAxes = 0
	edit.v3BlbReset = blbSelected:Pos()
	edit.sTypedValue = ""
	
end

--[[------------------------------------
	edit.CancelMoveMode
--------------------------------------]]
function edit.CancelMoveMode()
	
	edit.txtSet:SetString(nil)
	local blbSelected = edit.blbSelected
	
	if not scn.BulbExists(blbSelected) then
		return
	end
	
	blbSelected:SetPos(edit.v3BlbReset)
	blbSelected:SetOldPos(edit.v3BlbReset)
	
end

--[[------------------------------------
	edit.FinishMoveMode
--------------------------------------]]
function edit.FinishMoveMode()
	
	edit.txtSet:SetString(nil)
	local blbSelected = edit.blbSelected
	
	if not scn.BulbExists(blbSelected) then
		return
	end
	
	local v3Pos = blbSelected:Pos()
	edit.BulbTable(blbSelected)["pos"] = {xPos, yPos, zPos}
	
end

--[[------------------------------------
	edit.MoveMode
--------------------------------------]]
function edit.MoveMode()
	
	local blbSelected = edit.blbSelected
	
	if not scn.BulbExists(blbSelected) then
		
		edit.ChangeMode("SelectMode")
		return
		
	end
	
	if key.PressedFrame(ACT_EDITOR_X_AXIS) then edit.iCustomAxes, edit.sTypedValue = edit.iCustomAxes ~ 1, "" end
	if key.PressedFrame(ACT_EDITOR_Y_AXIS) then edit.iCustomAxes, edit.sTypedValue = edit.iCustomAxes ~ 2, "" end
	if key.PressedFrame(ACT_EDITOR_Z_AXIS) then edit.iCustomAxes, edit.sTypedValue = edit.iCustomAxes ~ 4, "" end
	
	local iCustomAxes = edit.iCustomAxes
	local iAxes = (iCustomAxes ~= 0 and iCustomAxes) or edit.iDefaultAxes
	
	local iOldLen = #edit.sTypedValue
	local bNewLine = edit.UpdateTypedValue()
	local iCurLen = #edit.sTypedValue
	
	local cam = scn.ActiveCamera()
	local v3Cam = cam:FinalPos()
	local v3Dir = qua.Dir(cam:FinalOri())
	local v3Reset = edit.v3BlbReset
	local v3Diff = v3Reset - v3Cam
	local edit_DistToPlane = edit.DistToPlane
	local v3New, nTyped, v3Mag, nMag, v3Hit, iProjectColor
	v3Mag = edit_DistToPlane(xDiff, xDir), edit_DistToPlane(yDiff, yDir), edit_DistToPlane(zDiff, zDir)
	
	if iCurLen > 0 then
		nTyped = tonumber(edit.sTypedValue)
	end
	
	if iAxes == 1 then
		
		-- X
		if nTyped then
			
			v3New = nTyped, yReset, zReset
			edit.txtSet:SetString(string.format("x = '%s'; x__", edit.sTypedValue))
			
		else
			
			-- Project ray onto closer of Y and Z planes, then keep only X component
			if yMag < zMag then
				nMag, iProjectColor = yMag, edit.iZColor
			else
				nMag, iProjectColor = zMag, edit.iYColor
			end
			
			if nMag == math.huge then nMag = 0.0 end
			v3Hit = v3Cam + v3Dir * nMag
			v3New = xHit, yReset, zReset
			edit.txtSet:SetString(string.format("x = %.2f; x__", xNew))
			
		end
		
	elseif iAxes == 2 then
		
		-- Y
		if nTyped then
			
			v3New = xReset, nTyped, zReset
			edit.txtSet:SetString(string.format("y = '%s'; _y_", edit.sTypedValue))
			
		else
			
			if xMag < zMag then
				nMag, iProjectColor = xMag, edit.iZColor
			else
				nMag, iProjectColor = zMag, edit.iXColor
			end
			
			if nMag == math.huge then nMag = 0.0 end
			v3Hit = v3Cam + v3Dir * nMag
			v3New = xReset, yHit, zReset
			edit.txtSet:SetString(string.format("y = %.2f; _y_", yNew))
			
		end
		
	elseif iAxes == 3 then
		
		-- XY
		nMag = zMag
		if nMag == math.huge then nMag = 0.0 end
		v3Hit = v3Cam + v3Dir * nMag
		v3New = v2Hit, zReset
		edit.txtSet:SetString(string.format("x = %.2f, y = %.2f; xy_", v2New))
		
	elseif iAxes == 4 then
		
		-- Z
		if nTyped then
			
			v3New = xReset, yReset, nTyped
			edit.txtSet:SetString(string.format("z = '%s'; __z", edit.sTypedValue))
			
		else
			
			if xMag < yMag then
				nMag, iProjectColor = xMag, edit.iYColor
			else
				nMag, iProjectColor = yMag, edit.iXColor
			end
			
			if nMag == math.huge then nMag = 0.0 end
			v3Hit = v3Cam + v3Dir * nMag
			v3New = xReset, yReset, zHit
			edit.txtSet:SetString(string.format("z = %.2f; __z", zNew))
			
		end
		
	elseif iAxes == 5 then
		
		-- XZ
		nMag = yMag
		if nMag == math.huge then nMag = 0.0 end
		v3Hit = v3Cam + v3Dir * nMag
		v3New = xHit, yReset, zHit
		edit.txtSet:SetString(string.format("x = %.2f, z = %.2f; x_z", xNew, zNew))
		
	elseif iAxes == 6 then
		
		-- YZ
		nMag = xMag
		if nMag == math.huge then nMag = 0.0 end
		v3Hit = v3Cam + v3Dir * nMag
		v3New = xReset, yHit, zHit
		edit.txtSet:SetString(string.format("y = %.2f, z = %.2f; _yz", yNew, zNew))
		
	else
		
		-- XYZ
		-- Project v3Dir onto closest of X, Y, and Z planes
		if xMag < yMag then
			
			if xMag < zMag then
				nMag = xMag
			else
				nMag = zMag
			end
			
		else
			
			if yMag < zMag then
				nMag = yMag
			else
				nMag = zMag
			end
			
		end
		
		if nMag == math.huge then nMag = 0.0 end
		v3Hit = v3Cam + v3Dir * nMag
		v3New = v3Hit
		edit.txtSet:SetString(string.format("x = %.2f, y = %.2f, z = %.2f; xyz", v3New))
		
	end
	
	blbSelected:SetPos(v3New)
	blbSelected:SetOldPos(v3New)
	
	rnd.DrawLine(v3Reset, xNew, yReset, zReset, edit.iXColor)
	rnd.DrawLine(xNew, yReset, zReset, xNew, yNew, zReset, edit.iYColor)
	rnd.DrawLine(xNew, yNew, zReset, v3New, edit.iZColor)
	
	rnd.DrawLine(v3New, xReset, yNew, zNew, edit.iXColor)
	rnd.DrawLine(xReset, yNew, zNew, xReset, yReset, zNew, edit.iYColor)
	rnd.DrawLine(xReset, yReset, zNew, v3Reset, edit.iZColor)
	
	if iProjectColor then
		rnd.DrawLine(v3Hit, v3New, iProjectColor)
	end
	
	if key.PressedFrame(ACT_EDITOR_CONFIRM) or bNewLine then
		edit.ChangeMode("SelectMode", true)
	end
	
end

--[[------------------------------------
	edit.DistToPlane

OUT	nDist
--------------------------------------]]
function edit.DistToPlane(nDiff, nDir)
	
	if nDir == 0.0 then
		return math.huge
	end
	
	local nDist = nDiff / nDir
	
	if nDist < 0.0 then
		return math.huge
	end
	
	return nDist
	
end

--[[------------------------------------
	edit.StartRotateMode
--------------------------------------]]
function edit.StartRotateMode()
	
	edit.txtSet:SetString(nil)
	local blbSelected = edit.blbSelected
	
	if not scn.BulbExists(blbSelected) then
		
		edit.ChangeMode("SelectMode")
		return
		
	end
	
	edit.iDefaultAxes = 6
	edit.iCustomAxes = 0
	local q4Blb = blbSelected:Ori()
	edit.q4BlbReset = q4Blb
	edit.nBlbPitSet, edit.nBlbYawSet = qua.PitchYaw(q4Blb)
	edit.sTypedValue = ""
	
end

--[[------------------------------------
	edit.CancelRotateMode
--------------------------------------]]
function edit.CancelRotateMode()
	
	edit.txtSet:SetString(nil)
	local blbSelected = edit.blbSelected
	
	if not scn.BulbExists(blbSelected) then
		return
	end
	
	blbSelected:SetOri(edit.q4BlbReset)
	blbSelected:SetOldOri(edit.q4BlbReset)
	
end

--[[------------------------------------
	edit.FinishRotateMode
--------------------------------------]]
function edit.FinishRotateMode()
	
	edit.txtSet:SetString(nil)
	local blbSelected = edit.blbSelected
	
	if not scn.BulbExists(blbSelected) then
		return
	end
	
	local q4Ori = blbSelected:Ori()
	edit.BulbTable(blbSelected)["ori"] = {qxOri, qyOri, qzOri, qwOri}
	
end

--[[------------------------------------
	edit.RotateMode
--------------------------------------]]
function edit.RotateMode()
	
	local blbSelected = edit.blbSelected
	
	if not scn.BulbExists(blbSelected) then
		
		edit.ChangeMode("SelectMode")
		return
		
	end
	
	if key.PressedFrame(ACT_EDITOR_Y_AXIS) then edit.iCustomAxes, edit.sTypedValue = edit.iCustomAxes == 2 and 0 or 2, "" end
	if key.PressedFrame(ACT_EDITOR_Z_AXIS) then edit.iCustomAxes, edit.sTypedValue = edit.iCustomAxes == 4 and 0 or 4, "" end
	
	local iCustomAxes = edit.iCustomAxes
	local iAxes = (iCustomAxes ~= 0 and iCustomAxes) or edit.iDefaultAxes
	
	local iOldLen = #edit.sTypedValue
	local bNewLine = edit.UpdateTypedValue()
	local iCurLen = #edit.sTypedValue
	
	local cam = scn.ActiveCamera()
	local v3Cam = cam:FinalPos()
	local v3Dir = qua.Dir(cam:FinalOri())
	local q4New
	
	if iAxes == 6 then
		
		local v3End = v3Cam + v3Dir * edit.nNearbyRadius
		local iC, nTF = hit.LineTest(v3Cam, v3End, hit.TREE, nil, nil)
		
		if iC == hit.NONE then
			nTF = 1.0
		elseif iC == hit.INTERSECT then
			nTF = 0.0
		end
		
		local v3Hit = v3Cam + (v3End - v3Cam) * nTF
		local v3Blb = blbSelected:Pos()
		local nPit, nYaw = vec.PitchYaw(v3Hit - v3Blb)
		q4New = qua.QuaPitchYaw(nPit, nYaw)
		local v3Side = vec.VecPitchYaw(0.0, nYaw + math.pi * 0.5)
		local v3Up = vec.VecPitchYaw(nPit - math.pi * 0.5, nYaw)
		local nCone = 16.0
		local v3Cone1 = v3Hit + v3Side * nCone
		local v3Cone2 = v3Hit - v3Side * nCone
		local v3Cone3 = v3Hit - v3Up * nCone
		local v3Cone4 = v3Hit + v3Up * nCone
		rnd.DrawLine(v3Blb, v3Cone1, 72)
		rnd.DrawLine(v3Blb, v3Cone2, 120)
		rnd.DrawLine(v3Cone1, v3Cone2, edit.iRotateColor)
		rnd.DrawLine(v3Blb, v3Cone3, 96)
		rnd.DrawLine(v3Blb, v3Cone4, edit.iRotateColor)
		rnd.DrawLine(v3Cone3, v3Cone4, edit.iRotateColor)
		
		edit.txtSet:SetString(string.format("pitch = %.2f, yaw = %.2f; aiming", math.deg(nPit),
			math.deg(nYaw)))
		
	else
		
		local sVar, sVarName, AngFunc
		
		if iAxes == 2 then
			sVar, sVarName, AngFunc = "nBlbPitSet", "pitch", qua.Pitch
		else -- iAxes == 4
			sVar, sVarName, AngFunc = "nBlbYawSet", "yaw", qua.Yaw
		end
		
		if iCurLen > 0 then
			
			local nConvert = tonumber(edit.sTypedValue)
			edit[sVar] = nConvert and math.rad(nConvert) or AngFunc(edit.q4BlbReset)
			edit.txtSet:SetString(string.format("%s = '%s'; adjusting", sVarName, edit.sTypedValue))
			
		else
			
			if iOldLen > 0 then
				
				local AngFunc = iAxes == 2 and qua.Pitch or qua.Yaw
				edit[sVar] = AngFunc(edit.q4BlbReset)
				
			end
			
			if key.PressedFrame(ACT_EDITOR_INCREASE) then
				edit[sVar] = edit[sVar] + edit.nRotateStep
			end
			
			if key.PressedFrame(ACT_EDITOR_DECREASE) then
				edit[sVar] = edit[sVar] - edit.nRotateStep
			end
			
			edit.txtSet:SetString(string.format("%s = %g; adjusting", sVarName,
				math.deg(edit[sVar])))
			
		end
		
		q4New = qua.QuaPitchYaw(edit.nBlbPitSet, edit.nBlbYawSet)
		
	end
	
	blbSelected:SetOri(q4New)
	blbSelected:SetOldOri(q4New)
	
	if key.PressedFrame(ACT_EDITOR_CONFIRM) or bNewLine then
		edit.ChangeMode("SelectMode", true)
	end
	
end

--[[------------------------------------
	edit.ChangeToNumberMode
--------------------------------------]]
function edit.ChangeToNumberMode(sVar, nStep, nMin, nMax, nMinClamp, nMaxClamp, InputToActual, ActualToInput)
	
	edit.sWantVar = sVar
	edit.ChangeMode("NumberMode")
	edit.nActiveVarStep = nStep or 1
	edit.nActiveVarMin = nMin or -math.huge
	edit.nActiveVarMax = nMax or math.huge
	edit.nActiveVarMinClamp = nMinClamp or edit.nActiveVarMin
	edit.nActiveVarMaxClamp = nMaxClamp or edit.nActiveVarMax
	edit.ActiveVarInputToActual = InputToActual
	edit.ActiveVarActualToInput = ActualToInput
	
end

--[[------------------------------------
	edit.StartNumberMode
--------------------------------------]]
function edit.StartNumberMode()
	
	edit.txtSet:SetString(nil)
	local blbSelected = edit.blbSelected
	
	if not scn.BulbExists(blbSelected) then
		
		edit.ChangeMode("SelectMode")
		return
		
	end
	
	edit.sActiveVar = edit.sWantVar
	local nNum = edit.tNumVarGetters[edit.sActiveVar](blbSelected)
	edit.nActiveVarValue = nNum
	edit.nActiveVarReset = nNum
	edit.sTypedValue = ""
	
end

--[[------------------------------------
	edit.CancelNumberMode
--------------------------------------]]
function edit.CancelNumberMode()
	
	edit.txtSet:SetString(nil)
	local blbSelected = edit.blbSelected
	
	if not scn.BulbExists(blbSelected) then
		return
	end
	
	local sVar = edit.sActiveVar
	local nReset = edit.nActiveVarReset
	edit.tNumVarSetters[sVar](blbSelected, nReset)
	local OldSetter = edit.tNumVarOldSetters[sVar]
	
	if OldSetter then
		OldSetter(blbSelected, nReset)
	end
	
end

--[[------------------------------------
	edit.FinishNumberMode
--------------------------------------]]
function edit.FinishNumberMode()
	
	edit.txtSet:SetString(nil)
	local blbSelected = edit.blbSelected
	
	if not scn.BulbExists(blbSelected) then
		return
	end
	
	edit.BulbTable(blbSelected)[edit.sActiveVar] = edit.nActiveVarValue
	
end

--[[------------------------------------
	edit.NumberMode
--------------------------------------]]
function edit.NumberMode()
	
	local blbSelected = edit.blbSelected
	
	if not scn.BulbExists(blbSelected) then
		
		edit.ChangeMode("SelectMode")
		return
		
	end
	
	local sVar = edit.sActiveVar
	local nNew = edit.nActiveVarValue
	local iOldLen = #edit.sTypedValue
	local bNewLine = edit.UpdateTypedValue()
	local iCurLen = #edit.sTypedValue
	
	if iCurLen > 0 then
		
		local nConvert = tonumber(edit.sTypedValue)
		local ITA = edit.ActiveVarInputToActual
		nNew = nConvert and (ITA and ITA(nConvert) or nConvert) or edit.nActiveVarValue
		
	else
		
		if iOldLen > 0 then
			nNew = edit.nActiveVarReset
		end
		
		if key.RepeatedFrame(ACT_EDITOR_INCREASE) then
			
			if nNew == edit.nActiveVarMinClamp then
				nNew = edit.nActiveVarMin
			end
			
			nNew = nNew + edit.nActiveVarStep
			
		end
		
		if key.RepeatedFrame(ACT_EDITOR_DECREASE) then
			
			if nNew == edit.nActiveVarMaxClamp then
				nNew = edit.nActiveVarMax
			end
			
			nNew = nNew - edit.nActiveVarStep
			
		end
		
	end
	
	if nNew <= edit.nActiveVarMin then
		nNew = edit.nActiveVarMinClamp
	elseif nNew >= edit.nActiveVarMax then
		nNew = edit.nActiveVarMaxClamp
	end
	
	local ATI = edit.ActiveVarActualToInput
	local nDisplay = ATI and ATI(nNew) or nNew
	
	if iCurLen > 0 then
		
		edit.txtSet:SetString(string.format("%s = '%s' => %g", edit.sActiveVar, edit.sTypedValue,
			nDisplay))
		
	else
		edit.txtSet:SetString(string.format("%s = %g", edit.sActiveVar, nDisplay))
	end
	
	edit.nActiveVarValue = nNew
	edit.tNumVarSetters[sVar](blbSelected, nNew)
	local OldSetter = edit.tNumVarOldSetters[sVar]
	
	if OldSetter then
		OldSetter(blbSelected, nNew)
	end
	
	if key.PressedFrame(ACT_EDITOR_CONFIRM) or bNewLine then
		edit.ChangeMode("SelectMode", true)
	end
	
end

con.CreateCommand("editor", edit.ToggleEditor)
con.CreateCommand("save_edits", edit.SaveEdits)

return edit