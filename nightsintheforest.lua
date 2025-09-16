--[[
SuperScript Mobile Full v3
Autor: Yuri / GitHub
Compatível: Delta / Mobile Roblox Executor
Funcionalidades: Tela de apresentação, Menu dragável, ESP Selecionável, AutoLoot, AutoFarm, AutoAbrir Baús, Teleporte Full, Kill Aura
--]]

repeat task.wait() until game:IsLoaded()

-- Serviços
local Players = game:GetService("Players")
local LocalPlayer = Players.LocalPlayer
local Workspace = game:GetService("Workspace")
local RunService = game:GetService("RunService")
local StarterGui = game:GetService("StarterGui")
local UserInputService = game:GetService("UserInputService")

-- =========================
-- CONFIGURAÇÕES INICIAIS
-- =========================
local Config = {
    ESP = {Enemies=false, Chests=false, Trees=false},
    AutoLoot=false,
    AutoFarm=false,
    AutoOpenChests=false,
    KillAura=false,
    FarmRadius=20,
    TreesSimultaneously=3,
    KillAuraRadius=10
}

-- =========================
-- TELA DE APRESENTAÇÃO
-- =========================
local SplashScreen = Instance.new("ScreenGui")
SplashScreen.Name = "SplashScreen"
SplashScreen.Parent = LocalPlayer:WaitForChild("PlayerGui")

local SplashFrame = Instance.new("Frame")
SplashFrame.Size = UDim2.new(0,400,0,200)
SplashFrame.Position = UDim2.new(0.5,-200,0.5,-100)
SplashFrame.BackgroundColor3 = Color3.fromRGB(20,20,20)
SplashFrame.BorderSizePixel = 0
SplashFrame.Parent = SplashScreen

local TitleLabel = Instance.new("TextLabel")
TitleLabel.Size = UDim2.new(1,0,1,0)
TitleLabel.BackgroundTransparency = 1
TitleLabel.TextColor3 = Color3.fromRGB(255,255,255)
TitleLabel.TextScaled = true
TitleLabel.Font = Enum.Font.GothamBold
TitleLabel.Text = "SuperScript"
TitleLabel.Parent = SplashFrame

task.delay(3, function()
    SplashScreen:Destroy()
end)

-- =========================
-- FUNÇÕES AUXILIARES
-- =========================

local function createESP(part, color)
    if not part or part:FindFirstChild("ESP") then return end
    local box = Instance.new("BoxHandleAdornment")
    box.Name = "ESP"
    box.Adornee = part
    box.Size = part.Size
    box.Color3 = color or Color3.new(1,0,0)
    box.AlwaysOnTop = true
    box.ZIndex = 10
    box.Parent = part
end

-- =========================
-- ESP SELECIONÁVEL
-- =========================
local function updateESP()
    if Config.ESP.Enemies then
        local enemiesFolder = Workspace:FindFirstChild("Enemies")
        if enemiesFolder then
            for _, enemy in pairs(enemiesFolder:GetChildren()) do
                if enemy:FindFirstChild("HumanoidRootPart") then
                    createESP(enemy.HumanoidRootPart, Color3.new(1,0,0))
                end
            end
        end
    end
    if Config.ESP.Chests then
        local chestsFolder = Workspace:FindFirstChild("Chests")
        if chestsFolder then
            for _, chest in pairs(chestsFolder:GetChildren()) do
                if chest:IsA("BasePart") then
                    createESP(chest, Color3.new(0,1,0))
                end
            end
        end
    end
    if Config.ESP.Trees then
        local treesFolder = Workspace:FindFirstChild("Trees")
        if treesFolder then
            for _, tree in pairs(treesFolder:GetChildren()) do
                if tree:IsA("BasePart") then
                    createESP(tree, Color3.fromRGB(153,76,0))
                end
            end
        end
    end
end

-- =========================
-- AUTOLOOT
-- =========================
local function autoLoot()
    if not Config.AutoLoot then return end
    local lootFolder = Workspace:FindFirstChild("Loot")
    if not lootFolder or not LocalPlayer.Character or not LocalPlayer.Character:FindFirstChild("HumanoidRootPart") then return end
    for _, item in pairs(lootFolder:GetChildren()) do
        if item:IsA("BasePart") then
            local distance = (item.Position - LocalPlayer.Character.HumanoidRootPart.Position).Magnitude
            if distance < 10 then
                pcall(function()
                    LocalPlayer.Character.HumanoidRootPart.CFrame = CFrame.new(item.Position)
                end)
            end
        end
    end
end

-- =========================
-- AUTOFARM
-- =========================
local function autoFarmTrees()
    if not Config.AutoFarm then return end
    local treesFolder = Workspace:FindFirstChild("Trees")
    if not treesFolder or not LocalPlayer.Character or not LocalPlayer.Character:FindFirstChild("HumanoidRootPart") then return end
    
    local treesCutted = 0
    for _, tree in pairs(treesFolder:GetChildren()) do
        if tree:IsA("BasePart") then
            local distance = (tree.Position - LocalPlayer.Character.HumanoidRootPart.Position).Magnitude
            if distance <= Config.FarmRadius then
                pcall(function()
                    LocalPlayer.Character.HumanoidRootPart.CFrame = CFrame.new(tree.Position + Vector3.new(0,3,0))
                    treesCutted = treesCutted + 1
                end)
            end
            if treesCutted >= Config.TreesSimultaneously then break end
        end
    end
