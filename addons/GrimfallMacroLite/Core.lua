local addonName = ...

local Addon = LibStub("AceAddon-3.0"):NewAddon(addonName, "AceConsole-3.0")

local CreateFrame = CreateFrame
local GetCVar = GetCVar
local SetCVar = SetCVar
local floor = math.floor
local tonumber = tonumber
local tostring = tostring

local defaults = {
    profile = {
        addonBridge = false,
        addonBridgeTrace = false,
        addonBridgeMaxBytes = 255,
    }
}

local cvars = {
    addonBridge = "grimfallAddonBridge",
    addonBridgeTrace = "grimfallAddonBridgeTrace",
    addonBridgeMaxBytes = "grimfallAddonBridgeMaxBytes",
}

local categories = {
    { key = "general", label = "General" },
    { key = "bridge", label = "Addon Bridge" },
    { key = "macros", label = "Macros" },
    { key = "profiles", label = "Profiles" },
    { key = "about", label = "About" },
}

local function ensureDatabase()
    if not Addon.db then
        Addon.db = LibStub("AceDB-3.0"):New("GrimfallMacroLiteDB", defaults, true)
    end
    return Addon.db
end

local function boolToCVar(value)
    return value and "1" or "0"
end

local function cvarAvailable(name)
    return GetCVar(name) ~= nil
end

local function setBackendCVar(name, value)
    if cvarAvailable(name) then
        SetCVar(name, tostring(value))
        return true
    end
    return false
end

local function applyBackendSettings()
    ensureDatabase()
    local profile = Addon.db.profile
    local applied = 0

    if setBackendCVar(cvars.addonBridge, boolToCVar(profile.addonBridge)) then
        applied = applied + 1
    end
    if setBackendCVar(cvars.addonBridgeTrace, boolToCVar(profile.addonBridgeTrace)) then
        applied = applied + 1
    end
    if setBackendCVar(cvars.addonBridgeMaxBytes, profile.addonBridgeMaxBytes) then
        applied = applied + 1
    end

    return applied
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
    selected = { 0.95, 0.88, 0.55 },
}

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
    valueText:SetPoint("TOPRIGHT", -30, y)
    valueText:SetTextColor(1, 1, 1)

    local slider = CreateFrame("Slider", "GrimfallMacroLite" .. name, parent, "OptionsSliderTemplate")
    slider:SetPoint("TOPLEFT", x, y - 22)
    slider:SetWidth(260)
    slider:SetMinMaxValues(minValue, maxValue)
    slider:SetValueStep(step)
    _G[slider:GetName() .. "Low"]:SetText(tostring(minValue))
    _G[slider:GetName() .. "High"]:SetText(tostring(maxValue))
    _G[slider:GetName() .. "Text"]:SetText("")
    slider:SetScript("OnValueChanged", function(self, value)
        local rounded = step >= 1 and floor(value + 0.5) or value
        setter(rounded)
        valueText:SetText(tostring(rounded))
    end)
    slider.Refresh = function(self)
        local value = getter()
        self:SetValue(value)
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

local function backendStatusText()
    if cvarAvailable(cvars.addonBridge) then
        return "Backend CVars detected"
    end
    return "Backend CVars pending"
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
            button.bg:SetVertexColor(0.95, 0.72, 0.16, 0.28)
            button.text:SetTextColor(colors.selected[1], colors.selected[2], colors.selected[3])
        else
            button.bg:SetVertexColor(0.08, 0.08, 0.08, 0.75)
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
    button.bg:SetVertexColor(0.08, 0.08, 0.08, 0.75)

    button.icon = button:CreateTexture(nil, "ARTWORK")
    button.icon:SetWidth(16)
    button.icon:SetHeight(16)
    button.icon:SetPoint("LEFT", 8, 0)
    button.icon:SetTexture("Interface\\Buttons\\UI-OptionsButton")

    button.text = button:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    button.text:SetPoint("LEFT", button.icon, "RIGHT", 7, 0)
    button.text:SetText(label)

    button:SetScript("OnClick", function()
        showPage(key)
    end)
    return button
end

