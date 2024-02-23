-- carrier.lua
-- demo hack
LFV_EXPAND_VECTORS()

local qua = gqua
local rnd = grnd
local scn = gscn
local scr = gscr
local vec = gvec
local wrp = gwrp

local com = scr.EnsureScript("common.lua")

local car = {}
car.__index = car

car.msh = rnd.EnsureMesh("carrier.msh")
car.tex = rnd.EnsureTexture("black.tex")
car.texLight = rnd.EnsureTexture("carrier_light.tex")
car.mshFlash = rnd.EnsureMesh("flash.msh")
car.texFlash = rnd.EnsureTexture("flash.tex")

car.aniOpen = car.msh:Animation("open")

--[[------------------------------------
	car:Init
--------------------------------------]]
function car:Init()
	
	setmetatable(self:Table(), car)
	self:SetMesh(self.msh)
	self:SetTexture(self.tex)
	
	local entMimic = scn.CreateEntity(nil, self:Place())
	self.entMimic = entMimic
	self:SetChild(entMimic)
	entMimic:SetAmbientGlass(true)
	entMimic:SetShadow(false)
	entMimic:SetTexture(self.texLight)
	entMimic:MimicMesh(self)
	
	self:SetAnimationSpeed(1/10)
	--self:SetAnimation(self.aniOpen, 0, 0)
	
	local v3Pos = self:Pos()
	
	--[[
	self.v3Spark = 30, 0, -93.0
	local blbSpark = scn.CreateBulb(v3Pos + self.v3Spark, 100.0, 2.0, 0.0, 3)
	blbSpark:SetInner(0)
	blbSpark:SetOuter(math.pi * 0.4)
	blbSpark:SetOri(qua.QuaEuler(0, 0, math.pi))
	self.blbSpark = blbSpark
	--]]
	local blbSpark = scn.CreateBulb(-1923.95, -3500.00, 1411.28, 16.0, 5.4, 0.0, 3)
	self.blbSpark = blbSpark
	
	--[[
	self.v3Spark2 = 30, -20, -93.0
	local blbSpark2 = scn.CreateBulb(v3Pos + self.v3Spark2, 100.0, 2.0, 0.0, 3)
	blbSpark2:SetInner(0)
	blbSpark2:SetOuter(math.pi * 0.4)
	blbSpark2:SetOri(qua.QuaEuler(0, 0, math.pi * 0.8))
	self.blbSpark2 = blbSpark2
	--]]
	local blbSpark2 = scn.CreateBulb(-1917.95, -3514.43, 1411.28, 24.0, 5.4, 0.0, 3)
	--local blbSpark2 = scn.CreateBulb(-1917.95, -3514.43, 1405, 16.0, 5.4, 0.0, 3)
	self.blbSpark2 = blbSpark2
	
	--self.v3Amb = -20, 0, -93
	--self.v3Amb = 20, 20, -93
	--local blbAmb = scn.CreateBulb(v3Pos + self.v3Amb, 100, 2, 0, 3)
	local blbAmb = scn.CreateBulb(-1900, -3514.43, 1405, 100, 2, 0, 3)
	--blbAmb:SetInner(0)
	--blbAmb:SetOuter(0.4)
	blbAmb:SetOri(qua.QuaEuler(0, 0, 0))
	self.blbAmb = blbAmb
	
	local blbBack = scn.CreateBulb(-1920, -3548, 1465, 736, 2, 0, 3)
	blbBack:SetInner(0)
	blbBack:SetOuter(math.pi * 0.15)
	blbBack:SetOri(qua.QuaEuler(0, 0.5, math.pi * -0.5))
	self.blbBack = blbBack
	
	--[[
	self.v3Flash = 0, -60, -96
	local entFlash = scn.CreateEntity(nil, v3Pos + self.v3Flash, qua.QuaEuler(0, 0, math.pi * 0.5))
	self.entFlash = entFlash
	entFlash:SetRegularGlass(true)
	entFlash:SetMesh(self.mshFlash)
	entFlash:SetTexture(self.texFlash)
	entFlash:SetSubPalette(3)
	entFlash:SetScale(40)
	]]
	
	--[[
	local q4Cam1 = qua.QuaEuler(0, 0, math.pi * -0.5)
	local cam1 = scn.Camera(xPos, yPos + 60, zPos - 94, q4Cam1, 0.2)
	--]]
	--[[
	local q4Cam1 = qua.QuaEuler(0, -0.03, math.pi * 0.5)
	local cam1 = scn.Camera(xPos, yPos - 60, zPos - 94, q4Cam1, 0.2)
	--]]
	--[[
	local q4Cam1 = qua.QuaEuler(0, 0.0, math.pi)
	local cam1 = scn.Camera(xPos + 100, yPos, zPos - 93, q4Cam1, 0.15)
	--]]
	--[[
	self.v3Cam1 = -20, 9, -93.75
	local cam1 = scn.Camera(v3Pos + self.v3Cam1, 0, 0, 0, 1, 0.2)
	--]]
	self.v3Cam1 = -0.25, 1.5, -93.15
	local q4Cam1 = qua.QuaEuler(0, 0, math.pi * -0.5)
	local cam1 = scn.Camera(v3Pos + self.v3Cam1, q4Cam1, 0.23)
	self.cam1 = cam1
	
	--[[
	local q4Cam2 = qua.QuaEuler(0, 0.8656, -0.75817)
	local cam2 = scn.Camera(-1933.0, -3496, 1425.89, q4Cam2, 0.7)
	--]]
	--[[
	local q4Cam2 = qua.QuaEuler(0, 0.22898719786165606, 2.0036381442101678)
	local v3Fwd = qua.Dir(q4Cam2)
	local v3Cam2 = -1891.1082763671875, -3570.48388671875, 1421.8
	local cam2 = scn.Camera(v3Cam2 + v3Fwd * 0, q4Cam2, 0.15)
	--]]
	self.v3Cam2 = -5, 5, 20
	self.q4Cam2 = qua.QuaEuler(0, math.pi * 0.5, 0)
	local cam2 = scn.Camera(0, 0, 0, 0, 0, 0, 1, 1.0)
	self.cam2 = cam2
	
	self.tSpots = {}
	for i = 1, 10 do
		
		--local blbSpot = scn.CreateBulb(-1922.68, -3892 + (i - 5) * 5.0, 1406.92, 16, 2, 6.8, 0)
		--blbSpot:SetOri(qua.QuaEuler(0, math.rad(0.62), math.rad(-71.42)))
		--blbSpot:SetInner(0)
		--blbSpot:SetOuter(math.rad(10))
		local blbSpot = scn.CreateBulb(-1921, -3892 + (i - 5) * 5.0, 1407, 8, 2, 2, 0)
		blbSpot:SetOri(qua.QuaEuler(0, math.rad(0.0), math.rad(0)))
		blbSpot:SetInner(0)
		blbSpot:SetOuter(math.rad(190))
		self.tSpots[i] = blbSpot
		
	end
	--[[
	self.nSpotTravel = 0
	self.nSpotTravelLimit = 5
	self.v3Spot = -1922.68, -3892.71 + self.nSpotTravelLimit, 1406.92
	local blbSpot = scn.CreateBulb(self.v3Spot, 16, 2, 6.8, 0)
	blbSpot:SetOri(qua.QuaEuler(0, math.rad(0.62), math.rad(-71.42)))
	blbSpot:SetInner(0)
	blbSpot:SetOuter(math.rad(10))
	self.blbSpot = blbSpot
	]]
	
	self.v3Illum = -7.73, 17.69, -82.38
	local blbIllum = scn.CreateBulb(v3Pos + self.v3Illum, 64, 4.6, 0, 0)
	self.blbIllum = blbIllum
	
	local blbIllum3 = scn.CreateBulb(-1922.11, -3500.70, 1402.97, 16, 2, 0, 0)
	self.blbIllum3 = blbIllum3
	
	local blbIllum2 = scn.CreateBulb(-1886.55, -3380.88, 1410.24, 1000, 2.8, 0, 0)
	self.blbIllum2 = blbIllum2
	blbIllum2:SetInner(0)
	blbIllum2:SetOuter(math.rad(35.0))
	blbIllum2:SetOri(qua.QuaEuler(0, math.rad(-15.05), math.rad(-50.44)))
	
	self.v3Vel = 0, 0, 0
	self.bCarryingPlayer = true
	
