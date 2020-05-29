#include "Memory.h"
#include <iostream>
#include <TlHelp32.h>
 
Memory* Mem; // Execute memory manager in main entry point.
 
// Offsets for current and future functions.
#define localPlayer = 0xCBD6B4;
#define entityList = 0x4CCDBFC;
#define iTeamNum = 0xF4;
#define bSpotted = 0x93D;
#define iGlowIndex = 0xA3F8; // Glow ESP function perhaps using in game feature.
#define objectGlowManager = 0x520DA28;
#define flashDuration = 0xA3E0;
#define modelAmbientMin = 0x58ED1C;
#define crosshairID = 0xB390;
#define fireWeapon = 0x30FF2A0;
#define fovOf = 0x31E4;
#define viewAngle = 0x3020;
#define aimAngle = 0x302C;
#define weaponScoped = 0x390A;
#define render = 0x70;
#define iHealth = 0x100;

// Offsets for ESP.
#define viewMatrix = 0x4CF6534;
#define boneMatrix = 0x26A8;
#define position = 0x138;
#define bDormant = 0xED;

// Offsets for TriggerBot.
#define iCrossHairID 0xA940

// Offset for No Flash.
#define flashDuration = 0xA3F4;

// Offsets for Bhop.
#define entityAutoJump = 0x51A81BC;
#define entityJumpState = 0x104;

struct Vec2 { // Vec2 stores screen 2D x, y coordinates.
    float x, y;
};

struct Vec3 { // Vec3 stores player position.
    float x, y, z;
};

struct Vec4 { // Vec4 stores clip coordinates.
    float x, y, z, w;
};

struct entity { // Read & store entity info into entity array.
    DWORD currentEntity = main.currentEntity;
    DWORD bone;
    Vec3 entPos;
    Vec3 entBonePos;
    int team;
    int health;
    int dormant;
    void ReadentityInfo(int player) {
        ReadProcessMemory(Mem.getProcHandle(), (LPVOID)(Mem.ClientDLL + main.entitylist + main.localPlayer * 0x10), &currentEntity, sizeof(currentEntity), 0);
        ReadProcessMemory(Mem.getProcHandle(), (LPVOID)(currentEntity + game.dw_position), &entPos, sizeof(entPos), 0);
        ReadProcessMemory(Mem.getProcHandle(), (LPVOID)(main.currentEntity + main.boneMatrix), &bone, sizeof(bone), 0);
        ReadProcessMemory(Mem.getProcHandle(), (Vec3*)(bone + 0x30 * 9 + 0x0C), &entBonePos.x, sizeof(entBonePos.x), 0);
        ReadProcessMemory(Mem.getProcHandle(), (Vec3*)(bone + 0x30 * 9 + 0x1C), &entBonePos.y, sizeof(entBonePos.y), 0);
        ReadProcessMemory(Mem.getProcHandle(), (Vec3*)(bone + 0x30 * 9 + 0x2C), &entBonePos.z, sizeof(entBonePos.z), 0);
        ReadProcessMemory(Mem.getProcHandle(), (LPINT)(currentEntity + main.iTeamNum), &team, sizeof(team), 0);
        ReadProcessMemory(Mem.getProcHandle(), (LPINT)(currentEntity + main.iHealth), &health, sizeof(health), 0);
        ReadProcessMemory(Mem.getProcHandle(), (LPINT)(entityList + main.bDormant), &dormant, sizeof(dormant), 0);
    }
}PlayerList[64]; // Array of players.

struct me { // Read & Store user info (My team and entity).
    DWORD Player;
    int Team;
    void ReadMyINFO()
    {
        ReadProcessMemory(Mem.getProcHandle(), (LPVOID)(Mem.ClientDLL + main.localPlayer), &Player, sizeof(Player), 0);
        ReadProcessMemory(Mem.getProcHandle(), (LPINT)(Player + main.iTeamNum), &Team, sizeof(Team), 0);
    }
}Me;

float ScreenX = GetSystemMetrics(SM_CXSCREEN); // Get Width of screen for ESP.
float ScreenY = GetSystemMetrics(SM_CYSCREEN); // Get Height of screen for ESP.

struct playerESP { // Variables For ESP (Box Drawing, Game window, Entity Info).
    HBRUSH Brush;
    HFONT Font;
    HDC gameWindow;
    HWND game;
    COLORREF textColor;
    HPEN snapLineColor;
}esp;

// ESP Functions.

void DrawFilledRect(int x, int y, int w, int h) {
    RECT rect = { x, y, x + w, y + h };
    FillRect(esp.gameWindow, &rect, esp.Brush);
}

