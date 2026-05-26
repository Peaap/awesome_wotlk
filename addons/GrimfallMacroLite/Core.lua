local addonName = ...

local Addon = {}

local CreateFrame = CreateFrame
local atan2 = math.atan2
local cos = math.cos
local floor = math.floor
local sin = math.sin
local tonumber = tonumber
local tostring = tostring
local tableRemove = table.remove

local nameplateCVarDefs = {
    { key = "grimfallNameplateApi", label = "Enable Nameplate API", kind = "check", default = 1 },
    { key = "grimfallNameplateStackingPatchApply", label = "Enable Stacking Patch", kind = "check", default = 0 },
    { key = "grimfallNameplateDebug", label = "Debug Nameplate Logs", kind = "check", default = 0 },
    { key = "grimfallNameplateDistance", label = "Nameplate Distance", min = 41, max = 100, step = 1, default = 41 },
    { key = "grimfallNameplatePlacement", label = "Placement", min = -1, max = 2, step = 0.01, default = 0 },
    { key = "grimfallNameplateStacking", label = "Stacking Mode", min = 0, max = 3, step = 1, default = 0 },
    { key = "grimfallNameplateMouseMode", label = "Mouse Mode", min = 0, max = 8, step = 1, default = 0 },
    { key = "grimfallNameplateBandX", label = "Band X", min = 0.1, max = 1, step = 0.01, default = 0.2 },
    { key = "grimfallNameplateBandY", label = "Band Y", min = 0.1, max = 1.5, step = 0.01, default = 0.2 },
    { key = "grimfallNameplateHitboxAnchor", label = "Hitbox Anchor", min = 0, max = 2, step = 1, default = 0 },
    { key = "grimfallNameplateHitboxWidthE", label = "Enemy Hitbox Width", min = 0, max = 1, step = 0.01, default = 0.1 },
    { key = "grimfallNameplateHitboxHeightE", label = "Enemy Hitbox Height", min = 0, max = 1, step = 0.01, default = 0.1 },
    { key = "grimfallNameplateHitboxWidthF", label = "Friendly Hitbox Width", min = 0, max = 1, step = 0.01, default = 0.1 },
    { key = "grimfallNameplateHitboxHeightF", label = "Friendly Hitbox Height", min = 0, max = 1, step = 0.01, default = 0.1 },
    { key = "grimfallNameplateRaiseSpeed", label = "Raise Speed", min = 1, max = 250, step = 1, default = 100 },
    { key = "grimfallNameplateLowerSpeed", label = "Lower Speed", min = 1, max = 250, step = 1, default = 100 },
    { key = "grimfallNameplatePullSpeed", label = "Pull Speed", min = 1, max = 250, step = 1, default = 100 },
    { key = "grimfallNameplateRaiseDistance", label = "Raise Distance", min = 1, max = 20, step = 0.1, default = 2 },
    { key = "grimfallNameplatePullDistance", label = "Pull Distance", min = 0, max = 2, step = 0.01, default = 0.25 },
    { key = "grimfallNameplateClampTop", label = "Clamp Top", min = 0, max = 2, step = 1, default = 0 },
    { key = "grimfallNameplateClampTopOffset", label = "Clamp Top Offset", min = 0, max = 0.25, step = 0.01, default = 0.1 },
    { key = "grimfallNameplateOcclusionAlpha", label = "Occlusion Alpha", min = 0, max = 1, step = 0.01, default = 0.5 },
    { key = "grimfallNameplateNonTargetAlpha", label = "Non-Target Alpha", min = 0, max = 1, step = 0.01, default = 1 },
    { key = "grimfallNameplateAlphaSpeed", label = "Alpha Speed", min = 0.01, max = 1, step = 0.01, default = 0.1 },
    { key = "grimfallNameplateInertia", label = "Inertia", min = 0, max = 20, step = 0.1, default = 1 },
    { key = "grimfallNameplateHysteresisDecay", label = "Hysteresis Decay", min = 0.25, max = 30, step = 0.25, default = 1 },
}

local cameraCVarDefs = {
    { key = "cameraFov", label = "Field of View", min = 60, max = 150, step = 1, default = 100 },
    { key = "cameraDistanceMax", label = "Max Camera Distance", min = 0, max = 50, step = 1, default = 15 },
    { key = "showPlayer", label = "Show Player", kind = "check", default = 1 },
    { key = "cameraIndirectVisibility", label = "Indirect Visibility", kind = "check", default = 1 },
    { key = "cameraIndirectAlpha", label = "Indirect Alpha", min = 0.6, max = 1, step = 0.05, default = 0.6 },
}

local cameraPresets = {
    Default = {
        cameraFov = 100,
        cameraDistanceMax = 15,
        showPlayer = 1,
        cameraIndirectVisibility = 1,
        cameraIndirectAlpha = 0.6,
    },
    Wide = {
        cameraFov = 115,
        cameraDistanceMax = 35,
        showPlayer = 1,
        cameraIndirectVisibility = 1,
        cameraIndirectAlpha = 0.75,
    },
    Raid = {
        cameraFov = 110,
        cameraDistanceMax = 50,
        showPlayer = 1,
        cameraIndirectVisibility = 1,
        cameraIndirectAlpha = 0.8,
    },
    Cinematic = {
        cameraFov = 85,
        cameraDistanceMax = 25,
        showPlayer = 1,
        cameraIndirectVisibility = 0,
        cameraIndirectAlpha = 0.6,
    },
}

local Protocol = {
    Version = "0.1.0",
    Prefix = "GML",
    Commands = {
        Ping = "GML",
        Status = "GML/status",
        Config = "GML/config",
        MacroTest = "GML/macro-test",
        ObjectPlayer = "GML/object/player",
        ObjectTarget = "GML/object/target",
        ObjectMouseover = "GML/object/mouseover",
        ObjectExists = "GML/object/exists",
    },
    Payloads = {
        Ping = "ping",
        Empty = "",
    },
    Status = {
        Ok = "ok",
        Rejected = "rejected",
        UnknownCommand = "unknown_command",
        Disabled = "disabled",
        Pending = "pending",
    },
    Modules = {
        Core = "core",
        NamePlates = "nameplates",
        MacroConditionals = "macro_conditionals",
        Spell = "spell",
        AddonBridge = "addon_bridge",
    },
    Modes = {
        LuaData = "lua-data",
    },
    CVars = {
        Enabled = "grimfallAddonBridge",
        Trace = "grimfallAddonBridgeTrace",
        MaxBytes = "grimfallAddonBridgeMaxBytes",
        Seq = "grimfallAddonBridgeSeq",
        Command = "grimfallAddonBridgeCommand",
        Payload = "grimfallAddonBridgePayload",
        AckSeq = "grimfallAddonBridgeAckSeq",
        AckStatus = "grimfallAddonBridgeAckStatus",
        AckPayload = "grimfallAddonBridgeAckPayload",
    },
}

local defaults = {
    profile = {
        addonBridge = false,
        addonBridgeTrace = false,
        addonBridgeMaxBytes = 255,
        minimapAngle = 225,
    }
}

