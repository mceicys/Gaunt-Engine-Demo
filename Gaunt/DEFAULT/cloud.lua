local rnd = grnd
local scn = gscn

local cld = {}

cld.tex = rnd.EnsureTexture("temp_grey_surface.tex")

--[[------------------------------------
	cld:Init
--------------------------------------]]
function cld:Init()
	
	self:SetTexture(cld.tex)
	self:SetShadow(false)
	self:SetCloud(true)
	
end

cld.typ = scn.CreateEntityType("cloud", cld.Init, nil, nil, nil, nil)

return cld