-- common.lua
LFV_EXPAND_VECTORS()

local hit = ghit
local qua = gqua
local rnd = grnd
local scn = gscn
local scr = gscr
local vec = gvec
local wrp = gwrp

local com = gcom

com.iNumEntFlags = 0
local tConversions = {}

local math_abs = math.abs
local math_acos = math.acos
local math_floor = math.floor
local math_huge = math.huge
local math_max = math.max
local math_min = math.min
local math_pi = math.pi
local math_random = math.random
local math_sin = math.sin
local vec_Dot3 = vec.Dot3
local vec_Mag3 = vec.Mag3
local wrp_TimeStep = wrp.TimeStep

--[[------------------------------------
	com.SafeArcCos
--------------------------------------]]
function com.SafeArcCos(n)
	
	if n >= 1.0 then
		return 0.0
	elseif n <= -1.0 then
		return math_pi
	end
	
	local ac = math_acos(n)
	
	if ac ~= ac then
		return 0.0
	else
		return ac
	end
	
end

--[[------------------------------------
	com.StrToNums

OUT	[nNum], ...
--------------------------------------]]
function com.StrToNums(s)
	
	local i = 0
	
	for sMatch in string.gmatch(s, "[^%s]+") do
		
		i = i + 1
		tConversions[i] = tonumber(sMatch)
		
	end
	
	if i > 0 then
		return table.unpack(tConversions, 1, i)
	end
	
end

--[[------------------------------------
	com.StrToBool
--------------------------------------]]
function com.StrToBool(s)
	
	if s == nil then
		return nil
	end
	
	if s == "0" or string.lower(s) == "false" then
		return false
	elseif s == "1" or string.lower(s) == "true" then
		return true
	else
		error(string.format("Invalid bool string '%s'", s))
	end
	
end

--[[------------------------------------
	com.NewEntFlag

OUT	iFlag

FIXME: this is probably a bad idea for loading state; set manual constants
--------------------------------------]]
function com.NewEntFlag()
	
	local iFlag = com.iNumEntFlags
	com.iNumEntFlags = com.iNumEntFlags + 1
	return iFlag
	
end

--[[------------------------------------
	com.Lerp

OUT	n
--------------------------------------]]
function com.Lerp(nA, nB, nT)
	
	return nA + (nB - nA) * nT
	
end

--[[------------------------------------
	com.LerpVec

OUT	x, y, z

FIXME: put nT first so last vector can be function call
--------------------------------------]]
function com.LerpVec(xA, yA, zA, xB, yB, zB, nT)
	
	return xA + (xB - xA) * nT, yA + (yB - yA) * nT, zA + (zB - zA) * nT
	
end

--[[------------------------------------
	com.ScaledLerpFraction

OUT	nSF

Returns nSF so that nA = Lerp(nA, nB, nSF) equals nA = Lerp(nA, nB, nF) done nScale times.
nF = [0.0, 1.0).
--------------------------------------]]
function com.ScaledLerpFraction(nF, nScale)
	
	return 1.0 - (1.0 - nF) ^ nScale
	
end

--[[------------------------------------
	com.Clamp

OUT	nClamped
--------------------------------------]]
function com.Clamp(n, min, max)
	
	if n > max then
		return max
	elseif n < min then
		return min
	else
		return n
	end
	
end

--[[------------------------------------
	com.ClampMag

OUT	nClamped
--------------------------------------]]
function com.ClampMag(n, max)
	
	if n > max then
		return max
	elseif n < -max then
		return -max
	else
		return n
	end
	
end

--[[------------------------------------
	com.SubMag

OUT	nNewVal
--------------------------------------]]
function com.SubMag(nVal, nSub)
	
	if nVal == 0.0 then
		return nVal
	elseif nVal > 0.0 then
		return math_max(0.0, nVal - nSub)
	elseif nVal < 0.0 then
		return math_min(nVal + nSub, 0.0)
	end
	
end

--[[------------------------------------
	com.WrapAngle

OUT nAng

Range [-PI, PI]
--------------------------------------]]
function com.WrapAngle(nAng)
	
	if nAng > math.pi then
		
		local nWrap = math.fmod(nAng, math.pi * 2.0)
		
		if nWrap > math.pi then
			return nWrap - math.pi * 2.0
		else
			return nWrap
		end
		
	elseif nAng < -math.pi then
		
		local nWrap = math.fmod(nAng, math.pi * 2.0)
		
		if nWrap < -math.pi then
			return nWrap + math.pi * 2.0
		else
			return nWrap
		end
		
	else
		return nAng
	end
	
end