end

--[[------------------------------------
	car:Tick
--------------------------------------]]
function car:Tick()
	
	local nStep = wrp.TimeStep()
	local v3Pos = self:Pos()
	v3Pos = v3Pos + self.v3Vel * nStep
	self:SetPos(v3Pos)
	self.entMimic:MimicMesh(self)
	local q4Ori = self:Ori()
	
	if gkey.Pressed(ACT_DETACH) then
		
		self.bCarryingPlayer = false
		entPlayer.v3Vel = self.v3Vel * 0.5
		
	end
	
	if self.bCarryingPlayer then
		
		entPlayer:SetPos(v2Pos, zPos - 93.0)
		entPlayer:SetOri(q4Ori)
		entPlayer.v3Vel = 0, 0, 0
		entPlayer:Tick()
		
	end
	
	local v3Player = entPlayer:Pos()
	local q4Player = entPlayer:Ori()
	self.cam2:SetPos(v3Player + self.v3Cam2)
	self.cam2:SetOri(qua.Product(q4Player, self.q4Cam2))
	
	local tSpots = self.tSpots
	for i = 1, #tSpots do
		
		local blbSpot = tSpots[i]
		local v3Spot = blbSpot:Pos()
		ySpot = ySpot - 10.0 * nStep
		if ySpot < -3892 - 25 then
			ySpot = ySpot + 50
		end
		blbSpot:SetPos(xSpot, ySpot, zSpot)
		
	end
	
	--[[
	self.nSpotTravel = self.nSpotTravel + 15.0 * nStep
	while self.nSpotTravel > self.nSpotTravelLimit * 2 do
		self.nSpotTravel = self.nSpotTravel - self.nSpotTravelLimit * 2
	end
	self.blbSpot:SetPos(self.xSpot, self.ySpot - self.nSpotTravel, self.zSpot)
	--]]
	
	--[[
	self.blbIllum:SetPos(v3Pos + self.v3Illum)
	self.blbIllum:SetIntensity(illum * 5)
	]]
	--self.blbIllum2:SetIntensity(illum * 6)
	--self.blbIllum3:SetIntensity(illum * 5)
	
	--self.cam1:SetPos(xPos + self.xCam1, yPos + self.yCam1 - illum * 15, zPos + self.zCam1)
	
	--self.blbSpark:SetPos(v3Pos + self.v3Spark)
	--self.blbSpark:SetIntensity(spark * 60)
	--self.blbLight:SetIntensity(spark * 10)
	--self.blbLight:SetPos(v2Pos + self.v2Spark, zPos + self.zSpark + 2)
	
	--self.blbSpark2:SetPos(v3Pos + self.v3Spark2)
	--self.blbSpark2:SetIntensity(spark2 * 7)
	--self.blbLight2:SetIntensity(spark2 * 50)
	--self.blbLight2:SetPos(v2Pos + self.v2Spark2, zPos + self.zSpark2 + 2)
	
	--self.blbAmb:SetIntensity((spark + spark2) * 0.4)
	
	--self.blbBack:SetIntensity((spark + spark2) * 0)
	
	--[[
	self.entFlash:SetPos(v3Pos + self.v3Flash)
	self.entFlash:SetOpacity((spark + spark2) * 0.2)
	]]
	
end

car.typ = scn.CreateEntityType("carrier", car.Init, car.Tick, nil, nil, nil)

return car