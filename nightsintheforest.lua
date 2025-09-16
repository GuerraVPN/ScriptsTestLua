--[[
SuperScript Mobile Ultra Full v6
Autor: Yuri / GitHub
Compatível: Delta / Mobile Roblox Executor
Funcionalidades: Menu dragável/minimizável/fechável, ESP visual, AutoFarm, AutoLoot, AutoAbrir Baús, Kill Aura, Teleporte Full, sliders de raio
--]]

repeat task.wait() until game:IsLoaded()

-- Serviços
local Players = game:GetService("Players")
local LocalPlayer = Players.LocalPlayer
local Workspace = game:GetService("Workspace")
local RunService = game:GetService("RunService")
local UserInputService = game:GetService("UserInputService")
local StarterGui = game:GetService("StarterGui")

-- CONFIGURAÇÕES INICIAIS
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

-- TELA DE APRESENTAÇÃO
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

-- FUNÇÕES AUXILIARES
local function createESP(part, color)
    if not part or part:FindFirstChild("ESPBox") then return end
    local box = Instance.new("BoxHandleAdornment")
    box.Name = "ESPBox"
    box.Adornee = part
    box.Size = part.Size
    box.Color3 = color
    box.AlwaysOnTop = true
    box.ZIndex = 10
    box.Parent = part
end

-- ESP VISUAL
local function updateESP()
    local enemiesFolder = Workspace:FindFirstChild("Enemies")
    if Config.ESP.Enemies and enemiesFolder then
        for _, enemy in pairs(enemiesFolder:GetChildren()) do
            if enemy:FindFirstChild("HumanoidRootPart") then
                createESP(enemy.HumanoidRootPart, Color3.new(1,0,0))
            end
        end
    end

    local chestsFolder = Workspace:FindFirstChild("Chests")
    if Config.ESP.Chests and chestsFolder then
        for _, chest in pairs(chestsFolder:GetChildren()) do
            if chest:IsA("BasePart") then
                createESP(chest, Color3.new(0,1,0))
            end
        end
    end

    local treesFolder = Workspace:FindFirstChild("Trees")
    if Config.ESP.Trees and treesFolder then
        for _, tree in pairs(treesFolder:GetChildren()) do
            if tree:IsA("BasePart") then
                createESP(tree, Color3.fromRGB(153,76,0))
            end
        end
    end
end

-- AUTOLOOT
local function autoLoot()
    if not Config.AutoLoot or not LocalPlayer.Character then return end
    local lootFolder = Workspace:FindFirstChild("Loot")
    if not lootFolder then return end
    for _, item in pairs(lootFolder:GetChildren()) do
        if item:IsA("BasePart") then
            local dist = (item.Position - LocalPlayer.Character.HumanoidRootPart.Position).Magnitude
            if dist < 10 then
                pcall(function()
                    LocalPlayer.Character.HumanoidRootPart.CFrame = CFrame.new(item.Position)
                end)
            end
        end
    end
end

-- AUTOFARM
local function autoFarmTrees()
    if not Config.AutoFarm or not LocalPlayer.Character then return end
    local treesFolder = Workspace:FindFirstChild("Trees")
    if not treesFolder then return end
    local count = 0
    for _, tree in pairs(treesFolder:GetChildren()) do
        if tree:IsA("BasePart") then
            local dist = (tree.Position - LocalPlayer.Character.HumanoidRootPart.Position).Magnitude
            if dist <= Config.FarmRadius then
                pcall(function()
                    LocalPlayer.Character.HumanoidRootPart.CFrame = CFrame.new(tree.Position + Vector3.new(0,3,0))
                    count = count + 1
                end)
            end
            if count >= Config.TreesSimultaneously then break end
        end
    end
end

-- TELEPORTE FULL
local TeleportFunctions = {}

function TeleportFunctions.ToChildren()
    local childrenFolder = Workspace:FindFirstChild("Children")
    if not childrenFolder or not LocalPlayer.Character then return end
    local nearestChild, minDist = nil, math.huge
    for _, child in pairs(childrenFolder:GetChildren()) do
        if child:IsA("Model") and child:FindFirstChild("HumanoidRootPart") then
            local dist = (child.HumanoidRootPart.Position - LocalPlayer.Character.HumanoidRootPart.Position).Magnitude
            if dist < minDist then nearestChild, minDist = child, dist end
        end
    end
    if nearestChild then
        LocalPlayer.Character.HumanoidRootPart.CFrame = nearestChild.HumanoidRootPart.CFrame
    end