--[[------------------------------------
	com.PitchYawDist

OUT nAngularDist
--------------------------------------]]
function com.PitchYawDist(nTarPit, nTarYaw, nPit, nYaw)
	
	return math_max(math.abs(nTarPit - nPit), math.abs(com.WrapAngle(nTarYaw - nYaw)))
	
end

--[[------------------------------------
	com.ResizeVec

OUT	xRes, yRes, zRes

Returns given vector resized to nNewMag. If given vector's current magnitude is 0.0, returns
zero vector if bAllowZero is true and nil otherwise.

FIXME: delete, replace all calls with vec.Resized2 and Resized3
--------------------------------------]]
function com.ResizeVec(x, y, z, nNewMag, bAllowZero)
	
	local nMag = vec.Mag3(x, y, z)
	
	if nMag == 0.0 then
		
		if bAllowZero then
			return 0.0, 0.0, 0.0
		end
		
		return
		
	end
	
	local nFactor = nNewMag / nMag
	return x * nFactor, y * nFactor, z * nFactor
	
end

--[[------------------------------------
	com.ClampVecMag

OUT	v3Clamped

Returns given vector, resized to nMaxMag if its current mag is larger.
--------------------------------------]]
function com.ClampVecMag(v3Orig, nMaxMag)
	
	local nMag = vec_Mag3(v3Orig)
	
	if nMag > nMaxMag then
		
		local nMul = nMaxMag / nMag
		return v3Orig * nMul
		
	end
	
	return v3Orig
	
end

--[[------------------------------------
	com.EnsureDir

OUT	nVal
--------------------------------------]]
function com.EnsureDir(nVal, nNew)
	
	if nNew == 0.0 then
		return nVal
	elseif nNew > 0.0 then
		return nVal > nNew and nVal or nNew
	else
		return nVal < nNew and nVal or nNew
	end
	
end

