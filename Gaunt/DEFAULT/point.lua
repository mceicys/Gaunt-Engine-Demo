-- point.lua
LFV_EXPAND_VECTORS()

local hit = ghit -- FIXME TEMP
local scn = gscn
local vec = gvec

local EntPos = scn.EntPos
local hit_ALL = hit.ALL -- FIXME TEMP
local hit_LineTest = hit.LineTest -- FIXME TEMP
local hit_NONE = hit.NONE
local hit_TREE = hit.TREE
local math_huge = math.huge
local vec_MagSq3 = vec.MagSq3

local pt = {}
pt.__index = pt

local FLEE = 1 -- Can start fleeing here
local tPoints = {}
local tFleePoints = {}

pt.FLEE = FLEE
pt.tPoints = tPoints
pt.tFleePoints = tFleePoints

--[[------------------------------------
	pt:Init
--------------------------------------]]
function pt:Init()
	
	setmetatable(self:Table(), pt)
	self:SetPointLink(true)
	self.iPointFlags = 0
	self.iNumNexts, self.iNumPrevs = 0, 0
	self.v3NextMean, self.v3PrevMean = 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
	tPoints[#tPoints + 1] = self
	
end

--[[------------------------------------
	pt:Level

FIXME: one-ways, accessible from other points as either a next or a prev

FIXME: some points may want to forward-connect to a reversed track
	revTarget? Would the original point be a reverse-next, then?
		Are there reverse-prevs?
--------------------------------------]]
function pt:Level()
	
	local t = self:Transcript()
	
	if not t then
		return
	end
	
	self.iPointFlags = self.iPointFlags | (t["pointFlags"] or 0)
	
	if self.iPointFlags & FLEE ~= 0 then
		tFleePoints[#tFleePoints + 1] = self
	end
	
	local sTarget = t["target"]
	
	if sTarget then
		
		if string.find(sTarget, " ") then
			
			-- Can have multiple space-separated targets
			for sName in string.gmatch(sTarget, "%g+") do
				self:AddNext(scn.EntityFromRecordID(sName))
			end
			
		else
			self:AddNext(scn.EntityFromRecordID(sTarget))
		end
		
	end
	
end

--[[------------------------------------
	pt:AddNext

Next entities are stored in self's sequence: [1, iNumNexts]
Prev entities are stored after that: [iNumNexts + 1, iNumNexts + iNumPrevs]
--------------------------------------]]
function pt:AddNext(entNext)
	
	if not entNext then
		return
	end
	
	-- Shift prevs right
	for i = self.iNumNexts + self.iNumPrevs, self.iNumNexts + 1, -1 do
		self[i + 1] = self[i]
	end
	
	-- Add next
	self.iNumNexts = self.iNumNexts + 1
	self[self.iNumNexts] = entNext
	
	-- Make two-way link
	entNext.iNumPrevs = entNext.iNumPrevs + 1
	entNext[entNext.iNumNexts + entNext.iNumPrevs] = self
	
	-- Update mean direction of nexts
	-- Note that the final mean is not normalized, so the direction is weighted
	local v3Pos = EntPos(self)
	local v3Next = EntPos(entNext)
	local v3NextDir = vec.Normalized3(v3Next - v3Pos)
	local nNextFrac = 1 / self.iNumNexts
	self.v3NextMean = self.v3NextMean + nNextFrac * (v3NextDir - self.v3NextMean)
	
	-- Update next's mean direction of prevs
	local nPrevFrac = 1 / entNext.iNumPrevs
	entNext.v3PrevMean = entNext.v3PrevMean + nPrevFrac * (-v3NextDir - entNext.v3PrevMean)
	
end

--[[------------------------------------
	pt.ClearPointList
--------------------------------------]]
function pt.ClearPointList()
	
	for i = 1, #tPoints do
		tPoints[i] = nil
	end
	
	for i = 1, #tFleePoints do
		tFleePoints[i] = nil
	end
	
end

--[[------------------------------------
	pt.ClosestFleePoint

OUT	[entPoint]
--------------------------------------]]
function pt.ClosestFleePoint(v3Pos)
	
	local entBest
	local nBestSq = math_huge
	
	for i = 1, #tFleePoints do
		
		local entFlee = tFleePoints[i]
		local v3Flee = EntPos(entFlee)
		local nDistSq = vec_MagSq3(v3Flee - v3Pos)
		
		if nDistSq < nBestSq then
			entBest, nBestSq = entFlee, nDistSq
		end
		
	end
	
	return entBest
	
end

pt.typ = scn.CreateEntityType("point", pt.Init, nil, nil, nil, pt.Level)
return pt