local function bridgeSetCVar(name, value)
    if type(SetCVar) ~= "function" then
        return nil, "SetCVar unavailable"
    end

    local ok, reason = pcall(SetCVar, name, tostring(value))
    if ok then
        return 1
    end
    return nil, reason
end

local function bridgeGetCVar(name)
    if type(GetCVar) ~= "function" then
        return nil
    end

    local ok, value = pcall(GetCVar, name)
    if ok then
        return value
    end
    return nil
end

local function ensureLuaBridge()
    _G.GrimfallMacroLiteConfig = _G.GrimfallMacroLiteConfig or {}
    _G.GrimfallMacroLiteOutbox = _G.GrimfallMacroLiteOutbox or {}
    _G.GrimfallMacroLiteInbox = _G.GrimfallMacroLiteInbox or {}

    local config = _G.GrimfallMacroLiteConfig
    if config.enabled == nil then
        config.enabled = false
    end
    if config.trace == nil then
        config.trace = false
    end
    if config.maxBytes == nil then
        config.maxBytes = 255
    end

    local bridge = _G.GrimfallMacroLite
    if type(bridge) ~= "table" then
        bridge = {}
        _G.GrimfallMacroLite = bridge
    end

    bridge.version = "0.1.0-lua"
    bridge.nativeSeq = tonumber(bridgeGetCVar(Protocol.CVars.Seq)) or bridge.nativeSeq or 0
    bridge.Protocol = Protocol
    bridge.Commands = Protocol.Commands
    bridge.Status = Protocol.Status
    bridge.Modules = Protocol.Modules
    bridge.Modes = Protocol.Modes
    bridge.GetVersion = function()
        return bridge.version
    end
    bridge.IsEnabled = function()
        return _G.GrimfallMacroLiteConfig.enabled and true or false
    end
    bridge.GetStatus = function()
        return {
            version = bridge.version,
            enabled = _G.GrimfallMacroLiteConfig.enabled and true or false,
            trace = _G.GrimfallMacroLiteConfig.trace and true or false,
            maxBytes = tonumber(_G.GrimfallMacroLiteConfig.maxBytes) or 255,
            mode = "lua-data",
        }
    end
    bridge.SetConfig = function(value)
        if type(value) ~= "table" then
            return nil, "usage: GrimfallMacroLite.SetConfig(table)"
        end

        _G.GrimfallMacroLiteConfig.enabled = value.enabled and true or false
        _G.GrimfallMacroLiteConfig.trace = value.trace and true or false
        _G.GrimfallMacroLiteConfig.maxBytes = tonumber(value.maxBytes) or 255
        bridgeSetCVar(Protocol.CVars.Enabled, _G.GrimfallMacroLiteConfig.enabled and 1 or 0)
        bridgeSetCVar(Protocol.CVars.Trace, _G.GrimfallMacroLiteConfig.trace and 1 or 0)
        bridgeSetCVar(Protocol.CVars.MaxBytes, _G.GrimfallMacroLiteConfig.maxBytes)
        return 1
    end
    bridge.Send = function(prefix, payload)
        if not _G.GrimfallMacroLiteConfig.enabled then
            return nil, Protocol.Status.Disabled
        end
        if type(prefix) ~= "string" or type(payload) ~= "string" then
            return nil, "usage: GrimfallMacroLite.Send(prefix, payload)"
        end
        if string.len(payload) > (tonumber(_G.GrimfallMacroLiteConfig.maxBytes) or 255) then
            return nil, "payload rejected"
        end

        table.insert(_G.GrimfallMacroLiteOutbox, {
            prefix = prefix,
            payload = payload,
            time = time(),
        })
        bridge.nativeSeq = (tonumber(bridge.nativeSeq) or 0) + 1
        bridgeSetCVar(Protocol.CVars.Command, prefix)
        bridgeSetCVar(Protocol.CVars.Payload, payload)
        bridgeSetCVar(Protocol.CVars.Seq, bridge.nativeSeq)
        return 1
    end
    bridge.SendCommand = function(command, payload)
        return bridge.Send(command, payload or Protocol.Payloads.Empty)
    end
    bridge.RequestStatus = function()
        return bridge.SendCommand(Protocol.Commands.Status)
    end
    bridge.PushConfig = function()
        return bridge.SendCommand(Protocol.Commands.Config, "apply")
    end
    bridge.RequestMacroTest = function()
        return bridge.SendCommand(Protocol.Commands.MacroTest)
    end
    bridge.RequestPlayerGUID = function()
        return bridge.SendCommand(Protocol.Commands.ObjectPlayer)
    end
    bridge.RequestTargetGUID = function()
        return bridge.SendCommand(Protocol.Commands.ObjectTarget)
    end
    bridge.RequestMouseoverGUID = function()
        return bridge.SendCommand(Protocol.Commands.ObjectMouseover)
    end
    bridge.RequestObjectExists = function(guid)
        return bridge.SendCommand(Protocol.Commands.ObjectExists, tostring(guid or ""))
    end
end

local function consumeInbox()
    local ackSeq = tonumber(bridgeGetCVar(Protocol.CVars.AckSeq)) or 0
    if ackSeq > (Addon.lastBridgeAckSeq or 0) then
        Addon.lastBridgeAckSeq = ackSeq
        Addon.lastBackendAck = {
            prefix = tostring(bridgeGetCVar(Protocol.CVars.Command) or ""),
            payload = tostring(bridgeGetCVar(Protocol.CVars.AckPayload) or ""),
            status = tostring(bridgeGetCVar(Protocol.CVars.AckStatus) or Protocol.Status.Ok),
        }
        Addon:Print("Backend ack: " .. Addon.lastBackendAck.status .. " " .. Addon.lastBackendAck.prefix .. "/" .. Addon.lastBackendAck.payload)
    end

    local inbox = _G.GrimfallMacroLiteInbox
    if type(inbox) ~= "table" then
        return
    end

    local entry = tableRemove(inbox, 1)
    while entry do
        if type(entry) == "table" then
            Addon.lastBackendAck = {
                prefix = tostring(entry.prefix or ""),
                payload = tostring(entry.payload or ""),
                status = tostring(entry.status or Protocol.Status.Ok),
            }
            Addon:Print("Backend ack: " .. Addon.lastBackendAck.status .. " " .. Addon.lastBackendAck.prefix .. "/" .. Addon.lastBackendAck.payload)
        end
        entry = tableRemove(inbox, 1)
    end
end

local categories = {
    { key = "general", label = "General" },
    { key = "bridge", label = "Addon Bridge" },
    { key = "camera", label = "Camera" },
    { key = "nameplates", label = "Nameplates" },
    { key = "macros", label = "Macros" },
    { key = "profiles", label = "Profiles" },
    { key = "credits", label = "Credits" },
    { key = "about", label = "About" },
}

