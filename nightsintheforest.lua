--[[
Script Mobile Full Teleporte + AutoAbrir Baús + Menu + AutoFarm
Para 99 Nights in the Forest
Autor: Yuri / GitHub
Compatível: Delta / Mobile Roblox Executor
--]]

repeat task.wait() until game:IsLoaded()

local Players = game:GetService("Players")
local LocalPlayer = Players.LocalPlayer
local Workspace = game:GetService("Workspace")
local RunService = game:GetService("RunService")
local StarterGui = game:GetService("StarterGui")

-- CONFIGURAÇÕES
local Config = {
    ESPEnabled = true,
    AutoLoot = true,
    AutoFarm = true,
    AutoOpenChests = false, -- Nova função separada
    ChatTags = true,
    FarmRadius = 20,
    TreesSimultaneously = 3
}

-- Funções de ESP, AutoLoot, AutoFarm já existentes
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

local function updateESP()
    if not Config.ESPEnabled then return end
    local enemiesFolder = Workspace:FindFirstChild("Enemies")
    if enemiesFolder then
        for _, enemy in pairs(enemiesFolder:GetChildren()) do
            if enemy:FindFirstChild("HumanoidRootPart") then
                createESP(enemy.HumanoidRootPart, Color3.new(1,0,0))
            end
        end
    end
    local lootFolder = Workspace:FindFirstChild("Loot")
    if lootFolder then
        for _, loot in pairs(lootFolder:GetChildren()) do
            if loot:IsA("BasePart") then
                createESP(loot, Color3.new(0,1,0))
            end
        end
    end
    if Config.AutoFarm then
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

local function autoLoot()
    if not Config.AutoLoot then return end
    local lootFolder = Workspace:FindFirstChild("Loot")
    if not lootFolder then return end
    for _, item in pairs(lootFolder:GetChildren()) do
        if item:IsA("BasePart") and LocalPlayer.Character and LocalPlayer.Character:FindFirstChild("HumanoidRootPart") then
            local distance = (item.Position - LocalPlayer.Character.HumanoidRootPart.Position).Magnitude
            if distance < 10 then
                pcall(function()
                    LocalPlayer.Character.HumanoidRootPart.CFrame = CFrame.new(item.Position)
                end)
            end
        end
    end
end

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

-- Funções de Teleporte Full
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

-- Nova função: Auto Abrir Baús
local function autoOpenChests()
    if not Config.AutoOpenChests then return end
    local chestsFolder = Workspace:FindFirstChild("Chests")
    if not chestsFolder or not LocalPlayer.Character or not LocalPlayer.Character:FindFirstChild("HumanoidRootPart") then return end
    for _, chest in pairs(chestsFolder:GetChildren()) do
        if chest:IsA("BasePart") then
            local distance = (chest.Position - LocalPlayer.Character.HumanoidRootPart.Position).Magnitude
            if distance < 5 then -- Distância mínima para abrir baú
                pcall(function()
                    -- Simula abrir o baú
                    if chest:FindFirstChild("OpenEvent") then
                        chest.OpenEvent:FireServer()
                    end
                end)
            end
        end
    end
end

-- MENU UI
local ScreenGui = Instance.new("ScreenGui")
ScreenGui.Name = "MobileMenu"
ScreenGui.Parent = LocalPlayer:WaitForChild("PlayerGui")

local Frame = Instance.new("Frame")
Frame.Size = UDim2.new(0, 260, 0, 360)
Frame.Position = UDim2.new(0, 20, 0, 50)
Frame.BackgroundColor3 = Color3.fromRGB(40,40,40)
Frame.BorderSizePixel = 0
Frame.Parent = ScreenGui

local function createButton(name, position, callback)
    local btn = Instance.new("TextButton")
    btn.Size = UDim2.new(1, -20, 0, 30)
    btn.Position = position
    btn.BackgroundColor3 = Color3.fromRGB(60,60,60)
    btn.BorderSizePixel = 0
    btn.TextColor3 = Color3.fromRGB(255,255,255)
    btn.Text = name
    btn.Parent = Frame
    btn.MouseButton1Click:Connect(callback)
    return btn
end

-- Botões do menu
createButton("ESP: ON", UDim2.new(0, 10, 0, 10), function(btn)
    Config.ESPEnabled = not Config.ESPEnabled
    btn.Text = "ESP: "..(Config.ESPEnabled and "ON" or "OFF")
end)

createButton("AutoLoot: ON", UDim2.new(0, 10, 0, 50), function(btn)
    Config.AutoLoot = not Config.AutoLoot
    btn.Text = "AutoLoot: "..(Config.AutoLoot and "ON" or "OFF")
end)

createButton("AutoFarm: ON", UDim2.new(0, 10, 0, 90), function(btn)
    Config.AutoFarm = not Config.AutoFarm
    btn.Text = "AutoFarm: "..(Config.AutoFarm and "ON" or "OFF")
end)

createButton("Auto Abrir Baús: OFF", UDim2.new(0, 10, 0, 130), function(btn)
    Config.AutoOpenChests = not Config.AutoOpenChests
    btn.Text = "Auto Abrir Baús: "..(Config.AutoOpenChests and "ON" or "OFF")
end)

createButton("TP -> Crianças", UDim2.new(0, 10, 0, 170), function()
    TeleportFunctions.ToChildren()
end)

createButton("TP -> Fogueira", UDim2.new(0, 10, 0, 210), function()
    TeleportFunctions.ToBonfire()
end)

createButton("TP -> Baús", UDim2.new(0, 10, 0, 250), function()
    TeleportFunctions.ToChests()
end)

createButton("Baús -> Você", UDim2.new(0, 10, 0, 290), function()
    TeleportFunctions.ChestsToMe()
end)

-- Loop principal
RunService.RenderStepped:Connect(function()
    pcall(function()
        updateESP()
        autoLoot()
        autoFarmTrees()
        autoOpenChests() -- função nova
    end)
end)

StarterGui:SetCore("SendNotification", {
    Title = "Script Mobile Full",
    Text = "Menu + Teleporte Full + AutoAbrir Baús ativo!",
    Duration = 5
})

print("Script Mobile Full com Teleporte Full e Auto Abrir Baús iniciado!")