void DrawBorderBox(int x, int y, int w, int h, int thickness) { // Draws box.
    DrawFilledRect(x, y, w, thickness); // Bottom Horizontal Line.
    DrawFilledRect(x, y, thickness, h); // Right Vertical Line.
    DrawFilledRect((x + w), y, thickness, h); // Left Vertical Line.
    DrawFilledRect(x, y + h, w + thickness, thickness); // Top Horizontal Line.
}

void DrawLine(int x, int y, HPEN LineColor) { // Draws the Snaplines.
    MoveToEx(esp.gameWindow, (ScreenX/2), (ScreenY/2), NULL);
    LineTo(esp.gameWindow, x, y);
    esp.snapLineColor = CreatePen(PS_SOLID, 1, RGB(0, 255, 255));
    SelectObject(esp.gameWindow, esp.snapLineColor);
}

void DrawString(int x, int y, COLORREF color, const char* text) { // Draws the text on screen.
    SetTextAlign(esp.gameWindow, TA_CENTER | TA_NOUPDATECP); //TA_NOUPDATECP: The current position is not updated after each text output call.
    SetBkColor(esp.gameWindow, RGB(0, 0, 0));
    SetBkMode(esp.gameWindow, TRANSPARENT); // TRANSPARENT: Background remains untouched.
    SetTextColor(esp.gameWindow, color);
    SelectObject(esp.gameWindow, esp.Font);
    TextOutA(esp.gameWindow, x, y, text, strlen(text));
    DeleteObject(esp.Font);
}

bool WorldToScreen(Vec3 pos, Vec2 &screen, float matrix[16], int windowWidth, int windowHeight) { // converts 3D location coordinates to 2D for player screen.
    // Matrix-vector Product, multiplying world(eye) coordinates by projection matrix = clipCoords.
    Vec4 clipCoords;
    clipCoords.x = pos.x*matrix[0] + pos.y*matrix[1] + pos.z*matrix[2] + matrix[3];
    clipCoords.y = pos.x*matrix[4] + pos.y*matrix[5] + pos.z*matrix[6] + matrix[7];
    clipCoords.z = pos.x*matrix[8] + pos.y*matrix[9] + pos.z*matrix[10] + matrix[11];
    clipCoords.w = pos.x*matrix[12] + pos.y*matrix[13] + pos.z*matrix[14] + matrix[15];
    if (clipCoords.w < 0.1f) // Don't draw if located behind player POV.
        return false;
    Vec3 NDC;
    // Normalize coordinates.
    NDC.x = clipCoords.x / clipCoords.w;
    NDC.y = clipCoords.y / clipCoords.w;
    NDC.z = clipCoords.z / clipCoords.w;
    // Convert to window/screen coordinates (x, y).
    screen.x = (windowWidth / 2 * NDC.x) + (NDC.x + windowWidth / 2);
    screen.y = -(windowHeight / 2 * NDC.y) + (NDC.y + windowHeight / 2);
    return true;
}

// To press key and toggle player ESP function on/off.
boolean playerESPToggled = false;