local function ensureDatabase()
    if not GrimfallMacroLiteDB then
        GrimfallMacroLiteDB = {}
    end
    if not GrimfallMacroLiteDB.profile then
        GrimfallMacroLiteDB.profile = {}
    end

    local profile = GrimfallMacroLiteDB.profile
    if profile.addonBridge == nil then
        profile.addonBridge = defaults.profile.addonBridge
    end
    if profile.addonBridgeTrace == nil then
        profile.addonBridgeTrace = defaults.profile.addonBridgeTrace
    end
    if profile.addonBridgeMaxBytes == nil then
        profile.addonBridgeMaxBytes = defaults.profile.addonBridgeMaxBytes
    end
    if profile.minimapAngle == nil then
        profile.minimapAngle = defaults.profile.minimapAngle
    end

    Addon.db = GrimfallMacroLiteDB
    return Addon.db
end

local function applyBackendSettings()
    ensureDatabase()
    local profile = Addon.db.profile

    local bridge = _G.GrimfallMacroLite
    if type(bridge) == "table" and type(bridge.SetConfig) == "function" then
        local ok, reason = bridge.SetConfig({
            enabled = profile.addonBridge and true or false,
            trace = profile.addonBridgeTrace and true or false,
            maxBytes = tonumber(profile.addonBridgeMaxBytes) or 255,
        })
        if ok then
            return 1, nil
        end
        return 0, reason
    end

    return 0, "backend bridge Lua table pending"
end

local function copyProfileToDraft()
    ensureDatabase()
    Addon.draft = {
        addonBridge = Addon.db.profile.addonBridge and true or false,
        addonBridgeTrace = Addon.db.profile.addonBridgeTrace and true or false,
        addonBridgeMaxBytes = tonumber(Addon.db.profile.addonBridgeMaxBytes) or 255,
    }
end

local function saveDraft()
    local draft = Addon.draft
    Addon.db.profile.addonBridge = draft.addonBridge and true or false
    Addon.db.profile.addonBridgeTrace = draft.addonBridgeTrace and true or false
    Addon.db.profile.addonBridgeMaxBytes = tonumber(draft.addonBridgeMaxBytes) or 255
    return applyBackendSettings()
end

local function resetDraft()
    Addon.draft.addonBridge = defaults.profile.addonBridge
    Addon.draft.addonBridgeTrace = defaults.profile.addonBridgeTrace
    Addon.draft.addonBridgeMaxBytes = defaults.profile.addonBridgeMaxBytes
end

local function safeGetCVar(name)
    if type(GetCVar) ~= "function" then
        return nil
    end

    local ok, value = pcall(GetCVar, name)
    if ok then
        return value
    end
    return nil
end

local function safeSetCVar(name, value)
    if type(SetCVar) ~= "function" then
        return nil, "SetCVar unavailable"
    end

    local ok, reason = pcall(SetCVar, name, tostring(value))
    if ok then
        return 1, nil
    end
    return nil, reason
end

local function getNameplateCVarNumber(def)
    local value = tonumber(safeGetCVar(def.key))
    if value == nil then
        return def.default
    end
    return value
end

local function getNameplateCVarBool(def)
    local value = safeGetCVar(def.key)
    if value == nil then
        return def.default ~= 0
    end
    return value == "1" or value == 1 or value == true
end

local function setNameplateCVar(def, value)
    if def.kind == "check" then
        value = value and 1 or 0
    end

    local ok, reason = safeSetCVar(def.key, value)
    if not ok then
        Addon:Print("Nameplate CVar unavailable: " .. def.key .. " (" .. tostring(reason) .. ")")
    end
end

local function resetNameplateCVars()
    for _, def in ipairs(nameplateCVarDefs) do
        setNameplateCVar(def, def.default)
    end
end

local function getCameraCVarNumber(def)
    local value = tonumber(safeGetCVar(def.key))
    if value == nil then
        return def.default
    end
    return value
end

local function getCameraCVarBool(def)
    local value = safeGetCVar(def.key)
    if value == nil then
        return def.default ~= 0
    end
    return value == "1" or value == 1 or value == true
end

local function setCameraCVar(def, value)
    if def.kind == "check" then
        value = value and 1 or 0
    end

    local ok, reason = safeSetCVar(def.key, value)
    if not ok then
        Addon:Print("Camera CVar unavailable: " .. def.key .. " (" .. tostring(reason) .. ")")
    end
end

local function resetCameraCVars()
    for _, def in ipairs(cameraCVarDefs) do
        setCameraCVar(def, def.default)
    end
end

local function applyCameraPreset(name)
    local preset = cameraPresets[name]
    if not preset then
        return
    end

    for _, def in ipairs(cameraCVarDefs) do
        if preset[def.key] ~= nil then
            setCameraCVar(def, preset[def.key])
        end
    end
end

local function cameraStatusText()
    local fov = safeGetCVar("cameraFov")
    local showPlayer = safeGetCVar("showPlayer")
    local indirect = safeGetCVar("cameraIndirectVisibility")
    if fov == nil or showPlayer == nil or indirect == nil then
        return "Backend camera CVars: unavailable"
    end
    return "Backend camera CVars: active  FOV " .. tostring(fov) .. "  Player " .. tostring(showPlayer) .. "  Indirect " .. tostring(indirect)
end

local function addPanelBackdrop(frame)
    frame:SetBackdrop({
        bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
        edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
        tile = true,
        tileSize = 32,
        edgeSize = 32,
        insets = { left = 8, right = 8, top = 8, bottom = 8 },
    })
    frame:SetBackdropColor(0.03, 0.03, 0.03, 0.98)
end

local function setFontColor(fontString, color)
    fontString:SetTextColor(color[1], color[2], color[3])
end

local colors = {
    gold = { 1.0, 0.82, 0.0 },
    text = { 0.86, 0.82, 0.72 },
    muted = { 0.55, 0.52, 0.46 },
    dim = { 0.30, 0.28, 0.23 },
    selected = { 0.95, 0.88, 0.55 },
    red = { 0.82, 0.18, 0.09 },
}

local backendStatusText

local function createSectionTitle(parent, text, anchor, x, y)
    local title = parent:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    title:SetPoint(anchor, x, y)
    title:SetText(text)
    setFontColor(title, colors.gold)
    return title
end

local function createDescription(parent, text, anchorTo, x, y, width)
    local fs = parent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    fs:SetPoint("TOPLEFT", anchorTo, "BOTTOMLEFT", x, y)
    fs:SetWidth(width)
    fs:SetJustifyH("LEFT")
    fs:SetText(text)
    setFontColor(fs, colors.text)
    return fs
end

local function createRule(parent, anchorTo, y)
    local line = parent:CreateTexture(nil, "ARTWORK")
    line:SetHeight(1)
    line:SetPoint("TOPLEFT", anchorTo, "BOTTOMLEFT", 0, y)
    line:SetPoint("RIGHT", parent, "RIGHT", -18, 0)
    line:SetTexture("Interface\\Buttons\\WHITE8X8")
    line:SetVertexColor(0.82, 0.65, 0.20, 0.25)
    return line
end

