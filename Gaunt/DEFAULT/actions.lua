-- actions.lua

local key_EnsureAction = gkey.EnsureAction

ACT_THROTTLE = key_EnsureAction("THROTTLE")
ACT_BOOST = key_EnsureAction("BOOST") -- FIXME: remove
ACT_BRAKE = key_EnsureAction("BRAKE") -- FIXME: remove?
ACT_ROLL_CCW = key_EnsureAction("ROLL_CCW")
ACT_ROLL_CW = key_EnsureAction("ROLL_CW")
ACT_PITCH_UP = key_EnsureAction("PITCH_UP") -- FIXME TEMP: these actions should be bindable to mouse movements
ACT_PITCH_DOWN = key_EnsureAction("PITCH_DOWN")
ACT_YAW_LEFT = key_EnsureAction("YAW_LEFT")
ACT_YAW_RIGHT = key_EnsureAction("YAW_RIGHT")
ACT_LOOK = key_EnsureAction("LOOK")
ACT_LOOK_BACK = key_EnsureAction("LOOK_BACK")
ACT_LOOK_DRIFT = key_EnsureAction("LOOK_DRIFT")
ACT_FIRE_ONE = key_EnsureAction("FIRE_ONE")
ACT_NO_GUN = key_EnsureAction("NO_GUN")
ACT_MINIGUN = key_EnsureAction("MINIGUN")
ACT_ROCKET = key_EnsureAction("ROCKET")
ACT_QUICKLOAD = key_EnsureAction("QUICKLOAD")

ACT_FREE_CAM_FORWARD = key_EnsureAction("FREE_CAM_FORWARD")
ACT_FREE_CAM_BACK = key_EnsureAction("FREE_CAM_BACK")
ACT_FREE_CAM_LEFT = key_EnsureAction("FREE_CAM_LEFT")
ACT_FREE_CAM_RIGHT = key_EnsureAction("FREE_CAM_RIGHT")
ACT_FREE_CAM_UP = key_EnsureAction("FREE_CAM_UP")
ACT_FREE_CAM_DOWN = key_EnsureAction("FREE_CAM_DOWN")

ACT_NEW_CAM = key_EnsureAction("NEW_CAM") -- FIXME TEMP

-- Editor actions
ACT_EDITOR_CONFIRM = key_EnsureAction("EDITOR_CONFIRM")
ACT_EDITOR_CANCEL = key_EnsureAction("EDITOR_CANCEL")
ACT_EDITOR_INCREASE = key_EnsureAction("EDITOR_INCREASE")
ACT_EDITOR_DECREASE = key_EnsureAction("EDITOR_DECREASE")
ACT_EDITOR_CREATE_BULB = key_EnsureAction("EDITOR_CREATE_BULB")
ACT_EDITOR_DELETE = key_EnsureAction("EDITOR_DELETE")
ACT_EDITOR_MOVE = key_EnsureAction("EDITOR_MOVE")
ACT_EDITOR_ROTATE = key_EnsureAction("EDITOR_ROTATE")
ACT_EDITOR_X_AXIS = key_EnsureAction("EDITOR_X_AXIS")
ACT_EDITOR_Y_AXIS = key_EnsureAction("EDITOR_Y_AXIS")
ACT_EDITOR_Z_AXIS = key_EnsureAction("EDITOR_Z_AXIS")
ACT_EDITOR_INTENSITY = key_EnsureAction("EDITOR_INTENSITY")
ACT_EDITOR_SUB_PALETTE = key_EnsureAction("EDITOR_SUB_PALETTE")
ACT_EDITOR_COLOR_SMOOTH = key_EnsureAction("EDITOR_COLOR_SMOOTH")
ACT_EDITOR_COLOR_PRIORITY = key_EnsureAction("EDITOR_COLOR_PRIORITY")
ACT_EDITOR_RADIUS = key_EnsureAction("EDITOR_RADIUS")
ACT_EDITOR_EXPONENT = key_EnsureAction("EDITOR_EXPONENT")
ACT_EDITOR_OUTER_ANGLE = key_EnsureAction("EDITOR_OUTER_ANGLE")
ACT_EDITOR_INNER_ANGLE = key_EnsureAction("EDITOR_INNER_ANGLE")