function Addon:CreateWindow()
    if self.ui then
        return
    end

    local frame = CreateFrame("Frame", "GrimfallMacroLiteFrame", UIParent)
    frame:SetWidth(520)
    frame:SetHeight(366)
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
    titleBar:SetWidth(320)
    titleBar:SetHeight(64)
    titleBar:SetPoint("TOP", 0, 12)

    local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    title:SetPoint("TOP", 0, -11)
    title:SetText("Grimfall MacroLite")
    setFontColor(title, colors.text)

    local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
    close:SetPoint("TOPRIGHT", -6, -6)

    local sidebar = CreateFrame("Frame", nil, frame)
    sidebar:SetPoint("TOPLEFT", 14, -42)
    sidebar:SetPoint("BOTTOMLEFT", 14, 50)
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
    content:SetPoint("TOPLEFT", 156, -43)
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
    local bridgeDescription = createDescription(
        ui.pages.bridge,
        "These settings are saved now. When the backend bridge registers its CVars, this panel will apply them automatically.",
        bridgeTitle,
        0,
        -14,
        320)
    bridgeDescription:SetHeight(42)

    local bridgeCheck = createCheck(ui.pages.bridge, "BridgeEnabled", "Enable Addon Bridge", 18, -96,
        function() return Addon.draft.addonBridge end,
        function(value) Addon.draft.addonBridge = value end)
    table.insert(ui.controls, bridgeCheck)

    local traceCheck = createCheck(ui.pages.bridge, "BridgeTrace", "Trace Bridge Messages", 18, -128,
        function() return Addon.draft.addonBridgeTrace end,
        function(value) Addon.draft.addonBridgeTrace = value end)
    table.insert(ui.controls, traceCheck)

    local maxBytesSlider = createSlider(ui.pages.bridge, "BridgeMaxBytes", "Max Message Bytes", 18, -172, 1, 255, 1,
        function() return Addon.draft.addonBridgeMaxBytes end,
        function(value) Addon.draft.addonBridgeMaxBytes = value end)
    table.insert(ui.controls, maxBytesSlider)

    local macrosTitle = createSectionTitle(ui.pages.macros, "Macro Features", "TOPLEFT", 18, -18)
    createDescription(ui.pages.macros, "Enabled in the current backend: [@cursor], [@playerlocation], and stance/form macro chain support.", macrosTitle, 0, -14, 320)

    local profilesTitle = createSectionTitle(ui.pages.profiles, "Profiles", "TOPLEFT", 18, -18)
    createDescription(ui.pages.profiles, "Settings are stored in GrimfallMacroLiteDB. Full profile management can be added after the backend bridge lands.", profilesTitle, 0, -14, 320)

    local aboutTitle = createSectionTitle(ui.pages.about, "About", "TOPLEFT", 18, -18)
    createDescription(ui.pages.about, "Grimfall MacroLite is a narrow Grimfall-specific rewrite focused on stable macro conditionals, the stance/form fix, and safe opt-in bridge controls.", aboutTitle, 0, -14, 320)

    local reset = createButton(frame, "Reset to Defaults", 118, 24)
    reset:SetPoint("BOTTOMLEFT", 28, 19)
    reset:SetScript("OnClick", function()
        resetDraft()
        refreshControls()
    end)

    local okay = createButton(frame, "Okay", 86, 24)
    okay:SetPoint("BOTTOMRIGHT", -116, 19)
    okay:SetScript("OnClick", function()
        local applied = saveDraft()
        Addon:Print("Settings saved. Applied " .. applied .. " backend CVar(s).")
        frame:Hide()
    end)

    local cancel = createButton(frame, "Cancel", 86, 24)
    cancel:SetPoint("BOTTOMRIGHT", -24, 19)
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
    ensureDatabase()
    copyProfileToDraft()

    self:RegisterChatCommand("gml", "SlashCommand")
    self:RegisterChatCommand("grimfallmacrolite", "SlashCommand")
end

function Addon:OnEnable()
    applyBackendSettings()
end

function Addon:SlashCommand(input)
    input = input and input:lower() or ""

    if input == "apply" then
        local applied = applyBackendSettings()
        self:Print("Applied " .. applied .. " backend CVar(s).")
        return
    end

    if input == "status" then
        self:Print(backendStatusText())
        return
    end

    self:OpenWindow()
end