local function createCheck(parent, name, label, x, y, getter, setter)
    local check = CreateFrame("CheckButton", "GrimfallMacroLite" .. name, parent, "UICheckButtonTemplate")
    check:SetPoint("TOPLEFT", x, y)
    check:SetWidth(24)
    check:SetHeight(24)
    check.text = _G[check:GetName() .. "Text"]
    check.text:SetText(label)
    check.text:SetTextColor(0.9, 0.82, 0.55)
    check:SetScript("OnClick", function(self)
        setter(self:GetChecked() and true or false)
    end)
    check.Refresh = function(self)
        self:SetChecked(getter() and true or false)
    end
    return check
end

local function createSlider(parent, name, label, x, y, minValue, maxValue, step, getter, setter)
    local title = parent:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    title:SetPoint("TOPLEFT", x, y)
    title:SetText(label)
    setFontColor(title, colors.text)

    local valueText = parent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    valueText:SetPoint("TOPRIGHT", parent, "TOPRIGHT", -28, y)
    valueText:SetTextColor(colors.selected[1], colors.selected[2], colors.selected[3])

    local slider = CreateFrame("Slider", "GrimfallMacroLite" .. name, parent, "OptionsSliderTemplate")
    slider:SetPoint("TOPLEFT", x, y - 28)
    slider:SetWidth(420)
    slider:SetMinMaxValues(minValue, maxValue)
    slider:SetValueStep(step)
    local low = _G[slider:GetName() .. "Low"]
    local high = _G[slider:GetName() .. "High"]
    low:ClearAllPoints()
    low:SetPoint("TOPLEFT", slider, "BOTTOMLEFT", 0, 3)
    low:SetText(tostring(minValue))
    low:SetTextColor(colors.muted[1], colors.muted[2], colors.muted[3])
    high:ClearAllPoints()
    high:SetPoint("TOPRIGHT", slider, "BOTTOMRIGHT", 0, 3)
    high:SetText(tostring(maxValue))
    high:SetTextColor(colors.muted[1], colors.muted[2], colors.muted[3])
    _G[slider:GetName() .. "Text"]:SetText("")
    slider.titleText = title
    slider.valueText = valueText
    slider:SetScript("OnValueChanged", function(self, value)
        local rounded = step >= 1 and floor(value + 0.5) or value
        if not self.isRefreshing then
            setter(rounded)
        end
        valueText:SetText(tostring(rounded))
    end)
    slider:SetScript("OnShow", function(self)
        self.titleText:Show()
        self.valueText:Show()
    end)
    slider:SetScript("OnHide", function(self)
        self.titleText:Hide()
        self.valueText:Hide()
    end)
    slider.Refresh = function(self)
        local value = getter()
        self.isRefreshing = true
        self:SetValue(value)
        self.isRefreshing = false
        valueText:SetText(tostring(value))
    end
    return slider
end

local function createButton(parent, text, width, height)
    local button = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    button:SetWidth(width)
    button:SetHeight(height)
    button:SetText(text)
    return button
end

local function createEditBox(parent, width, height)
    local editBox = CreateFrame("EditBox", nil, parent)
    editBox:SetWidth(width)
    editBox:SetHeight(height)
    editBox:SetAutoFocus(false)
    editBox:SetFontObject(ChatFontNormal)
    editBox:SetTextInsets(8, 8, 0, 0)
    editBox:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true,
        tileSize = 16,
        edgeSize = 10,
        insets = { left = 2, right = 2, top = 2, bottom = 2 },
    })
    editBox:SetBackdropColor(0.01, 0.01, 0.01, 0.85)
    editBox:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)
    editBox:SetScript("OnEnterPressed", function(self) self:ClearFocus() end)
    return editBox
end

local function skinActionButton(button)
    button:SetNormalTexture("Interface\\Buttons\\UI-Panel-Button-Up")
    button:GetNormalTexture():SetVertexColor(0.75, 0.16, 0.08)
    button:SetHighlightTexture("Interface\\Buttons\\UI-Panel-Button-Highlight")
end

local function updateMinimapButtonPosition()
    local button = Addon.minimapButton
    if not button then
        return
    end

    ensureDatabase()
    local angle = Addon.db.profile.minimapAngle or defaults.profile.minimapAngle
    local radians = angle * 0.017453292519943
    local radius = 80
    button:SetPoint("CENTER", Minimap, "CENTER", cos(radians) * radius, sin(radians) * radius)
end

local function moveMinimapButton()
    local centerX, centerY = Minimap:GetCenter()
    local cursorX, cursorY = GetCursorPosition()
    local scale = Minimap:GetEffectiveScale()

    cursorX = cursorX / scale
    cursorY = cursorY / scale

    local angle = atan2(cursorY - centerY, cursorX - centerX)
    Addon.db.profile.minimapAngle = angle * 57.295779513082
    updateMinimapButtonPosition()
end

function Addon:CreateMinimapButton()
    if self.minimapButton then
        updateMinimapButtonPosition()
        return
    end

    ensureDatabase()

    local button = CreateFrame("Button", "GrimfallMacroLiteMinimapButton", Minimap)
    button:SetWidth(32)
    button:SetHeight(32)
    button:SetFrameStrata("MEDIUM")
    button:SetMovable(true)
    button:EnableMouse(true)
    button:RegisterForClicks("LeftButtonUp", "RightButtonUp")
    button:RegisterForDrag("LeftButton")

    local overlay = button:CreateTexture(nil, "OVERLAY")
    overlay:SetWidth(53)
    overlay:SetHeight(53)
    overlay:SetPoint("TOPLEFT")
    overlay:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")

    local icon = button:CreateTexture(nil, "BACKGROUND")
    icon:SetWidth(20)
    icon:SetHeight(20)
    icon:SetPoint("CENTER", -1, 1)
    icon:SetTexture("Interface\\Icons\\Spell_Arcane_Arcane01")

    local highlight = button:CreateTexture(nil, "HIGHLIGHT")
    highlight:SetAllPoints(icon)
    highlight:SetTexture("Interface\\Minimap\\UI-Minimap-ZoomButton-Highlight")
    highlight:SetBlendMode("ADD")

    button:SetScript("OnClick", function(_, clickedButton)
        if clickedButton == "RightButton" then
            Addon:Print(backendStatusText())
            return
        end
        Addon:OpenWindow()
    end)
    button:SetScript("OnDragStart", function(self)
        self:SetScript("OnUpdate", moveMinimapButton)
    end)
    button:SetScript("OnDragStop", function(self)
        self:SetScript("OnUpdate", nil)
        moveMinimapButton()
    end)
    button:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_LEFT")
        GameTooltip:AddLine("Grimfall MacroLite", 1, 0.82, 0)
        GameTooltip:AddLine("Left-click to open options.", 0.86, 0.82, 0.72)
        GameTooltip:AddLine("Right-click for backend status.", 0.55, 0.52, 0.46)
        GameTooltip:Show()
    end)
    button:SetScript("OnLeave", function()
        GameTooltip:Hide()
    end)

    self.minimapButton = button
    updateMinimapButtonPosition()
end