end

-- =========================
-- TELEPORTE FULL
-- =========================
local TeleportFunctions = {}

function TeleportFunctions.ToChildren()
    local childrenFolder = Workspace:FindFirstChild("Children")
    if not childrenFolder then return end
    local nearestChild = nil
    local minDist = math.huge
    for _, child in pairs(childrenFolder:GetChildren()) do
        if child:IsA("Model") and child:FindFirstChild("HumanoidRootPart") then
            local dist = (child.HumanoidRootPart.Position - LocalPlayer.Character.HumanoidRootPart.Position).Magnitude
            if dist < minDist then
                minDist = dist
                nearestChild = child
            end
        end
    end
    if nearestChild then
        LocalPlayer.Character.HumanoidRootPart.CFrame = nearestChild.HumanoidRootPart.CFrame
    end
end

function TeleportFunctions.ToBonfire()
    local bonfire = Workspace:FindFirstChild("Bonfire")
    if bonfire and bonfire:IsA("BasePart") then
        LocalPlayer.Character.HumanoidRootPart.CFrame = bonfire.CFrame + Vector3.new(0,3,0)
    end
end

function TeleportFunctions.ToChests()
    local chestsFolder = Workspace:FindFirstChild("Chests")
    if not chestsFolder then return end
    local nearestChest = nil
    local minDist = math.huge
    for _, chest in pairs(chestsFolder:GetChildren()) do
        if chest:IsA("BasePart") then
            local dist = (chest.Position - LocalPlayer.Character.HumanoidRootPart.Position).Magnitude
            if dist < minDist then
                minDist = dist
                nearestChest = chest
            end
        end
    end
    if nearestChest then
        LocalPlayer.Character.HumanoidRootPart.CFrame = nearestChest.CFrame + Vector3.new(0,3,0)
    end
end

function TeleportFunctions.ChestsToMe()
    local chestsFolder = Workspace:FindFirstChild("Chests")
    if not chestsFolder then return end
    for _, chest in pairs(chestsFolder:GetChildren()) do
        if chest:IsA("BasePart") then
            chest.CFrame = LocalPlayer.Character.HumanoidRootPart.CFrame + Vector3.new(2,0,0)
        end
    end
end

-- =========================
-- AUTO ABRIR BAÚS
-- =========================
local function autoOpenChests()
    if not Config.AutoOpenChests then return end
    local chestsFolder = Workspace:FindFirstChild("Chests")
    if not chestsFolder or not LocalPlayer.Character or not LocalPlayer.Character:FindFirstChild("HumanoidRootPart") then return end
    for _, chest in pairs(chestsFolder:GetChildren()) do
        if chest:IsA("BasePart") then
            local distance = (chest.Position - LocalPlayer.Character.HumanoidRootPart.Position).Magnitude
            if distance < 5 then
                pcall(function()
                    if chest:FindFirstChild("OpenEvent") then
                        chest.OpenEvent:FireServer()
                    end
                end)
            end
        end
    end
end

-- =========================
-- KILL AURA
-- =========================
local function killAura()
    if not Config.KillAura then return end
    local enemiesFolder = Workspace:FindFirstChild("Enemies")
    if not enemiesFolder or not LocalPlayer.Character or not LocalPlayer.Character:FindFirstChild("HumanoidRootPart") then return end
    for _, enemy in pairs(enemiesFolder:GetChildren()) do
        if enemy:IsA("Model") and enemy:FindFirstChild("HumanoidRootPart") then
            local dist = (enemy.HumanoidRootPart.Position - LocalPlayer.Character.HumanoidRootPart.Position).Magnitude
            if dist <= Config.KillAuraRadius then
                pcall(function()
                    if enemy:FindFirstChild("Humanoid") then
                        enemy.Humanoid.Health = 0
                    end
                end)
            end
        end
    end
end

-- =========================
-- LOOP PRINCIPAL
-- =========================
RunService.RenderStepped:Connect(function()
    updateESP()
    autoLoot()
    autoFarmTrees()
    autoOpenChests()
    killAura()
end)

-- =========================
-- MENU UI (simplificado para exemplo)
-- =========================
local ScreenGui = Instance.new("ScreenGui")
ScreenGui.Name = "SuperScriptMenu"
ScreenGui.Parent = LocalPlayer:WaitForChild("PlayerGui")

local Frame = Instance.new("Frame")
Frame.Size = UDim2.new(0, 260, 0, 500)
Frame.Position = UDim2.new(0, 20, 0, 50)
Frame.BackgroundColor3 = Color3.fromRGB(40,40,40)
Frame.BorderSizePixel = 0
Frame.Parent = ScreenGui

-- Aqui você pode adicionar todos os toggles e sliders, seguindo o padrão:
-- createButton("AutoFarm: OFF", function() Config.AutoFarm = not Config.AutoFarm end)
-- createSlider("FarmRadius", min, max, function(value) Config.FarmRadius = value end)
-- createButton("Teleport To Children", function() TeleportFunctions.ToChildren() end)

-- A UI final deve ser expandida com todos os toggles, sliders e botões de teleporte.