end

function TeleportFunctions.ToBonfire()
    local bonfire = Workspace:FindFirstChild("Bonfire")
    if bonfire and bonfire:IsA("BasePart") and LocalPlayer.Character then
        LocalPlayer.Character.HumanoidRootPart.CFrame = bonfire.CFrame + Vector3.new(0,3,0)
    end
end

function TeleportFunctions.ToChests()
    local chestsFolder = Workspace:FindFirstChild("Chests")
    if not chestsFolder or not LocalPlayer.Character then return end
    local nearestChest, minDist = nil, math.huge
    for _, chest in pairs(chestsFolder:GetChildren()) do
        if chest:IsA("BasePart") then
            local dist = (chest.Position - LocalPlayer.Character.HumanoidRootPart.Position).Magnitude
            if dist < minDist then nearestChest, minDist = chest, dist end
        end
    end
    if nearestChest then
        LocalPlayer.Character.HumanoidRootPart.CFrame = nearestChest.CFrame + Vector3.new(0,3,0)
    end
end

function TeleportFunctions.ChestsToMe()
    local chestsFolder = Workspace:FindFirstChild("Chests")
    if not chestsFolder or not LocalPlayer.Character then return end
    for _, chest in pairs(chestsFolder:GetChildren()) do
        if chest:IsA("BasePart") then
            chest.CFrame = LocalPlayer.Character.HumanoidRootPart.CFrame + Vector3.new(2,0,0)
        end
    end
end

-- AUTO ABRIR BAÚS
local function autoOpenChests()
    if not Config.AutoOpenChests or not LocalPlayer.Character then return end
    local chestsFolder = Workspace:FindFirstChild("Chests")
    if not chestsFolder then return end
    for _, chest in pairs(chestsFolder:GetChildren()) do
        if chest:IsA("BasePart") then
            local dist = (chest.Position - LocalPlayer.Character.HumanoidRootPart.Position).Magnitude
            if dist < 5 then
                pcall(function()
                    if chest:FindFirstChild("OpenEvent") then
                        chest.OpenEvent:FireServer()
                    end
                end)
            end
        end
    end
end

-- KILL AURA
local function killAura()
    if not Config.KillAura or not LocalPlayer.Character then return end
    local enemiesFolder = Workspace:FindFirstChild("Enemies")
    if not enemiesFolder then return end
    for _, enemy in pairs(enemiesFolder:GetChildren()) do
        if enemy:IsA("Model") and enemy:FindFirstChild("HumanoidRootPart") and enemy:FindFirstChild("Humanoid") then
            local dist = (enemy.HumanoidRootPart.Position - LocalPlayer.Character.HumanoidRootPart.Position).Magnitude
            if dist <= Config.KillAuraRadius then
                pcall(function()
                    enemy.Humanoid.Health = 0
                end)
            end
        end
    end
end

-- LOOP PRINCIPAL
RunService.RenderStepped:Connect(function()
    updateESP()
    autoLoot()
    autoFarmTrees()
    autoOpenChests()
    killAura()
end)

-- MENU UI FUNCIONAL
local ScreenGui = Instance.new("ScreenGui")
ScreenGui.Name = "SuperScriptMenu"
ScreenGui.Parent = LocalPlayer:WaitForChild("PlayerGui")

local Frame = Instance.new("Frame")
Frame.Size = UDim2.new(0, 300, 0, 600)
Frame.Position = UDim2.new(0, 20, 0, 50)
Frame.BackgroundColor3 = Color3.fromRGB(40,40,40)
Frame.BorderSizePixel = 0
Frame.Parent = ScreenGui

-- FUNÇÕES DE DRAG, MINIMIZE E FECHAR
Frame.Active = true
Frame.Draggable = true

local CloseBtn = Instance.new("TextButton")
CloseBtn.Size = UDim2.new(0,30,0,30)
CloseBtn.Position = UDim2.new(1,-35,0,5)
CloseBtn.Text = "X"
CloseBtn.BackgroundColor3 = Color3.fromRGB(200,50,50)
CloseBtn.TextColor3 = Color3.fromRGB(255,255,255)
CloseBtn.Parent = Frame
CloseBtn.MouseButton1Click:Connect(function()
    ScreenGui:Destroy()
end)