backendStatusText = function()
    local bridge = _G.GrimfallMacroLite
    if type(bridge) == "table" and type(bridge.GetStatus) == "function" then
        local status = bridge.GetStatus()
        if type(status) == "table" then
            local enabled = status.enabled and "enabled" or "disabled"
            local text = "Lua bridge ready: " .. enabled .. " / " .. tostring(status.maxBytes or "?") .. " bytes"
            if Addon.lastBackendAck then
                text = text .. " / " .. Addon.lastBackendAck.status .. ": " .. Addon.lastBackendAck.prefix
            end
            return text
        end
    end

    return "Lua bridge pending"
end

local function showPage(key)
    Addon.activePage = key
    local ui = Addon.ui

    for _, page in pairs(ui.pages) do
        page:Hide()
    end
    ui.pages[key]:Show()

    for _, button in pairs(ui.categoryButtons) do
        if button.key == key then
            button.bg:SetVertexColor(0.95, 0.72, 0.16, 0.18)
            button.leftEdge:SetVertexColor(1.0, 0.76, 0.18, 0.9)
            button.glow:Show()
            button.text:SetTextColor(colors.selected[1], colors.selected[2], colors.selected[3])
        else
            button.bg:SetVertexColor(0.02, 0.02, 0.02, 0.15)
            button.leftEdge:SetVertexColor(0.35, 0.32, 0.22, 0.2)
            button.glow:Hide()
            button.text:SetTextColor(colors.text[1], colors.text[2], colors.text[3])
        end
    end
end

local function refreshControls()
    local ui = Addon.ui
    if not ui then
        return
    end

    for _, control in ipairs(ui.controls) do
        control:Refresh()
    end
    if ui.RefreshNameplatePage then
        ui.RefreshNameplatePage()
    end
    if ui.cameraStatus then
        ui.cameraStatus:SetText(cameraStatusText())
    end
    ui.status:SetText(backendStatusText())
end

local function createCategoryButton(parent, key, label, index)
    local button = CreateFrame("Button", nil, parent)
    button.key = key
    button:SetWidth(118)
    button:SetHeight(28)
    button:SetPoint("TOPLEFT", 14, -54 - ((index - 1) * 31))

    button.bg = button:CreateTexture(nil, "BACKGROUND")
    button.bg:SetAllPoints()
    button.bg:SetTexture("Interface\\Tooltips\\UI-Tooltip-Background")
    button.bg:SetVertexColor(0.02, 0.02, 0.02, 0.15)

    button.glow = button:CreateTexture(nil, "BORDER")
    button.glow:SetPoint("TOPLEFT", 2, -2)
    button.glow:SetPoint("BOTTOMRIGHT", -2, 2)
    button.glow:SetTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight")
    button.glow:SetBlendMode("ADD")
    button.glow:SetVertexColor(0.85, 0.56, 0.12, 0.35)
    button.glow:Hide()

    button.leftEdge = button:CreateTexture(nil, "ARTWORK")
    button.leftEdge:SetWidth(2)
    button.leftEdge:SetPoint("TOPLEFT", 2, -4)
    button.leftEdge:SetPoint("BOTTOMLEFT", 2, 4)
    button.leftEdge:SetTexture("Interface\\Buttons\\WHITE8X8")
    button.leftEdge:SetVertexColor(0.35, 0.32, 0.22, 0.2)

    button.icon = button:CreateTexture(nil, "ARTWORK")
    button.icon:SetWidth(16)
    button.icon:SetHeight(16)
    button.icon:SetPoint("LEFT", 8, 0)
    button.icon:SetTexture("Interface\\Buttons\\UI-GuildButton-PublicNote-Up")
    button.icon:SetVertexColor(0.75, 0.68, 0.48, 0.9)

    button.text = button:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    button.text:SetPoint("LEFT", button.icon, "RIGHT", 7, 0)
    button.text:SetText(label)

    button:SetScript("OnClick", function()
        showPage(key)
    end)
    button:SetScript("OnEnter", function(self)
        if Addon.activePage ~= self.key then
            self.bg:SetVertexColor(0.35, 0.25, 0.08, 0.22)
        end
    end)
    button:SetScript("OnLeave", function(self)
        if Addon.activePage ~= self.key then
            self.bg:SetVertexColor(0.02, 0.02, 0.02, 0.15)
        end
    end)
    return button
end