--[[------------------------------------
	com.Ref

OUT	iRef

Mimics Lua 5.3's luaL_ref.

A sequence is always maintained; code can iterate over [1, #t] so long as numbers are ignored.
Mid-sequence integers are used to maintain a linked list of free references.

The primary purpose of this is to have a method of adding and removing table items while
maintaining a deterministic iteration and insertion order that can be saved and loaded.
--------------------------------------]]
function com.Ref(t, obj)
	
	if not t[0] then
		t[0] = 0 -- New table; initialize linked list
	end
	
	local iRef
	
	if t[0] == 0 then
		
		iRef = #t + 1
		t[iRef] = obj
		
	else
		
		iRef = t[0]
		t[0] = t[iRef]
		t[iRef] = obj
		
	end
	
	return iRef
	
end

--[[------------------------------------
	com.Unref

Mimics Lua 5.3's luaL_unref.
--------------------------------------]]
function com.Unref(t, iRef)
	
	if iRef == #t then
		t[iRef] = nil
	else
		
		t[iRef] = t[0]
		t[0] = iRef
		
	end
	
end

--[[------------------------------------
	com.RefBi

OUT	iRef

Like Ref but also creates a pair objKey : iRef. objKey cannot be an integer and must be unique.

Unreference with UnrefBi.
--------------------------------------]]
function com.RefBi(t, objKey, objVal)
	
	if objVal == nil then
		error(string.format("Tried to reference nil with key %s in %s", objKey, t))
	end
	
	if t[objKey] ~= nil then
		error(string.format("Key %s is already referenced in %s", objKey, t))
	end
	
	local iRef = com.Ref(t, objVal)
	t[objKey] = iRef
	return iRef
	
end

--[[------------------------------------
	com.UnrefBi

Unreferences binary reference from t.
--------------------------------------]]
function com.UnrefBi(t, objKey)
	
	if t[objKey] == nil then
		error(string.format("objKey %s is not referenced in table %s", objKey, t))
	end
	
	com.Unref(t, t[objKey])
	t[objKey] = nil
	
end

--[[------------------------------------
	com.DerefBi

OUT	objVal
--------------------------------------]]
function com.DerefBi(t, objKey)
	
	return t[t[objKey]]
	
end

--[[------------------------------------
	com.RefIter

OUT	Next

Next returns iRef, t[iRef].
--------------------------------------]]
function com.RefIter(t)
	
	local i = 0
	
	return function()
		
		repeat
			i = i + 1
		until type(t[i]) ~= "number"
		
		if t[i] then
			return i, t[i]
		else
			return
		end
		
	end
	
end

--[[------------------------------------
	com.Smooth

OUT	nVal, nDel
--------------------------------------]]
function com.Smooth(nVal, nTar, nFac, nMin, nMax, nStep, nDif)
	
	nMin = nMin or 0.0
	nMax = nMax or math_huge
	nDif = nDif or nTar - nVal
	local nBase = nDif
	nStep = nStep or wrp_TimeStep()
	local nDel
	
	if nDif > 0.0 then
		
		if nDif < nMin then
			nBase = nMin
		elseif nDif > nMax then
			nBase = nMax
		end
		
		nDel = nBase * nFac * nStep
		
		if nDel > nDif then
			return nTar, nDif
		end
		
	elseif nDif < 0.0 then
		
		if -nDif < nMin then
			nBase = -nMin
		elseif -nDif > nMax then
			nBase = -nMax
		end
		
		nDel = nBase * nFac * nStep
		
		if nDel < nDif then
			return nTar, nDif
		end
		
	else
		return nVal, 0.0
	end
	
	return nVal + nDel, nDel
	
end

--[[------------------------------------
	com.SmoothVec

OUT	v3Val, v3Del

FIXME: Vector expansion should have a cc prefix feature that copies the assigned variable's component prefix
--------------------------------------]]
local com_Smooth = com.Smooth

function com.SmoothVec(v3Val, v3Tar, nFac, nMin, nMax, nStep, v3Dif)
	
	local v3Del
	xVal, xDel = com_Smooth(xVal, xTar, nFac, nMin, nMax, nStep, xDif)
	yVal, yDel = com_Smooth(yVal, yTar, nFac, nMin, nMax, nStep, yDif)
	zVal, zDel = com_Smooth(zVal, zTar, nFac, nMin, nMax, nStep, zDif)
	return v3Val, v3Del
	
end

--[[------------------------------------
	com.SmoothResetEuler

OUT	nRol, nPit, nYaw
--------------------------------------]]
function com.SmoothResetEuler(nRol, nPit, nYaw, nFac, nMin, nMax, nStep)
	
	local nMag = vec_Mag3(nRol, nPit, nYaw)
	
	if nMag > 0.0 then
		
		local nNewMag = com_Smooth(nMag, 0, nFac, nMin, nMax, nStep)
		local nMul = nNewMag / nMag
		nRol = nRol * nMul
		nPit = nPit * nMul
		nYaw = nYaw * nMul
		
	end
	
	return nRol, nPit, nYaw
	
end

--[[------------------------------------
	com.Approach

OUT	nVal
--------------------------------------]]
function com.Approach(nVal, nTar, nDel, nDif)
	
	nDif = nDif or nTar - nVal
	
	if nDif > 0.0 then
		
		if nDel >= nDif then
			return nTar
		end
		
	elseif nDif < 0.0 then
		
		if nDel >= -nDif then
			return nTar
		end
		
		nDel = -nDel
		
	else
		return nVal
	end
	
	return nVal + nDel
	
end

--[[------------------------------------
	com.UnevenApproach

OUT	nVal

Like Approach but with a different delta for moving up or down. Both deltas should be positive.
--------------------------------------]]
function com.UnevenApproach(nVal, nTar, nUpDel, nDownDel, nDif)
	
	nDif = nDif or nTar - nVal
	
	if nDif > 0.0 then
		
		if nUpDel >= nDif then
			return nTar
		end
		
		return nVal + nUpDel
		
	elseif nDif < 0.0 then
		
		if nDownDel >= -nDif then
			return nTar
		end
		
		return nVal - nDownDel
		
	else
		return nVal
	end
	
end

--[[------------------------------------
	com.TimedApproach

OUT	nVal

Like UnevenApproach but slows the rate (should be non-negative, per-second) to avoid reaching
nTar for nTime seconds. A nil rate means infinite. nTime can be 0.
--------------------------------------]]
function com.TimedApproach(nVal, nTar, nUpRate, nDownRate, nTime, nStep, nDif)
	
	nDif = nDif or nTar - nVal
	
	if nDif == 0.0 then
		return nVal
	end
	
	local nRate, iSign
	
	if nDif > 0.0 then
		nRate, iSign = nUpRate, 1
	else
		nRate, iSign = nDownRate, -1
	end
	
	local nAbsDif = nDif * iSign
	
	if nRate then
		
		if nTime > 0.0 then
			nRate = math_min(nRate, nAbsDif / nTime)
		end
		
	elseif nTime > 0.0 then
		nRate = nAbsDif / nTime -- Infinite rate, get to target in nTime
	else
		return nTar -- Infinite rate and 0 time; reach target instantly
	end
	
	local nAbsDel = nRate * (nStep or wrp_TimeStep())
	
	if nAbsDel >= nAbsDif then
		return nTar
	end
	
	return nVal + nAbsDel * iSign
	
end

--[[------------------------------------
	com.PlaceDelta

OUT	xDel, yDel, zDel, xDelO, yDelO, zDelO, wDelO

FIXME: Place stuff should be in gcom
--------------------------------------]]
function com.PlaceDelta(x1, y1, z1, x1O, y1O, z1O, w1O, x2, y2, z2, x2O, y2O, z2O, w2O)
	
	return x2 - x1, y2 - y1, z2 - z1, qua.QuaDelta(x1O, y1O, z1O, w1O, x2O, y2O, z2O, w2O)
	
end

--[[------------------------------------
	com.PlaceRelEnt

OUT	xRel, yRel, zRel, xRelO, yRelO, zRelO, wRelO

Returns given place relative to ent.
--------------------------------------]]
function com.PlaceRelEnt(ent, xPos, yPos, zPos, xOri, yOri, zOri, wOri)
	
	local xEnt, yEnt, zEnt, xEntO, yEntO, zEntO, wEntO = ent:Place()
	local xDel, yDel, zDel, xDelO, yDelO, zDelO, wDelO = com.PlaceDelta(
		xEnt, yEnt, zEnt, xEntO, yEntO, zEntO, wEntO,
		xPos, yPos, zPos, xOri, yOri, zOri, wOri
	)
	
	xDel, yDel, zDel = qua.VecRotInv(xDel, yDel, zDel, xEntO, yEntO, zEntO, wEntO)
	return xDel, yDel, zDel, xDelO, yDelO, zDelO, wDelO
	
end

--[[------------------------------------
	com.DrawCross
--------------------------------------]]
function com.DrawCross(xPos, yPos, zPos, nSize, iX, iY, iZ, nTime)
	
	rnd.DrawLine(xPos - nSize, yPos, zPos, xPos + nSize, yPos, zPos, iX, nTime)
	rnd.DrawLine(xPos, yPos - nSize, zPos, xPos, yPos + nSize, zPos, iY, nTime)
	rnd.DrawLine(xPos, yPos, zPos - nSize, xPos, yPos, zPos + nSize, iZ, nTime)
	
end

--[[------------------------------------
	com.OutsideWorld
--------------------------------------]]
function com.OutsideWorld(xPos, yPos, zPos, nSize)
	
	local xMin, yMin, zMin, xMax, yMax, zMax = scn.WorldBox()
	
	return xPos < xMin - nSize or xPos > xMax + nSize or
		yPos < yMin - nSize or yPos > yMax + nSize or
		zPos < zMin - nSize or zPos > zMax + nSize
	
end

--[[------------------------------------
	com.RandRange
--------------------------------------]]
function com.RandRange(nMin, nMax)
	
	return nMin + math_random() * (nMax - nMin)
	
end

--[[------------------------------------
	com.TempTable

OUT	tTemp

FIXME: pre-optimization, test how garbage collector holds up without this
--------------------------------------]]
local tTempTables = {}
local iNumTempTables = 0

function com.TempTable()
	
	if iNumTempTables > 0 then
		
		local tTemp = tTempTables[iNumTempTables]
		tTempTables[iNumTempTables] = nil
		iNumTempTables = iNumTempTables - 1
		return tTemp
		
	else
		return {}
	end
	
end

--[[------------------------------------
	com.FreeTempTable

FIXME: Clear table with a C function
--------------------------------------]]
function com.FreeTempTable(tTemp)
	
	for k in pairs(tTemp) do tTemp[k] = nil end
	iNumTempTables = iNumTempTables + 1
	tTempTables[iNumTempTables] = tTemp
	
end

--[[------------------------------------
	com.NumTempTables
--------------------------------------]]
function com.NumTempTables()
	return iNumTempTables
end

--[[------------------------------------
	com.ViewPos

OUT	v3Pos
--------------------------------------]]
function com.ViewPos(ent)
	
	if ent.ViewPos then
		return ent:ViewPos()
	else
		return ent:Pos()
	end
	
end

--[[------------------------------------
	com.ParseExplist

OUT	...

Evaluates tostring(explist) as an expression list and returns it.

FIXME: tEnv parameter
--------------------------------------]]
function com.ParseExplist(explist)
	
	if explist == nil or explist == "" then
		return
	end
	
	local Chunk, sErr = scr.LoadString("return " .. tostring(explist), nil, true)
	
	if not Chunk then
		error(string.format("Failed to compile string '%s' as explist (%s)", explist, sErr))
	end
	
	return Chunk()
	
end

--[[------------------------------------
	com.GenMeshHull
--------------------------------------]]
function com.GenMeshHull(msh, nAdd)
	
	if msh then
		
		local v3Min, v3Max = msh:Bounds()
		
		if nAdd then
			
			v3Min = v3Min + nAdd
			v3Max = v3Max + nAdd
			
		end
		
		return hit.HullBox(v3Min, v3Max)
		
	else
		return nil
	end
	
end

--[[------------------------------------
	com.HelpfulEntName
--------------------------------------]]
function com.HelpfulEntName(ent)
	
	return string.format("%s '%s' at %f %f %f", ent:TypeName(), ent:RecordID(), ent:Pos())
	
end

--[[------------------------------------
	com.Lagrange

OUT	y

t is a sequence of 2D points to extrapolate or interpolate, where odd is x and even is y.
--------------------------------------]]
function com.Lagrange(t, x)
	
	local n = #t / 2
	local y = 0.0
	
	for j = 1, n do
		
		local p = t[j * 2]
		
		for k = 1, n do
			
			if k ~= j then
				
				local xk = t[k * 2 - 1]
				p = p * ((x - xk) / (t[j * 2 - 1] - xk))
				
			end
			
		end
		
		y = y + p
		
	end
	
	return y
	
end

--[[------------------------------------
	com.Est3Add

OUT	iEntry

Adds and sorts a data entry into t, a sequence of 4D points (nTime, v3Vector) that can be used
to extrapolate or interpolate.
--------------------------------------]]
function com.Est3Add(t, nTime, v3)
	
	local iLength = #t
	local iStart, iEnd = 1, iLength / 4
	local iInsert
	
	if iEnd == 0 or t[iEnd * 4 - 3] < nTime then
		iInsert = iEnd + 1
	else
		
		while iStart <= iEnd do
			
			local iMid = iStart + math_floor((iEnd - iStart) / 2)
			local nComp = t[iMid * 4 - 3]
			
			if nTime > nComp then
				iStart, iInsert = iMid + 1, iMid
			elseif nTime < nComp then
				
				iEnd = iMid - 1
				iInsert = iEnd
				
			else
				
				-- Replace
				local iMul = iMid * 4
				t[iMul - 3], t[iMul - 2], t[iMul - 1], t[iMul] = nTime, v3
				return iMid
				
			end
			
		end
		
	end
	
	-- Shift
	local iInsertMul = iInsert * 4
	
	for i = iLength, iInsertMul - 3, -1 do
		t[i] = t[i + 4]
	end
	
	-- Insert
	t[iInsertMul - 3], t[iInsertMul - 2], t[iInsertMul - 1], t[iInsertMul] = nTime, v3
	return iInsert
	
end

--[[------------------------------------
	com.Est3Exclude

OUT	iNumEntriesRemoved

Removes entries with nTime < nBefore.
--------------------------------------]]
function com.Est3Exclude(t, nBefore)
	
	local iLength = #t
	
	if iLength == 0 or t[1] >= nBefore then
		return 0
	end
	
	local iStart, iEnd = 1, iLength / 4
	local iDelete
	
	while iStart <= iEnd do
		
		local iMid = iStart + math_floor((iEnd - iStart) / 2)
		local nComp = t[iMid * 4 - 3]
		
		if nBefore > nComp then
			iStart, iDelete = iMid + 1, iMid
		elseif nBefore < nComp then
			
			iEnd = iMid - 1
			iDelete = iEnd
			
		else
			
			iDelete = iMid - 1
			break
			
		end
		
	end
	
	if iDelete == 0 then
		return 0
	end
	
	-- Shift
	local iDeleteMul = iDelete * 4
	
	for i = 1, iLength do
		t[i] = t[i + iDeleteMul]
	end
	
	return iDelete
	
end

--[[------------------------------------
	com.Est3Lagrange

OUT	v3Estimate

FIXME: can something be preprocessed for getting multiple estimates from the same table?
--------------------------------------]]
function com.Est3Lagrange(t, nTime)
	
	local iNumEntries = #t / 4
	local v3Estimate = 0.0, 0.0, 0.0
	
	for j = 1, iNumEntries do
		
		local iZ = j * 4
		local v3Add = t[iZ - 2], t[iZ - 1], t[iZ]
		local nTimeJ = t[iZ - 3]
		
		for k = 1, iNumEntries do
			
			local iZK = k * 4
			
			if k ~= j then
				
				local nTimeK = t[iZK - 3]
				local nMul = (nTime - nTimeK) / (nTimeJ - nTimeK)
				v3Add = v3Add * nMul
				
			end
			
		end
		
		v3Estimate = v3Estimate + v3Add
		
	end
	
	return v3Estimate
	
end

--[[------------------------------------
	com.InterceptTime

OUT	[nTime]
--------------------------------------]]
function com.InterceptTime(v3PosA, nVelA, v3PosB, v3VelB)
	
	--[[
	There's a triangle ABC connecting v3PosA (A), v3PosB (B), the intercept point (C)
	
	Triangle lengths:
	a = nVelB * nTime
	b = nVelA * nTime
	c = mag(v3PosA - v3PosB) = nDif
	
	A quadratic equation in terms of nTime can be derived from the law of cosines.
	
	b^2 = c^2 + a^2 - 2ca * cos(B)
	0 = c^2 + a^2 - b^2 - 2ca * cos(B)
	0 = nDif^2 + nVelB^2 * nTime^2 - nVelA^2 * nTime^2 - 2 * nDif * nVelB * nTime * cos(B)
	0 = (nVelB^2 - nVelA^2) * nTime^2 - 2 * nDif * nVelB * cos(B) * nTime + nDif^2
	0 = nQuadratic * nTime^2 + nLinear * nTime + nConstant
	
	Then solve for nTime.
	--]]
	
	-- Get coefficients for quadratic equation
	local v3Dif = v3PosA - v3PosB
	local nDif = vec.Mag3(v3Dif)
	
	if nDif == 0.0 then
		return 0.0 -- A is already at B
	end
	
	if nVelA == 0.0 then
		return nil -- A can't move
	end
	
	local nConstant = nDif * nDif
	local nVelB = vec.Mag3(v3VelB)
	
	if nVelB == 0.0 then
		return nDif / nVelA -- B is not moving
	end
	
	local nQuadratic = nVelB * nVelB - nVelA * nVelA
	local nLinear = -2.0 * vec.Dot3(v3Dif, v3VelB) -- Equal to -2 * nDif * nVelB * cos(B)
	
	-- Solve for time
	if nQuadratic == 0.0 then
		
		-- A and B are moving at same speed, degenerated to a linear equation
		if nLinear == 0.0 then
			return nil -- Invalid equation (0 != nConstant), A can't intercept B
		else
			
			local nTime = -nConstant / nLinear -- Solve linear equation
			
			if nTime < 0.0 then
				return nil -- B would need to go back in time for A to intercept it
			end
			
			return nTime
			
		end
		
	end
	
	-- Use quadratic formula
	local nDiscriminant = nLinear * nLinear - 4.0 * nQuadratic * nConstant
	
	if nDiscriminant < 0.0 then
		return nil -- A can't intercept B
	end
	
	local nSqrt = math.sqrt(nDiscriminant)
	local nDiv = 2.0 * nQuadratic
	local nTimeAdd = (nSqrt - nLinear) / nDiv
	local nTimeSub = (-nLinear - nSqrt) / nDiv
	
	-- Choose smallest non-negative time
	if nTimeAdd >= 0.0 then
		
		if nTimeSub < 0.0 then
			return nTimeAdd
		else
			return nTimeAdd <= nTimeSub and nTimeAdd or nTimeSub
		end
		
	else
		return nTimeSub >= 0.0 and nTimeSub or nil
	end
	
end

--[[------------------------------------
	com.ProjectPointLine

OUT	[nF]

FIXME: give scripts access to engine-side function
--------------------------------------]]
function com.ProjectPointLine(v3A, v3B, v3P)
	
	local v3U = v3B - v3A
	local nO = vec.Dot3(v3U, v3A)
	local nOB = vec.Dot3(v3U, v3B) - nO
	return nOB ~= 0.0 and (vec.Dot3(v3U, v3P) - nO) / nOB or nil
	
end

--[[------------------------------------
	com.HorizontalFOV

OUT	nHFOV

FIXME: give scripts access to engine-side function
--------------------------------------]]
function com.HorizontalFOV(nVFOV, nAspect)
	
	return 2.0 * math.atan(math.tan(nVFOV * 0.5) * nAspect)
	
end

--[[------------------------------------
	com.DrawCircle
--------------------------------------]]
function com.DrawCircle(v3Center, nRadius, v3Normal, iColor, nTime)
	
	v3Normal = vec.Normalized3(v3Normal)
	local v3Hori, v3Vert
	
	if math_abs(xNormal) <= 0.99 then
		v3Hori = vec.Normalized3(vec.Cross(v3Normal, 1, 0, 0))
	else
		v3Hori = vec.Normalized3(vec.Cross(v3Normal, 0, 1, 0))
	end
	
	v3Vert = vec.Normalized3(vec.Cross(v3Hori, v3Normal))
	local v3PrevPoint = v3Center + v3Hori * nRadius
	
	for nPointAngle = 0.1, math.pi * 2.0 + 0.05, 0.1 do
		
		local nPointHori = math.cos(nPointAngle) * nRadius
		local nPointVert = math.sin(nPointAngle) * nRadius
		local v3Point = v3Center + v3Hori * nPointHori + v3Vert * nPointVert
		rnd.DrawLine(v3PrevPoint, v3Point, iColor, nTime)
		v3PrevPoint = v3Point
		
	end
	
end

--[[------------------------------------
	com.DrawTurnCircle
--------------------------------------]]
function com.DrawTurnCircle(v3Pos, v3OldDir, v3NewDir, nRadius, iColor, nTime)
	
	v3OldDir = vec.Normalized3(v3OldDir)
	local v3Axis, nAngle = vec.DirDelta(v3OldDir, v3NewDir)
	local v3Away = vec.Cross(v3OldDir, v3Axis)
	local v3TurnCenter = v3Pos - v3Away * nRadius
	com.DrawCircle(v3TurnCenter, nRadius, v3Axis, iColor, nTime)
	
end

--[[------------------------------------
	com.ApproachDir

OUT	nVal
--------------------------------------]]
function com.ApproachDir(v3Cur, v3Tar, nDel)
	
	local v3Axis, nAngle = vec.DirDelta(v3Cur, v3Tar)
	nAngle = math_min(nAngle, nDel)
	return qua.VecRot(v3Cur, qua.QuaAxisAngle(v3Axis, nAngle))
	
end

--[[------------------------------------
	com.SignedPow

OUT	nValPow
--------------------------------------]]
function com.SignedPow(nVal, nPow)
	
	if nVal >= 0.0 then
		return nVal ^ nPow
	else
		return -(math_abs(nVal) ^ nPow)
	end
	
end

--[[------------------------------------
	com.SignedVecPow

OUT	v3ValPow
--------------------------------------]]
local com_SignedPow = com.SignedPow

function com.SignedVecPow(v3Val, nPow)
	
	return com_SignedPow(xVal, nPow), com_SignedPow(yVal, nPow), com_SignedPow(zVal, nPow)
	
end

--[[------------------------------------
	com.Random3

OUT	nRandom

Output is [0, 1). nSeed should probably be [0, 1].
--------------------------------------]]
function com.Random3(v3Pos, nSeed)
	
	nSeed = nSeed + 1.0 -- Prevent seed from being near zero
	
	return (math_sin(vec_Dot3(v3Pos, 125.9898 + nSeed * 3.5, 78.2333 - nSeed * 6.0, 512.5841 + nSeed) + nSeed * 4517.43) * 43758.5453 * nSeed) % 1.0
	
end

--[[------------------------------------
	com.RandomVec3

OUT	v3Random

Each component is [0, 1).
--------------------------------------]]
function com.RandomVec3(v3Pos, nSeed)
	
	nSeed = nSeed + 1.0 -- Prevent seed from being near zero
	
	local v3D =
		vec_Dot3(v3Pos, 125.9898 + nSeed * 3.5, 78.2333 - nSeed * 6.0, 512.5841 + nSeed) + nSeed * 4517.43,
		vec_Dot3(v3Pos, 58.0317 - nSeed * 2.6, 934.7379 + nSeed * 1.4, 314.1592 - nSeed * 9.3) + nSeed * 1820.19,
		vec_Dot3(v3Pos, 698.8524 + nSeed * 6.7, 426.6868 - nSeed * 4.7, 175.9579 + nSeed * 7.9) - nSeed * 6599.94
	
	local nFactor = 43758.5453 * nSeed
	return (math_sin(xD) * nFactor) % 1.0, (math_sin(yD) * nFactor) % 1.0, (math_sin(zD) * nFactor) % 1.0
	
end

--[[------------------------------------
	com.SimplexNoise3

OUT	nNoise

Based on code by Stefan Gustavson and Peter Eastman.
	https://weber.itn.liu.se/~stegu/simplexnoise/simplexnoise.pdf

FIXME: Try different nFac powers to see if it feels better for turbulence? Could also be a
	parameter but the sum multiplier has to be figured out to get a [-1, 1] range
--------------------------------------]]
local com_RandomVec3 = com.RandomVec3

function com.SimplexNoise3(v3Pos, nSeed)
	
	-- The entire 3D space contains a deformed grid of cube cells
	-- Each cell contains 6 tetrahedra/simplices
	-- Positions can be put into "skewed space" by adding sum of components divided by 3
		-- This makes the cells look like cubes numerically; every cell is a 1x1x1 cube
	-- Positions can be put into "original space" by subtracting sum of components divided by 6
	
	-- Get v3Pos in skewed space
	local nSkewPosAddend = (xPos + yPos + zPos) * 0.33333333333333333333333333333333
	local v3SkewedPos = v3Pos + nSkewPosAddend
	
	--[[
	In original space, find v3Pos relative to the four vertices of the tetrahedron that v3Pos is in
	]]
	
	-- Origin of the cell (0, 0, 0) is connected to every tetrahedron
	local v3SkewedCellOrigin = math_floor(xSkewedPos), math_floor(ySkewedPos), math_floor(zSkewedPos)
	local nUnskewOriginSubtrahend = (xSkewedCellOrigin + ySkewedCellOrigin + zSkewedCellOrigin) * 0.16666666666666666666666666666667
	local v3PosFromOrigin = v3Pos - v3SkewedCellOrigin + nUnskewOriginSubtrahend
	
	-- Vertex opposite of origin (1, 1, 1) is also connected to every tetrahedron
	local v3PosFromFar = v3PosFromOrigin - 0.5 -- == v3PosFromOrigin - 1.0 + (1.0 + 1.0 + 1.0) / 6.0
	
	-- Third and fourth vertices require us to know what simplex we're in
	local v3SkewedThirdFromOrigin, v3SkewedFourthFromOrigin
	
	if xPosFromOrigin >= yPosFromOrigin then
		
		if yPosFromOrigin >= zPosFromOrigin then
			v3SkewedThirdFromOrigin, v3SkewedFourthFromOrigin = 1, 1, 0, 1, 0, 0
		elseif xPosFromOrigin >= zPosFromOrigin then
			v3SkewedThirdFromOrigin, v3SkewedFourthFromOrigin = 1, 0, 0, 1, 0, 1
		else
			v3SkewedThirdFromOrigin, v3SkewedFourthFromOrigin = 1, 0, 1, 0, 0, 1
		end
		
	else
		
		if yPosFromOrigin < zPosFromOrigin then
			v3SkewedThirdFromOrigin, v3SkewedFourthFromOrigin = 0, 0, 1, 0, 1, 1
		elseif xPosFromOrigin < zPosFromOrigin then
			v3SkewedThirdFromOrigin, v3SkewedFourthFromOrigin = 0, 1, 1, 0, 1, 0
		else
			v3SkewedThirdFromOrigin, v3SkewedFourthFromOrigin = 0, 1, 0, 1, 1, 0
		end
		
	end
	
	local nUnskewThirdSubtrahend = (xSkewedThirdFromOrigin + ySkewedThirdFromOrigin + zSkewedThirdFromOrigin) * 0.16666666666666666666666666666667
	local v3PosFromThird = v3PosFromOrigin - v3SkewedThirdFromOrigin + nUnskewThirdSubtrahend
	
	local nUnskewFourthSubtrahend = (xSkewedFourthFromOrigin + ySkewedFourthFromOrigin + zSkewedFourthFromOrigin) * 0.16666666666666666666666666666667
	local v3PosFromFourth = v3PosFromOrigin - v3SkewedFourthFromOrigin + nUnskewFourthSubtrahend
	
	--[[
	Get each vertex's gradient and add its contribution to point at v3Pos
	]]
	
	local nSum = 0.0
	
	-- 1
	local v3Grad = com_RandomVec3(v3SkewedCellOrigin, nSeed)
	v3Grad = 1.0 - 2.0 * v3Grad
	local nFac = 0.6 - vec_Dot3(v3PosFromOrigin, v3PosFromOrigin)
	
	if nFac > 0.0 then
		
		nFac = nFac * nFac
		nSum = nFac * nFac * vec_Dot3(v3Grad, v3PosFromOrigin)
		
	end
	
	-- 2
	v3Grad = com_RandomVec3(v3SkewedCellOrigin + 1.0, nSeed)
	v3Grad = 1.0 - 2.0 * v3Grad
	nFac = 0.6 - vec_Dot3(v3PosFromFar, v3PosFromFar)
	
	if nFac > 0.0 then
		
		nFac = nFac * nFac
		nSum = nSum + nFac * nFac * vec_Dot3(v3Grad, v3PosFromFar)
		
	end
	
	-- 3
	v3Grad = com_RandomVec3(v3SkewedCellOrigin + v3SkewedThirdFromOrigin, nSeed)
	v3Grad = 1.0 - 2.0 * v3Grad
	nFac = 0.6 - vec_Dot3(v3PosFromThird, v3PosFromThird)
	
	if nFac > 0.0 then
		
		nFac = nFac * nFac
		nSum = nSum + nFac * nFac * vec_Dot3(v3Grad, v3PosFromThird)
		
	end
	
	-- 4
	v3Grad = com_RandomVec3(v3SkewedCellOrigin + v3SkewedFourthFromOrigin, nSeed)
	v3Grad = 1.0 - 2.0 * v3Grad
	nFac = 0.6 - vec_Dot3(v3PosFromFourth, v3PosFromFourth)
	
	if nFac > 0.0 then
		
		nFac = nFac * nFac
		nSum = nSum + nFac * nFac * vec_Dot3(v3Grad, v3PosFromFourth)
		
	end
	
	return nSum * 32.0 -- Scale to approximate range [-1, 1]
	
end

return com