local MinBtn = Instance.new("TextButton")
MinBtn.Size = UDim2.new(0,30,0,30)
MinBtn.Position = UDim2.new(1,-70,0,5)
MinBtn.Text = "-"
MinBtn.BackgroundColor3 = Color3.fromRGB(100,100,100)
MinBtn.TextColor3 = Color3.fromRGB(255,255,255)
MinBtn.Parent = Frame

local toggled = true
MinBtn.MouseButton1Click:Connect(function()
    toggled = not toggled
    for _,child in pairs(Frame:GetChildren()) do
        if child:IsA("TextButton") and child ~= CloseBtn and child ~= MinBtn then
            child.Visible = toggled
        end
    end
end)

-- Funções de toggle
local function createToggle(text, configTable, key)
    local btn = Instance.new("TextButton")
    btn.Size = UDim2.new(1, -10, 0, 30)
    btn.Position = UDim2.new(0,5,0,#Frame:GetChildren()*35)
    btn.Text = text..": OFF"
    btn.BackgroundColor3 = Color3.fromRGB(60,60,60)
    btn.TextColor3 = Color3.fromRGB(255,255,255)
    btn.Parent = Frame
    btn.MouseButton1Click:Connect(function()
        configTable[key] = not configTable[key]
        btn.Text = text..": "..(configTable[key] and "ON" or "OFF")
    end)
end

createToggle("ESP Enemies", Config.ESP, "Enemies")
createToggle("ESP Chests", Config.ESP, "Chests")
createToggle("ESP Trees", Config.ESP, "Trees")
createToggle("AutoLoot", Config, "AutoLoot")
createToggle("AutoFarm", Config, "AutoFarm")
createToggle("AutoOpenChests", Config, "AutoOpenChests")
createToggle("KillAura", Config, "KillAura")

-- Botões de Teleporte
local function createTPButton(text, func)
    local btn = Instance.new("TextButton")
    btn.Size = UDim2.new(1, -10, 0, 30)
    btn.Position = UDim2.new(0,5,0,#Frame:GetChildren()*35)
    btn.Text = text
    btn.BackgroundColor3 = Color3.fromRGB(80,80,80)
    btn.TextColor3 = Color3.fromRGB(255,255,255)
    btn.Parent = Frame
    btn.MouseButton1Click:Connect(func)
end

createTPButton("Teleport To Children", TeleportFunctions.ToChildren)
createTPButton("Teleport To Bonfire", TeleportFunctions.ToBonfire)
createTPButton("Teleport To Chests", TeleportFunctions.ToChests)
createTPButton("Bring Chests To Me", TeleportFunctions.ChestsToMe)

-- Sliders
local function createSlider(name, configTable, key, min, max)
    local sliderLabel = Instance.new("TextLabel")
    sliderLabel.Size = UDim2.new(1, -10, 0, 20)
    sliderLabel.Position = UDim2.new(0,5,0,#Frame:GetChildren()*35)
    sliderLabel.Text = name..": "..tostring(configTable[key])
    sliderLabel.TextColor3 = Color3.fromRGB(255,255,255)
    sliderLabel.BackgroundTransparency = 1
    sliderLabel.Parent = Frame

    local slider = Instance.new("TextButton")
    slider.Size = UDim2.new(1, -10, 0, 20)
    slider.Position = UDim2.new(0,5,0,#Frame:GetChildren()*35)
    slider.Text = ""
    slider.BackgroundColor3 = Color3.fromRGB(100,100,100)
    slider.Parent = Frame

    slider.MouseButton1Click:Connect(function(input)
        local mouse = game.Players.LocalPlayer:GetMouse()
        local function onMove()
            local newVal = math.clamp(math.floor((mouse.X - slider.AbsolutePosition.X)/slider.AbsoluteSize.X*(max-min)+min), min, max)
            configTable[key] = newVal
            sliderLabel.Text = name..": "..tostring(newVal)
        end
        local conn
        conn = UserInputService.InputChanged:Connect(onMove)
        UserInputService.InputEnded:Connect(function()
            conn:Disconnect()
        end)
    end)
end

createSlider("FarmRadius", Config, "FarmRadius", 5, 50)
createSlider("KillAuraRadius", Config, "KillAuraRadius", 5, 50)
createSlider("TreesSimultaneously", Config, "TreesSimultaneously", 1, 10)