function Addon:CreateWindow()
    if self.ui then
        return
    end

    local frame = CreateFrame("Frame", "GrimfallMacroLiteFrame", UIParent)
    frame:SetWidth(720)
    frame:SetHeight(520)
    frame:SetPoint("CENTER")
    frame:SetFrameStrata("DIALOG")
    frame:EnableMouse(true)
    frame:SetMovable(true)
    frame:RegisterForDrag("LeftButton")
    frame:SetScript("OnDragStart", frame.StartMoving)
    frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
    frame:Hide()
    addPanelBackdrop(frame)

    local titleBar = frame:CreateTexture(nil, "ARTWORK")
    titleBar:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Header")
    titleBar:SetWidth(360)
    titleBar:SetHeight(64)
    titleBar:SetPoint("TOP", 0, 12)

    local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    title:SetPoint("TOP", 0, -11)
    title:SetText("Grimfall MacroLite")
    setFontColor(title, colors.text)

    local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
    close:SetPoint("TOPRIGHT", -6, -6)

    local sidebar = CreateFrame("Frame", nil, frame)
    sidebar:SetPoint("TOPLEFT", 16, -44)
    sidebar:SetPoint("BOTTOMLEFT", 16, 52)
    sidebar:SetWidth(132)
    sidebar:SetBackdrop({
        bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true,
        tileSize = 16,
        edgeSize = 12,
        insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    sidebar:SetBackdropColor(0.015, 0.015, 0.015, 0.92)

    local content = CreateFrame("Frame", nil, frame)
    content:SetPoint("TOPLEFT", 160, -44)
    content:SetPoint("BOTTOMRIGHT", -18, 52)
    content:SetBackdrop({
        bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true,
        tileSize = 16,
        edgeSize = 12,
        insets = { left = 3, right = 3, top = 3, bottom = 3 },
    })
    content:SetBackdropColor(0.02, 0.02, 0.02, 0.94)

    local ui = {
        frame = frame,
        content = content,
        pages = {},
        controls = {},
        categoryButtons = {},
    }
    self.ui = ui

    for i, category in ipairs(categories) do
        ui.categoryButtons[category.key] = createCategoryButton(frame, category.key, category.label, i)
        local page = CreateFrame("Frame", nil, content)
        page:SetAllPoints()
        page:Hide()
        ui.pages[category.key] = page
    end

    local generalTitle = createSectionTitle(ui.pages.general, "General Settings", "TOPLEFT", 18, -18)
    createRule(ui.pages.general, generalTitle, -9)
    local statusLabel = createDescription(ui.pages.general, "", generalTitle, 0, -14, 310)
    ui.status = statusLabel
    local generalCopy = createDescription(
        ui.pages.general,
        "Macro conditionals and stance/form fixes are managed by the MacroLite backend. Addon bridge controls are prepared here before the backend bridge is enabled.",
        statusLabel,
        0,
        -22,
        320)
    generalCopy:SetHeight(56)

    local bridgeTitle = createSectionTitle(ui.pages.bridge, "Addon Bridge", "TOPLEFT", 18, -18)
    createRule(ui.pages.bridge, bridgeTitle, -9)
    local bridgeDescription = createDescription(
        ui.pages.bridge,
        "These settings are saved locally and applied through the MacroLite Lua bridge when the backend is available.",
        bridgeTitle,
        0,
        -14,
        320)
    bridgeDescription:SetHeight(42)

    local bridgeCheck = createCheck(ui.pages.bridge, "BridgeEnabled", "Enable Addon Bridge", 18, -102,
        function() return Addon.draft.addonBridge end,
        function(value) Addon.draft.addonBridge = value end)
    table.insert(ui.controls, bridgeCheck)

    local traceCheck = createCheck(ui.pages.bridge, "BridgeTrace", "Trace Bridge Messages", 18, -134,
        function() return Addon.draft.addonBridgeTrace end,
        function(value) Addon.draft.addonBridgeTrace = value end)
    table.insert(ui.controls, traceCheck)

    local maxBytesSlider = createSlider(ui.pages.bridge, "BridgeMaxBytes", "Max Message Bytes", 18, -184, 1, 255, 1,
        function() return Addon.draft.addonBridgeMaxBytes end,
        function(value) Addon.draft.addonBridgeMaxBytes = value end)
    table.insert(ui.controls, maxBytesSlider)

    local testBridge = createButton(ui.pages.bridge, "Test Bridge", 104, 24)
    testBridge:SetPoint("TOPLEFT", 18, -258)
    skinActionButton(testBridge)
    testBridge:SetScript("OnClick", function()
        local bridge = _G.GrimfallMacroLite
        if type(bridge) ~= "table" or type(bridge.Send) ~= "function" then
            Addon:Print("Backend bridge Lua table is not available.")
            return
        end

        local ok, reason = bridge.Send(Protocol.Commands.Ping, Protocol.Payloads.Ping)
        if ok then
            Addon:Print("Bridge test queued.")
        else
            Addon:Print("Bridge test failed: " .. tostring(reason))
        end
        refreshControls()
    end)

    local statusBridge = createButton(ui.pages.bridge, "Status", 76, 24)
    statusBridge:SetPoint("LEFT", testBridge, "RIGHT", 10, 0)
    skinActionButton(statusBridge)
    statusBridge:SetScript("OnClick", function()
        local bridge = _G.GrimfallMacroLite
        if type(bridge) ~= "table" or type(bridge.RequestStatus) ~= "function" then
            Addon:Print("Backend bridge Lua table is not available.")
            return
        end

        local ok, reason = bridge.RequestStatus()
        if ok then
            Addon:Print("Status request queued.")
        else
            Addon:Print("Status request failed: " .. tostring(reason))
        end
        refreshControls()
    end)

    local configBridge = createButton(ui.pages.bridge, "Config", 76, 24)
    configBridge:SetPoint("LEFT", statusBridge, "RIGHT", 10, 0)
    skinActionButton(configBridge)
    configBridge:SetScript("OnClick", function()
        local bridge = _G.GrimfallMacroLite
        if type(bridge) ~= "table" or type(bridge.PushConfig) ~= "function" then
            Addon:Print("Backend bridge Lua table is not available.")
            return
        end

        applyBackendSettings()
        local ok, reason = bridge.PushConfig()
        if ok then
            Addon:Print("Config request queued.")
        else
            Addon:Print("Config request failed: " .. tostring(reason))
        end
        refreshControls()
    end)

    local objectTitle = createSectionTitle(ui.pages.bridge, "Object / Target API", "TOPLEFT", 18, -314)
    createRule(ui.pages.bridge, objectTitle, -9)

    local playerGuidButton = createButton(ui.pages.bridge, "Player GUID", 104, 24)
    playerGuidButton:SetPoint("TOPLEFT", 18, -350)
    skinActionButton(playerGuidButton)
    playerGuidButton:SetScript("OnClick", function()
        local bridge = _G.GrimfallMacroLite
        local ok, reason = bridge and bridge.RequestPlayerGUID and bridge.RequestPlayerGUID()
        Addon:Print(ok and "Player GUID request queued." or ("Player GUID request failed: " .. tostring(reason)))
    end)

    local targetGuidButton = createButton(ui.pages.bridge, "Target GUID", 104, 24)
    targetGuidButton:SetPoint("LEFT", playerGuidButton, "RIGHT", 10, 0)
    skinActionButton(targetGuidButton)
    targetGuidButton:SetScript("OnClick", function()
        local bridge = _G.GrimfallMacroLite
        local ok, reason = bridge and bridge.RequestTargetGUID and bridge.RequestTargetGUID()
        Addon:Print(ok and "Target GUID request queued." or ("Target GUID request failed: " .. tostring(reason)))
    end)

    local mouseoverGuidButton = createButton(ui.pages.bridge, "Mouseover GUID", 128, 24)
    mouseoverGuidButton:SetPoint("LEFT", targetGuidButton, "RIGHT", 10, 0)
    skinActionButton(mouseoverGuidButton)
    mouseoverGuidButton:SetScript("OnClick", function()
        local bridge = _G.GrimfallMacroLite
        local ok, reason = bridge and bridge.RequestMouseoverGUID and bridge.RequestMouseoverGUID()
        Addon:Print(ok and "Mouseover GUID request queued." or ("Mouseover GUID request failed: " .. tostring(reason)))
    end)

    local existsLabel = ui.pages.bridge:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    existsLabel:SetPoint("TOPLEFT", 18, -390)
    existsLabel:SetText("Object Exists")
    setFontColor(existsLabel, colors.muted)

    local existsInput = createEditBox(ui.pages.bridge, 236, 24)
    existsInput:SetPoint("TOPLEFT", 18, -410)
    existsInput:SetMaxLetters(18)
    existsInput:SetText("")
    ui.objectExistsInput = existsInput

    local existsButton = createButton(ui.pages.bridge, "Exists", 76, 24)
    existsButton:SetPoint("LEFT", existsInput, "RIGHT", 10, 0)
    skinActionButton(existsButton)
    existsButton:SetScript("OnClick", function()
        local guid = existsInput:GetText()
        local bridge = _G.GrimfallMacroLite
        local ok, reason = bridge and bridge.RequestObjectExists and bridge.RequestObjectExists(guid)
        Addon:Print(ok and "Object exists request queued." or ("Object exists request failed: " .. tostring(reason)))
    end)

    local cameraTitle = createSectionTitle(ui.pages.camera, "Camera Controls", "TOPLEFT", 18, -18)
    createRule(ui.pages.camera, cameraTitle, -9)
    local cameraDescription = createDescription(
        ui.pages.camera,
        "Camera controls are backed by MacroLite CVars. Indirect visibility is experimental.",
        cameraTitle,
        0,
        -14,
        360)
    cameraDescription:SetHeight(36)

    local cameraStatus = createDescription(ui.pages.camera, "", cameraDescription, 0, -28, 360)
    cameraStatus:SetHeight(18)
    ui.cameraStatus = cameraStatus

    ui.cameraControls = {}
    for index, def in ipairs(cameraCVarDefs) do
        local cvarDef = def
        local y = -96 - ((index - 1) * 44)
        local control
        if cvarDef.kind == "check" then
            control = createCheck(ui.pages.camera, "Camera" .. index, cvarDef.label, 18, y,
                function() return getCameraCVarBool(cvarDef) end,
                function(value) setCameraCVar(cvarDef, value) end)
        else
            control = createSlider(ui.pages.camera, "Camera" .. index, cvarDef.label, 18, y, cvarDef.min, cvarDef.max, cvarDef.step,
                function() return getCameraCVarNumber(cvarDef) end,
                function(value) setCameraCVar(cvarDef, value) end)
        end
        table.insert(ui.cameraControls, control)
        table.insert(ui.controls, control)
    end

    local presetTitle = createSectionTitle(ui.pages.camera, "Presets", "TOPLEFT", 18, -332)
    createRule(ui.pages.camera, presetTitle, -9)

    local defaultCamera = createButton(ui.pages.camera, "Default", 78, 24)
    defaultCamera:SetPoint("TOPLEFT", 18, -366)
    skinActionButton(defaultCamera)
    defaultCamera:SetScript("OnClick", function()
        applyCameraPreset("Default")
        Addon:Print("Camera preset applied: Default")
        refreshControls()
    end)

    local wideCamera = createButton(ui.pages.camera, "Wide", 78, 24)
    wideCamera:SetPoint("LEFT", defaultCamera, "RIGHT", 10, 0)
    skinActionButton(wideCamera)
    wideCamera:SetScript("OnClick", function()
        applyCameraPreset("Wide")
        Addon:Print("Camera preset applied: Wide")
        refreshControls()
    end)

    local raidCamera = createButton(ui.pages.camera, "Raid", 78, 24)
    raidCamera:SetPoint("LEFT", wideCamera, "RIGHT", 10, 0)
    skinActionButton(raidCamera)
    raidCamera:SetScript("OnClick", function()
        applyCameraPreset("Raid")
        Addon:Print("Camera preset applied: Raid")
        refreshControls()
    end)

    local cinematicCamera = createButton(ui.pages.camera, "Cinematic", 92, 24)
    cinematicCamera:SetPoint("LEFT", raidCamera, "RIGHT", 10, 0)
    skinActionButton(cinematicCamera)
    cinematicCamera:SetScript("OnClick", function()
        applyCameraPreset("Cinematic")
        Addon:Print("Camera preset applied: Cinematic")
        refreshControls()
    end)

    local resetCamera = createButton(ui.pages.camera, "Reset Camera", 104, 24)
    resetCamera:SetPoint("TOPLEFT", 18, -406)
    skinActionButton(resetCamera)
    resetCamera:SetScript("OnClick", function()
        resetCameraCVars()
        Addon:Print("Camera CVars reset to defaults.")
        refreshControls()
    end)

    local nameplatesTitle = createSectionTitle(ui.pages.nameplates, "Nameplate API", "TOPLEFT", 18, -18)
    createRule(ui.pages.nameplates, nameplatesTitle, -9)
    local nameplatesDescription = createDescription(
        ui.pages.nameplates,
        "These controls read the backend CVars directly. If values show defaults only, load the MacroLite DLL and reload the UI.",
        nameplatesTitle,
        0,
        -14,
        320)
    nameplatesDescription:SetHeight(36)

    ui.nameplatePage = 1
    ui.nameplateControls = {}
    local nameplatePageSize = 5
    local nameplatePageCount = floor((#nameplateCVarDefs + nameplatePageSize - 1) / nameplatePageSize)

    for index, def in ipairs(nameplateCVarDefs) do
        local cvarDef = def
        local pageIndex = floor((index - 1) / nameplatePageSize) + 1
        local rowIndex = (index - 1) - ((pageIndex - 1) * nameplatePageSize)
        local y = -88 - (rowIndex * 52)
        local rowFrame = CreateFrame("Frame", nil, ui.pages.nameplates)
        rowFrame:SetPoint("TOPLEFT", 18, y)
        rowFrame:SetPoint("RIGHT", ui.pages.nameplates, "RIGHT", -18, 0)
        rowFrame:SetHeight(48)
        local control

        if cvarDef.kind == "check" then
            control = createCheck(rowFrame, "Nameplate" .. index, cvarDef.label, 0, -2,
                function() return getNameplateCVarBool(cvarDef) end,
                function(value) setNameplateCVar(cvarDef, value) end)
        else
            control = createSlider(rowFrame, "Nameplate" .. index, cvarDef.label, 0, 0, cvarDef.min, cvarDef.max, cvarDef.step,
                function() return getNameplateCVarNumber(cvarDef) end,
                function(value) setNameplateCVar(cvarDef, value) end)
        end

        control.nameplatePage = pageIndex
        control.rowFrame = rowFrame
        table.insert(ui.nameplateControls, control)
        table.insert(ui.controls, control)
    end

    local nameplatePageLabel = ui.pages.nameplates:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    nameplatePageLabel:SetPoint("BOTTOM", ui.pages.nameplates, "BOTTOM", 0, 15)
    setFontColor(nameplatePageLabel, colors.muted)
    ui.nameplatePageLabel = nameplatePageLabel

    local nameplatePrev = createButton(ui.pages.nameplates, "Prev", 58, 22)
    nameplatePrev:SetPoint("BOTTOMLEFT", 18, 9)
    skinActionButton(nameplatePrev)

    local nameplateNext = createButton(ui.pages.nameplates, "Next", 58, 22)
    nameplateNext:SetPoint("LEFT", nameplatePrev, "RIGHT", 8, 0)
    skinActionButton(nameplateNext)

    local nameplateReset = createButton(ui.pages.nameplates, "Reset CVars", 92, 22)
    nameplateReset:SetPoint("BOTTOMRIGHT", ui.pages.nameplates, "BOTTOMRIGHT", -18, 9)
    skinActionButton(nameplateReset)

    ui.RefreshNameplatePage = function()
        for _, control in ipairs(ui.nameplateControls) do
            if control.nameplatePage == ui.nameplatePage then
                control.rowFrame:Show()
            else
                control.rowFrame:Hide()
            end
        end
        nameplatePageLabel:SetText("Page " .. tostring(ui.nameplatePage) .. " / " .. tostring(nameplatePageCount))
    end

    nameplatePrev:SetScript("OnClick", function()
        ui.nameplatePage = ui.nameplatePage - 1
        if ui.nameplatePage < 1 then
            ui.nameplatePage = nameplatePageCount
        end
        refreshControls()
    end)

    nameplateNext:SetScript("OnClick", function()
        ui.nameplatePage = ui.nameplatePage + 1
        if ui.nameplatePage > nameplatePageCount then
            ui.nameplatePage = 1
        end
        refreshControls()
    end)

    nameplateReset:SetScript("OnClick", function()
        resetNameplateCVars()
        Addon:Print("Nameplate CVars reset to backend defaults.")
        refreshControls()
    end)

    local macrosTitle = createSectionTitle(ui.pages.macros, "Macro Features", "TOPLEFT", 18, -18)
    createRule(ui.pages.macros, macrosTitle, -9)
    createDescription(ui.pages.macros, "Enabled in the current backend: [@cursor], [@playerlocation], and stance/form macro chain support.", macrosTitle, 0, -14, 320)

    local macroTest = createButton(ui.pages.macros, "Query Backend", 118, 24)
    macroTest:SetPoint("TOPLEFT", 18, -86)
    skinActionButton(macroTest)
    macroTest:SetScript("OnClick", function()
        local bridge = _G.GrimfallMacroLite
        if type(bridge) ~= "table" or type(bridge.RequestMacroTest) ~= "function" then
            Addon:Print("Backend bridge Lua table is not available.")
            return
        end

        local ok, reason = bridge.RequestMacroTest()
        if ok then
            Addon:Print("Macro status request queued.")
        else
            Addon:Print("Macro status request failed: " .. tostring(reason))
        end
        refreshControls()
    end)

    local profilesTitle = createSectionTitle(ui.pages.profiles, "Profiles", "TOPLEFT", 18, -18)
    createRule(ui.pages.profiles, profilesTitle, -9)
    createDescription(ui.pages.profiles, "Settings are stored in GrimfallMacroLiteDB. Full profile management can be added after the backend bridge lands.", profilesTitle, 0, -14, 320)

    local creditsTitle = createSectionTitle(ui.pages.credits, "Credits", "TOPLEFT", 18, -18)
    createRule(ui.pages.credits, creditsTitle, -9)
    createDescription(ui.pages.credits, "Grimfall MacroLite rewrite and testing: Peaap / peaps.", creditsTitle, 0, -14, 320)

    local aboutTitle = createSectionTitle(ui.pages.about, "About", "TOPLEFT", 18, -18)
    createRule(ui.pages.about, aboutTitle, -9)
    createDescription(ui.pages.about, "Grimfall MacroLite is a narrow Grimfall-specific rewrite focused on stable macro conditionals, the stance/form fix, and safe opt-in bridge controls.", aboutTitle, 0, -14, 320)

    local reset = createButton(frame, "Reset to Defaults", 118, 24)
    reset:SetPoint("BOTTOMLEFT", 28, 19)
    skinActionButton(reset)
    reset:SetScript("OnClick", function()
        if Addon.activePage == "nameplates" then
            resetNameplateCVars()
            Addon:Print("Nameplate CVars reset to backend defaults.")
        elseif Addon.activePage == "camera" then
            resetCameraCVars()
            Addon:Print("Camera CVars reset to defaults.")
        else
            resetDraft()
        end
        refreshControls()
    end)

    local okay = createButton(frame, "Okay", 86, 24)
    okay:SetPoint("BOTTOMRIGHT", -116, 19)
    skinActionButton(okay)
    okay:SetScript("OnClick", function()
        local applied, reason = saveDraft()
        if applied > 0 then
            Addon:Print("Settings saved and applied to backend.")
        else
            Addon:Print("Settings saved locally: " .. tostring(reason))
        end
        frame:Hide()
    end)

    local cancel = createButton(frame, "Cancel", 86, 24)
    cancel:SetPoint("BOTTOMRIGHT", -24, 19)
    skinActionButton(cancel)
    cancel:SetScript("OnClick", function()
        copyProfileToDraft()
        refreshControls()
        frame:Hide()
    end)
end

function Addon:OpenWindow()
    self:CreateWindow()
    copyProfileToDraft()
    refreshControls()
    showPage(self.activePage or "general")
    self.ui.frame:Show()
end

function Addon:OnInitialize()
    ensureLuaBridge()
    ensureDatabase()
    copyProfileToDraft()
end

function Addon:OnEnable()
    applyBackendSettings()
    self:CreateMinimapButton()
    if not self.inboxFrame then
        local frame = CreateFrame("Frame")
        frame.elapsed = 0
        frame:SetScript("OnUpdate", function(self, elapsed)
            self.elapsed = self.elapsed + elapsed
            if self.elapsed < 0.5 then
                return
            end
            self.elapsed = 0
            consumeInbox()
            refreshControls()
        end)
        self.inboxFrame = frame
    end
end

function Addon:SlashCommand(input)
    input = input and input:lower() or ""

    if input == "apply" then
        local applied, reason = applyBackendSettings()
        if applied > 0 then
            self:Print("Applied settings to backend.")
        else
            self:Print("Apply pending: " .. tostring(reason))
        end
        return
    end

    if input == "status" then
        self:Print(backendStatusText())
        return
    end

    if input == "playerguid" or input == "player" then
        local bridge = _G.GrimfallMacroLite
        local ok, reason = bridge and bridge.RequestPlayerGUID and bridge.RequestPlayerGUID()
        self:Print(ok and "Player GUID request queued." or ("Player GUID request failed: " .. tostring(reason)))
        return
    end

    if input == "targetguid" or input == "target" then
        local bridge = _G.GrimfallMacroLite
        local ok, reason = bridge and bridge.RequestTargetGUID and bridge.RequestTargetGUID()
        self:Print(ok and "Target GUID request queued." or ("Target GUID request failed: " .. tostring(reason)))
        return
    end

    if input == "mouseoverguid" or input == "mouseover" then
        local bridge = _G.GrimfallMacroLite
        local ok, reason = bridge and bridge.RequestMouseoverGUID and bridge.RequestMouseoverGUID()
        self:Print(ok and "Mouseover GUID request queued." or ("Mouseover GUID request failed: " .. tostring(reason)))
        return
    end

    local existsGuid = input:match("^exists%s+(.+)$")
    if existsGuid then
        local bridge = _G.GrimfallMacroLite
        local ok, reason = bridge and bridge.RequestObjectExists and bridge.RequestObjectExists(existsGuid)
        self:Print(ok and "Object exists request queued." or ("Object exists request failed: " .. tostring(reason)))
        return
    end

    self:OpenWindow()
end

function Addon:Print(message)
    DEFAULT_CHAT_FRAME:AddMessage("|cffd9b84aGrimfall MacroLite:|r " .. tostring(message))
end

SLASH_GRIMFALLMACROLITE1 = "/gml"
SLASH_GRIMFALLMACROLITE2 = "/grimfallmacrolite"
SlashCmdList.GRIMFALLMACROLITE = function(input)
    Addon:SlashCommand(input)
end

local events = CreateFrame("Frame")
events:RegisterEvent("ADDON_LOADED")
events:RegisterEvent("PLAYER_LOGIN")
events:SetScript("OnEvent", function(_, event, name)
    if event == "ADDON_LOADED" and name == addonName then
        Addon:OnInitialize()
        return
    end

    if event == "PLAYER_LOGIN" then
        Addon:OnEnable()
    end
end)