void playerESP() { // Main ESP Function, reads through entities and draws boxes and player info on them.
    if (GetAsyncKeyState(VK_F2)) { // If pressed F2 key, toggle on/off.
        playerESPToggled = !playerESPToggled;
        Sleep(200); // So pressing key doesn't switch on/off the moment it presses.
    }
    if(playerESPToggled) { // Draw ESP boxes + player info if playerESP is toggled on.
        Vec2 vHead;
        Vec2 vScreen;
        float Matrix[16]; // [4][4]: 4*4 = 16
        ReadProcessMemory(Mem.getProcHandle(), (PFLOAT)(Mem.ClientDLL + main.viewMatrix), &Matrix, sizeof(Matrix), 0);
        for (int i = 0; i < 64; i++) { // Loops through all entities.
            PlayerList[i].ReadentityInfo(i);
            Me.ReadMyINFO();
            if (PlayerList[i].currentEntity != NULL) {
                if (PlayerList[i].currentEntity != Me.Player) {
                    if (PlayerList->dormant == 0) { // Checks if players are being updated by server.
                        if (PlayerList[i].health > 0) { // Checks if entities are alive.
                            if (WorldToScreen(PlayerList[i].entPos, vScreen, Matrix, ScreenX, ScreenY)) {
                                if (WorldToScreen(PlayerList[i].entBonePos, vHead, Matrix, ScreenX, ScreenY)) {
                                    // ESP Box Size Calculations.
                                    float head = vHead.y - vScreen.y;
                                    float width = head / 2;
                                    float center = width / -2;
                                    if (PlayerList[i].Team == Me.Team) { // Draws Team ESP Boxes.
                                        esp.Brush = CreateSolidBrush(RGB(0, 0, 255));
                                        DrawBorderBox(vScreen.x + center, vScreen.y, width, head - 5, 2);
                                        DrawLine(vScreen.x, vScreen.y, esp.snapLineColor);
                                        esp.textColor = RGB(0, 255, 0);
                                        char Healthchar[255];
                                        sprintf_s(Healthchar, sizeof(Healthchar), "Health: %i", (int)(PlayerList[i].health));
                                        DrawString(vScreen.x, vScreen.y, esp.textColor, Healthchar);
                                        DeleteObject(esp.Brush);
                                    } else { // Draws Enemy ESP Boxes.
                                        esp.Brush = CreateSolidBrush(RGB(255, 0, 0));
                                        DrawBorderBox(vScreen.x + center, vScreen.y, width, head, 2);
                                        DrawLine(vScreen.x, vScreen.y, esp.snapLineColor);
                                        DeleteObject(esp.Brush);
                                        esp.textColor = RGB(0, 255, 0);
                                        char Healthchar[255];
                                        sprintf_s(Healthchar, sizeof(Healthchar), "Health: %i", (int)(PlayerList[i].health)); // Prints health value of enemy.
                                        DrawString(vScreen.x, vScreen.y, esp.textColor, Healthchar);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

// To press key and toggle no flash function on/off.
boolean noFlashToggled = false;

void noFlash() { // Simple No Flash Function (No Player Blinding from Flash Utility).
    if (GetAsyncKeyState(VK_F3)) { // If pressed F3 key, toggle on/off.
        noFlashToggled = !noFlashToggled;
        Sleep(200); // So pressing key doesn't switch on/off the moment it presses.
    }
    if(noFlashToggled){
        DWORD myEntity; // Represents your player.
        int flashDuration;
        int noFlashVal = 0;
        ReadProcessMemory(Mem.getProcHandle(), (LPVOID)(Mem.ClientDLL + main.localPlayer), &myEntity, sizeof(myEntity), 0);
        if (myEntity == NULL) {
            while (myEntity == NULL) {
                ReadProcessMemory(Mem.getProcHandle(), (LPVOID)(Mem.ClientDLL + main.localPlayer), &myEntity, sizeof(myEntity), 0);
            }
        }
        ReadProcessMemory(Mem.getProcHandle(), (LPINT)(myEntity + main.flashDuration), &flashDuration, sizeof(flashDuration), 0);
        if (flashDuration > 0) { // Cancels out flash blinding.
            WriteProcessMemory(Mem.getProcHandle(), (LPINT)(myEntity + main.flashDuration), &noFlashVal, sizeof(noFlashVal), 0);
        }
    }
}

// To press key and toggle triggerbot function on/off.
boolean triggerToggled = false;

void triggerBot() { // Trigger Bot Main Function: Attack automatically if enemy is in player's crosshair.
    // Check for toggle on/off.
    if (GetAsyncKeyState(VK_F1)) { // If pressed F1 key, toggle on/off.
        triggerToggled = !triggerToggled;
        Sleep(200); // So pressing key doesn't switch on/off the moment it presses.
    }
    if (triggerToggled) { // If triggerbot toggled, proceed triggerbot.
        // Retrieve player information.
        DWORD LocalPlayer_Base = Mem->Read<DWORD>(Mem->ClientDLL_Base + m_dwLocalPlayer);
        int localPlayerInCross = Mem->Read<int>(LocalPlayer_Base + main.iCrossHairID); // Checks entity in player's crosshair.
        int localPlayerTeam = Mem->Read<int>(LocalPlayer_Base + main.iTeamNum); // Gets their team. (Want to check team before activating).
        // Retrieve the EntityBase, using dwEntityList.
        DWORD triggerEntityBase = Mem->Read<DWORD>(Mem->ClientDLL_Base + main.entityList + ((LocalPlayer_inCross - 1)*0x10));
        int triggerEntityTeam = Mem->Read<int>(triggerEntityBase + main.iTeamNum);
        bool triggerEntityDormant = Mem->Read<bool>(triggerEntityBase + main.bDormant);
        // If in enemy entity is in the player crosshair, attack automatically.
        if ((localPlayerInCross > 0 && localPlayerInCross <= 64) && (triggerEntityBase != NULL) && (triggerEntityTeam != localPlayerTeam) && (!triggerEntityDormant)) {
            Sleep(10); // Delay before attacking to make sure not to shoot short of the enemy (Make sure is in entity "blob").
            mouse_event(MOUSEEVENTF_LEFTDOWN,NULL,NULL,NULL,NULL); // Mouse left click shoot.
            Sleep(10); // Delay between shots.
            mouse_event(MOUSEEVENTF_LEFTUP, NULL, NULL, NULL, NULL); // Mouse left click release.
            Sleep(10); // Delay after shooting.
        }
    }
}

// To press key and toggle bhop function on/off.
boolean autoHopToggled = false;

void autoHop() { // AutoHop Function: Automatically jumps/bhops when holding space bar done if toggled on.
    // Check for toggle on/off.
    if (GetAsyncKeyState(VK_F4)) { // If pressed F4 key, toggle on/off.
        autoHopToggled = !autoHopToggled;
        Sleep(200); // So pressing key doesn't switch on/off the moment it presses.
    }
    if(autoHopToggled){ // If bhop toggled, proceed bhop function.
        BYTE autoJump = 6; // Represents forced jump interval: autoJump: 4 = On Ground, 5 = Jumping, 6 = Reset to 4 to be able to jump again.
        int flags; // Represents player state: flags: 257 = On Ground, 256 = Jumping.
        DWORD myEntity;
        ReadProcessMemory(Mem.getProcHandle(), (LPVOID)(Mem.ClientDLL + main.localPlayer), &myEntity, sizeof(myEntity), 0);
        if (myEntity == NULL) {
            while (myEntity == NULL) {
                ReadProcessMemory(Mem.getProcHandle(), (LPVOID)(Mem.ClientDLL + main.localPlayer), &myEntity, sizeof(myEntity), 0);
            }
        }
        ReadProcessMemory(Mem.getProcHandle(), (PBYTE)(myEntity + main.entityJumpState), &flags, sizeof(flags), 0);
        if (GetAsyncKeyState(VK_SPACE) && flags & (1 << 0)) { // Checks if space bar is held and swaps player state to jump.
            WriteProcessMemory(Mem.getProcHandle(), (PBYTE)(Mem.ClientDLL + main.entityAutoJump), &autoJump, sizeof(autoJump), 0);
            Sleep(1); // Jump on tickrate intervals, Change this depending on server tickrate to work properly.
        }
    }
}

void aimbot() { // Aimbot Function: Automatically assist player in aiming crosshair to enemy.
    // Make entity loop and execute world to screen function to snap onto enemy in range.
}

boolean smooth = false; // Smoother variable.

void aimAtPos() { // Helper Function to move mouse to enemy in range.
    int distMax = 75;  // Max Distance from crosshair an enemy will be targeted.
    double lowDist = distMax;
    float aimPosX = 1920 / 2; // X Position of crosshair on screen. (Change these values if screen resolution is different)
    float aimPosY = 1080 / 2; // Y Position of crosshair on screen.
    float screenCenterX = (this.Width / 2);
    float screenCenterY = (this.Height / 2);
    float targetX = 0;
    float targetY = 0;
    // Calculate mouse angles.
    if (x != 0) {
        if (x > screenCenterX) {
            targetX = -(screenCenterX - x);
            targetX /= AimSpeed;
            if (targetX + screenCenterX > screenCenterX * 2)
                targetX = 0;
        }
        if (x < screenCenterX) {
            targetX = x - screenCenterX;
            targetX /= AimSpeed;
            if (targetX + screenCenterX < 0) 
                targetX = 0;
        }
    }
    if (y != 0) {
        if (y > screenCenterY) {
            targetY = -(screenCenterY - y);
            targetY /= AimSpeed;
            if (targetY + screenCenterY > screenCenterY * 2) 
                targetY = 0;
        }
        if (y < screenCenterY) {
            targetY = y - screenCenterY;
            targetY /= AimSpeed;
            if (targetY + screenCenterY < 0) 
                targetY = 0;
        }
    }
    if (!smooth) {
        mouse_event(0x0001, (uint)(targetX), (uint)(targetY), NULL, NULLPTR);
        return;
    }
    targetX /= 10;
    targetY /= 10;
    if (Math.Abs(targetX) < 1) {
        if (targetX > 0) {
            targetX = 1;
        }
        if (targetX < 0) {
            targetX = -1;
        }
    }
    if (Math.Abs(targetY) < 1) {
        if (targetY > 0) {
            targetY = 1;
        }
        if (targetY < 0) {
            targetY = -1;
        }
    }
    mouse_event(0x0001, (uint)targetX, (uint)targetY, NULL, NULLPTR);
}

int main() { // Main process.
    SetConsoleTitleA("CSGO Assistant");
    Mem = new MemoryManager();
    esp.game = FindWindowA(0, "Counter-Strike: Global Offensive"); // Gets game window for ESP feature.
    esp.gameWindow = GetDC(esp.game);
    while (Mem.getProcID() != NULL) { // Runs playerESP, noFlash, triggerBot, and autoHop that the user can toggle on and off until game application closes.
        playerESP();
        noFlash();
        triggerBot();
        autoHop();
    }
    CloseHandle(Mem.getProcHandle());
    delete Mem; // Deletes Memory Manager pointer to execute destructor/close hProc.
    return 0;
}