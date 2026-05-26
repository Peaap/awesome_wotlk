local addonName = ...

local Addon = LibStub("AceAddon-3.0"):NewAddon(addonName, "AceConsole-3.0")
local AceConfig = LibStub("AceConfig-3.0")
local AceConfigDialog = LibStub("AceConfigDialog-3.0")

local GetCVar = GetCVar
local SetCVar = SetCVar
local InterfaceOptionsFrame_OpenToCategory = InterfaceOptionsFrame_OpenToCategory
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

local function getBackendStatus()
    if cvarAvailable(cvars.addonBridge) then
        return "Backend CVars detected."
    end
    return "Backend CVars not detected yet. Settings are saved and will apply once the MacroLite backend registers them."
end

local options = {
    type = "group",
    name = "Grimfall MacroLite",
    args = {
        status = {
            type = "description",
            name = function()
                return getBackendStatus()
            end,
            order = 1,
            fontSize = "medium",
        },
        bridgeHeader = {
            type = "header",
            name = "Addon Bridge",
            order = 10,
        },
        addonBridge = {
            type = "toggle",
            name = "Enable addon bridge",
            desc = "Saved now. Once the backend bridge exists, this maps to grimfallAddonBridge.",
            order = 20,
            get = function()
                return Addon.db.profile.addonBridge
            end,
            set = function(_, value)
                Addon.db.profile.addonBridge = value and true or false
                setBackendCVar(cvars.addonBridge, boolToCVar(Addon.db.profile.addonBridge))
            end,
        },
        addonBridgeTrace = {
            type = "toggle",
            name = "Trace addon bridge",
            desc = "Enables verbose bridge logging when the backend bridge exists.",
            order = 30,
            get = function()
                return Addon.db.profile.addonBridgeTrace
            end,
            set = function(_, value)
                Addon.db.profile.addonBridgeTrace = value and true or false
                setBackendCVar(cvars.addonBridgeTrace, boolToCVar(Addon.db.profile.addonBridgeTrace))
            end,
        },
        addonBridgeMaxBytes = {
            type = "range",
            name = "Max message bytes",
            desc = "Upper bound passed to grimfallAddonBridgeMaxBytes once the backend bridge exists.",
            order = 40,
            min = 1,
            max = 255,
            step = 1,
            get = function()
                return Addon.db.profile.addonBridgeMaxBytes
            end,
            set = function(_, value)
                local bytes = tonumber(value) or 255
                if bytes < 1 then
                    bytes = 1
                elseif bytes > 255 then
                    bytes = 255
                end
                Addon.db.profile.addonBridgeMaxBytes = bytes
                setBackendCVar(cvars.addonBridgeMaxBytes, bytes)
            end,
        },
        apply = {
            type = "execute",
            name = "Apply to backend",
            desc = "Attempts to write saved settings to MacroLite CVars.",
            order = 50,
            func = function()
                local applied = applyBackendSettings()
                Addon:Print("Applied " .. applied .. " backend CVar(s).")
            end,
        },
    },
}

function Addon:OnInitialize()
    self.db = LibStub("AceDB-3.0"):New("GrimfallMacroLiteDB", defaults, true)
    AceConfig:RegisterOptionsTable("GrimfallMacroLite", options)
    self.optionsFrame = AceConfigDialog:AddToBlizOptions("GrimfallMacroLite", "Grimfall MacroLite")

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
        self:Print(getBackendStatus())
        return
    end

    InterfaceOptionsFrame_OpenToCategory(self.optionsFrame)
    InterfaceOptionsFrame_OpenToCategory(self.optionsFrame)
end
