/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2021 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include <3ds.h>
#include "luma_config.h"
#include "menus/sysconfig.h"
#include "memory.h"
#include "draw.h"
#include "fmt.h"
#include "utils.h"
#include "ifile.h"
#include "luminance.h"
#include "menus/lang.h"

static Menu audioRoutingMenu = {
    TR("音声の出力先", "Audio routing menu"),
    {
        { TR("自動（初期設定）", "Auto (default)"), METHOD, .method = &SysConfigMenu_SetAudioRoutingAuto },
        { TR("ヘッドホンにする（抜き差しで再適用）", "Force headphones (reapply on HP ins./rem.)"), METHOD, .method = &SysConfigMenu_SetAudioRoutingHeadphones },
        { TR("スピーカーにする（抜き差しで再適用）", "Force speakers (reapply on HP ins./rem.)"), METHOD, .method = &SysConfigMenu_SetAudioRoutingSpeakers },
        {},
    }
};

Menu sysconfigMenu = {
    TR("システム設定", "System configuration menu"),
    {
        { TR("音量を上書きする...", "Set volume slider override..."), METHOD, .method=&SysConfigMenu_AdjustVolume},
        { TR("無線の接続先を選ぶ", "Control Wireless connection"), METHOD, .method = &SysConfigMenu_ControlWifi },
        { TR("LED の明るさを上書きする...", "Set LED master brightness override..."), METHOD, .method = &SysConfigMenu_SetLedMasterBrightnessOverride },
        { TR("音声の出力先...", "Set audio routing..."), MENU, .menu = &audioRoutingMenu },
        { TR("無線の入／切", "Toggle Wireless"), METHOD, .method = &SysConfigMenu_ToggleWireless },
        { TR("電源ボタンの有効／無効", "Toggle Power Button"), METHOD, .method=&SysConfigMenu_TogglePowerButton },
        { TR("カードスロットの電源の入／切", "Toggle power to card slot"), METHOD, .method=&SysConfigMenu_ToggleCardIfPower},
        { TR("画面の明るさを変える", "Change screen brightness"), METHOD, .method = &SysConfigMenu_ChangeScreenBrightness },
        {},
    }
};

bool isConnectionForced = false;

static Result SysConfigMenu_ApplyLedMasterBrightnessOverride(void)
{
    u8 override = lumaMcuConfig.ledMasterBrightnessOverride;

    // Round to nearest integer. 255 (max brightness) is the default value
    // We deliberately leave the POWER LED setting as "auto" instead of "off" so that the red POWER LED appears
    // regardless of setting. Note that the red POWER LED is not dimmable.
    u8 brightness = PctOverride_IsActive(override) ? (255 * PctOverride_GetPercentage(override) + 50) / 100 : 255;

    Result res = mcuHwcInit();
    if (R_SUCCEEDED(res))
    {
        res = MCUHWC_WriteRegister(0x28, &brightness, 1);
        mcuHwcExit();
    }

    return res;
}

static void SysConfigMenu_SetLedMasterBrightnessOverrideValue(u8 override)
{
    if (override == lumaMcuConfig.ledMasterBrightnessOverride)
        return;

    lumaMcuConfig.ledMasterBrightnessOverride = override;
    // Let kernel know for MCURTC interception (to prevent PTM from setting
    // pattern and brightness if we set override to 0%)
    svcKernelSetState(0x10008, override);
    SysConfigMenu_ApplyLedMasterBrightnessOverride();
    LumaConfig_SaveMcuConfig();
}

// Character grid positions (10px margins, same as the title)
#define SYSCONFIG_COL(x) (10 + (x) * SPACING_X)
#define SYSCONFIG_ROW(y) (10 + (y) * SPACING_Y)

static void SysConfigMenu_DrawOverrideStatus(u8 override)
{
    bool enabled = PctOverride_IsEnabled(override);
    Draw_DrawString(SYSCONFIG_COL(0), SYSCONFIG_ROW(2), COLOR_WHITE, TR("上書きの状態:       ", "Override status:    "));
    Draw_DrawString(SYSCONFIG_COL(20), SYSCONFIG_ROW(2), enabled ? COLOR_GREEN : COLOR_RED, enabled ? TR("有効    ", "ENABLED ") : TR("無効    ", "DISABLED"));
    Draw_DrawFormattedString(SYSCONFIG_COL(28), SYSCONFIG_ROW(2), COLOR_WHITE, TR("  （%s）    ", "  (%s)      "), PctOverride_HasValue(override) ? TR("値あり", "set") : TR("値なし", "not set"));
}

void SysConfigMenu_SetLedMasterBrightnessOverride(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    // Keep the mcu::HWC session open while this window is open (the helpers' init/exit then only update the refcount)
    bool mcuHwcOpened = R_SUCCEEDED(mcuHwcInit());

    do
    {
        u8 override = lumaMcuConfig.ledMasterBrightnessOverride;
        // Always use 100% as the sampled value, for correctness and consistency - as nothing else changes it
        u8 curPct = PctOverride_HasValue(override) ? PctOverride_GetPercentage(override) : 100;

        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("システム設定", "System configuration menu"));
        SysConfigMenu_DrawOverrideStatus(override);
        Draw_DrawFormattedString(SYSCONFIG_COL(0), SYSCONFIG_ROW(3), COLOR_WHITE, TR("上書きの値:         %u%%  ", "Override value:     %u%%  "), curPct);
        Draw_DrawString(SYSCONFIG_COL(0), SYSCONFIG_ROW(5), COLOR_WHITE,
            TR("A: 上書きの入／切\n"
               "左右: 値を 10 ずつ変える\n"
               "上下: 値を 1 ずつ変える\n"
               "X: 上書きの値を消す／今の値にする\n"
               "B: 戻る\n",
               "Press A to toggle override.\n"
               "Press Right/Left to +/- the value by 10 points.\n"
               "Press Up/Down to +/- the value by 1 point.\n"
               "Press X to clear or refresh the override.\n"
               "Press B to go back.\n")
        );
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(15); // redraw at about frame rate

        if (pressed & KEY_B)
            break;

        override = lumaMcuConfig.ledMasterBrightnessOverride;
        curPct = PctOverride_HasValue(override) ? PctOverride_GetPercentage(override) : 100;
        s32 delta = 0;

        if (pressed & KEY_A)
        {
            override = PctOverride_SetEnabled(override, !PctOverride_IsEnabled(override));

            // Ensure proper value is set when enabling: SetEnabled alone would leave an override
            // that was never set in the "enabled but not set" state, which means "no override"
            if (PctOverride_IsEnabled(override))
                override = PctOverride_SetPercentage(override, curPct);

            SysConfigMenu_SetLedMasterBrightnessOverrideValue(override);
        }
        else if (pressed & KEY_LEFT)
            delta = -10;
        else if (pressed & KEY_RIGHT)
            delta = +10;
        else if (pressed & KEY_DOWN)
            delta = -1;
        else if (pressed & KEY_UP)
            delta = +1;
        else if (pressed & KEY_X)
        {
            // Unset the override, or refresh it if enabled
            override = PctOverride_IsEnabled(override) ? PctOverride_SetPercentage(override, 100) : PCT_OVERRIDE_UNSET;
            SysConfigMenu_SetLedMasterBrightnessOverrideValue(override);
        }

        if (delta != 0)
            SysConfigMenu_SetLedMasterBrightnessOverrideValue(PctOverride_SetPercentage(override, (u8)CLAMP((s32)curPct + delta, 0, 100)));
    }
    while(!menuShouldExit);

    if (mcuHwcOpened)
        mcuHwcExit();
}

static Result SysConfigMenu_OpenCdcChkSession(void)
{
    // TODO: stop opening and closing cdc:CHK, which unecessarily spawns and despawns threads.

    Handle *cdcChkHandlePtr = cdcChkGetSessionHandle();
    *cdcChkHandlePtr = 0;

    Result res = srvGetServiceHandle(cdcChkHandlePtr, "cdc:CHK");
    // Try to steal the handle if some other process is using the service (custom SVC)
    if (R_FAILED(res))
        res = svcControlService(SERVICEOP_STEAL_CLIENT_SESSION, cdcChkHandlePtr, "cdc:CHK");

    return res;
}

static void SysConfigMenu_CloseCdcChkSession(void)
{
    Handle *cdcChkHandlePtr = cdcChkGetSessionHandle();
    svcCloseHandle(*cdcChkHandlePtr);
    *cdcChkHandlePtr = 0;
}

Result SysConfigMenu_ApplyAudioRouting(void)
{
    // DSP/Codec sysmodule already have a way to force headphone output,
    // but it's only for when the shell is closed (applied on shell close,
    // cleared on shell opened); that mechanism is usually used by apps
    // which have a "jukebox" feature  (e.g Pokémon SMD).

    // This whole thing here is fragile and doesn't mesh well with the "codec"
    // sysmodule. For example, inserting then removing HPs will undo what this
    // function does.

    bool headphones;
    switch (LumaConfig_GetAudioRouting())
    {
        case AUDIO_ROUTING_SPEAKERS:
            headphones = false;
            break;
        case AUDIO_ROUTING_HEADPHONES:
            headphones = true;
            break;
        case AUDIO_ROUTING_AUTO:
        default:
            // Restore what the codec sysmodule would have set. GPIO2 bit0: headphones inserted
            headphones = (*(vu8 *)((0x10147000 | (1u << 31)) + 0x10) & 1) != 0;
            break;
    }

    Result res = SysConfigMenu_OpenCdcChkSession();
    if (R_FAILED(res))
        return res;

    // 0x20 is the override enable bit (also set by the codec sysmodule), 0x10 selects headphones
    u8 reg;
    res = CDCCHK_ReadRegisters2(100, 69, &reg, 1);
    reg = (reg & ~0x30) | 0x20 | (headphones ? 0x10 : 0x00);
    if (R_SUCCEEDED(res)) res = CDCCHK_WriteRegisters2(100, 69, &reg, 1);

    SysConfigMenu_CloseCdcChkSession();
    return res;
}

static void SysConfigMenu_SetAudioRouting(AudioRouting routing)
{
    LumaConfig_SetMcuConfigFlags(MCUCFG_FLAGS_AUDIO_ROUTING_MASK, (u8)routing << MCUCFG_FLAGS_AUDIO_ROUTING_SHIFT);
    SysConfigMenu_ApplyAudioRouting();
}

void SysConfigMenu_SetAudioRoutingAuto(void)
{
    SysConfigMenu_SetAudioRouting(AUDIO_ROUTING_AUTO);
}

void SysConfigMenu_SetAudioRoutingHeadphones(void)
{
    SysConfigMenu_SetAudioRouting(AUDIO_ROUTING_HEADPHONES);
}

void SysConfigMenu_SetAudioRoutingSpeakers(void)
{
    SysConfigMenu_SetAudioRouting(AUDIO_ROUTING_SPEAKERS);
}

void SysConfigMenu_ToggleWireless(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    bool nwmRunning = isServiceUsable("nwm::EXT");

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("システム設定", "System configuration menu"));
        Draw_DrawString(10, 30, COLOR_WHITE, TR("A: 切り替え   B: 戻る", "A: switch   B: back"));

        u8 wireless = (*(vu8 *)((0x10140000 | (1u << 31)) + 0x180));

        if(nwmRunning)
        {
            Draw_DrawString(10, 50, COLOR_WHITE, TR("今の状態:", "Current status:"));
            Draw_DrawString(100, 50, (wireless ? COLOR_GREEN : COLOR_RED), (wireless ? TR(" オン ", " ON  ") : TR(" オフ ", " OFF ")));
        }
        else
        {
            Draw_DrawString(10, 50, COLOR_RED, "NWM isn't running.");
            Draw_DrawString(10, 60, COLOR_RED, "If you're currently on Test Menu,");
            Draw_DrawString(10, 70, COLOR_RED, "exit then press R+RIGHT to toggle the WiFi.");
            Draw_DrawString(10, 80, COLOR_RED, "Otherwise, simply exit and wait a few seconds.");
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A && nwmRunning)
        {
            nwmExtInit();
            NWMEXT_ControlWirelessEnabled(!wireless);
            nwmExtExit();
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_UpdateStatus(bool control)
{
    MenuItem *item = &sysconfigMenu.items[1];

    if(control)
    {
        item->title = TR("無線の接続先を選ぶ", "Control Wireless connection");
        item->method = &SysConfigMenu_ControlWifi;
    }
    else
    {
        item->title = TR("無線の強制接続をやめる", "Disable forced wireless connection");
        item->method = &SysConfigMenu_DisableForcedWifiConnection;
    }
}

static bool SysConfigMenu_ForceWifiConnection(u32 slot)
{
    char ssid[0x20 + 1] = {0};
    isConnectionForced = false;

    if(R_FAILED(acInit()))
        return false;

    acuConfig config = {0};
    ACU_CreateDefaultConfig(&config);
    ACU_SetNetworkArea(&config, 2);
    ACU_SetAllowApType(&config, 1 << slot);
    ACU_SetRequestEulaVersion(&config);

    Handle connectEvent = 0;
    svcCreateEvent(&connectEvent, RESET_ONESHOT);

    bool forcedConnection = false;
    if(R_SUCCEEDED(ACU_ConnectAsync(&config, connectEvent)))
    {
        if(R_SUCCEEDED(svcWaitSynchronization(connectEvent, -1)) && R_SUCCEEDED(ACU_GetSSID(ssid)))
            forcedConnection = true;
    }
    svcCloseHandle(connectEvent);

    if(forcedConnection)
    {
        isConnectionForced = true;
        SysConfigMenu_UpdateStatus(false);
    }
    else
        acExit();

    char infoString[80] = {0};
    u32 infoStringColor = forcedConnection ? COLOR_GREEN : COLOR_RED;
    if(forcedConnection)
        sprintf(infoString, TR("強制接続しました: %s", "Successfully forced a connection to: %s"), ssid);
    else
       sprintf(infoString, "Failed to connect to slot %d", (int)slot + 1);

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("システム設定", "System configuration menu"));
        Draw_DrawString(10, 30, infoStringColor, infoString);
        Draw_DrawString(10, 40, COLOR_WHITE, TR("B: 戻る", "B: back"));

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_B)
            break;
    }
    while(!menuShouldExit);

    return forcedConnection;
}

void SysConfigMenu_TogglePowerButton(void)
{
    u32 mcuIRQMask;

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    mcuHwcInit();
    MCUHWC_ReadRegister(0x18, (u8*)&mcuIRQMask, 4);
    mcuHwcExit();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("システム設定", "System configuration menu"));
        Draw_DrawString(10, 30, COLOR_WHITE, TR("A: 切り替え   B: 戻る", "A: switch   B: back"));

        Draw_DrawString(10, 50, COLOR_WHITE, TR("今の状態:", "Current status:"));
        Draw_DrawString(100, 50, (((mcuIRQMask & 0x00000001) == 0x00000001) ? COLOR_RED : COLOR_GREEN), (((mcuIRQMask & 0x00000001) == 0x00000001) ? TR(" 無効", " DISABLED") : TR(" 有効", " ENABLED ")));

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            mcuHwcInit();
            MCUHWC_ReadRegister(0x18, (u8*)&mcuIRQMask, 4);
            mcuIRQMask ^= 0x00000001;
            MCUHWC_WriteRegister(0x18, (u8*)&mcuIRQMask, 4);
            mcuHwcExit();
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_ControlWifi(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    u32 slot = 0;
    char ssids[3][32] = {{0}};

    Result resInit = acInit();
    for (u32 i = 0; i < 3; i++)
    {
        if (R_SUCCEEDED(ACI_LoadNetworkSetting(i)))
            ACI_GetNetworkWirelessEssidSecuritySsid(ssids[i]);
        else
            strcpy(ssids[i], TR("（未設定）", "(not configured)"));
    }
    if (R_SUCCEEDED(resInit))
        acExit();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("システム設定", "System configuration menu"));
        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, TR("A: このスロットに強制接続   B: 戻る\n\n", "A: force a connection to the slot   B: back\n\n"));

        for (u32 i = 0; i < 3; i++)
        {
            Draw_DrawString(10, posY + SPACING_Y * i, COLOR_TITLE, slot == i ? ">" : " ");
            Draw_DrawFormattedString(30, posY + SPACING_Y * i, COLOR_WHITE, "[%d] %s", (int)i + 1, ssids[i]);
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            if(SysConfigMenu_ForceWifiConnection(slot))
            {
                // Connection successfully forced, return from this menu to prevent ac handle refcount leakage.
                break;
            }

            Draw_Lock();
            Draw_ClearFramebuffer();
            Draw_FlushFramebuffer();
            Draw_Unlock();
        }
        else if(pressed & KEY_DOWN)
        {
            slot = (slot + 1) % 3;
        }
        else if(pressed & KEY_UP)
        {
            slot = (3 + slot - 1) % 3;
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_DisableForcedWifiConnection(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    acExit();
    SysConfigMenu_UpdateStatus(true);

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("システム設定", "System configuration menu"));
        Draw_DrawString(10, 30, COLOR_WHITE, TR("強制接続をやめました。\n注意: 自動接続が直らないことがあります。", "Forced connection successfully disabled.\nNote: auto-connection may remain broken."));

        u32 pressed = waitInputWithTimeout(1000);
        if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_ToggleCardIfPower(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    bool cardIfStatus = false;
    bool updatedCardIfStatus = false;

    do
    {
        Result res = FSUSER_CardSlotGetCardIFPowerStatus(&cardIfStatus);
        if (R_FAILED(res)) cardIfStatus = false;

        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("システム設定", "System configuration menu"));
        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, TR("A: 切り替え   B: 戻る\n\n", "A: toggle   B: back\n\n"));
        posY = Draw_DrawString(10, posY, COLOR_WHITE, TR("カードを抜き差しすると状態は元に戻ります。\nカードで遊ぶときは、カードを差し直してください。\n\n", "Inserting or removing a card will reset the status,\nand you'll need to reinsert the cart if you want to\nplay it.\n\n"));
        Draw_DrawString(10, posY, COLOR_WHITE, TR("今の状態:", "Current status:"));
        Draw_DrawString(100, posY, !cardIfStatus ? COLOR_RED : COLOR_GREEN, !cardIfStatus ? TR(" 無効", " DISABLED") : TR(" 有効", " ENABLED "));

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            if (!cardIfStatus)
                res = FSUSER_CardSlotPowerOn(&updatedCardIfStatus);
            else
                res = FSUSER_CardSlotPowerOff(&updatedCardIfStatus);

            if (R_SUCCEEDED(res))
                cardIfStatus = !updatedCardIfStatus;
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

// round(-40 * log10(1 - i/100)), i.e. stereo balance attenuation in 0.5 dB units, for i = 0 to 99
static const u8 stereoBalanceAttenuationLut[100] = {
     0,  0,  0,  1,  1,  1,  1,  1,  1,  2,
     2,  2,  2,  2,  3,  3,  3,  3,  3,  4,
     4,  4,  4,  5,  5,  5,  5,  5,  6,  6,
     6,  6,  7,  7,  7,  7,  8,  8,  8,  9,
     9,  9,  9, 10, 10, 10, 11, 11, 11, 12,
    12, 12, 13, 13, 13, 14, 14, 15, 15, 15,
    16, 16, 17, 17, 18, 18, 19, 19, 20, 20,
    21, 22, 22, 23, 23, 24, 25, 26, 26, 27,
    28, 29, 30, 31, 32, 33, 34, 35, 37, 38,
    40, 42, 44, 46, 49, 52, 56, 61, 68, 80,
};

static s8 SysConfigMenu_ConvertSliderPercentageToVolume(u8 pct)
{
    // Considering I found this table inside MCU fw bin (at around offset 0x1200 in the raw bin):
    // 127, 126, 125, ... 56
    // which corresponds to round(127 - 71 * (i/63)) modulo some rounding error, it is certain
    // that the MCU writes to "Page 0/Register 117: VOL/MICDET-Pin Gain" using linear interpolation
    // to map the slider position (0..63) to the raw gain values, using 127 ("reserved") as "mute".
    // Indeed, the value used in all calibration data for shutter volume is -10 dB, which maps to 56.

    // However, if you look at the definition of reg 0,117 closely, you will notice that the mapping
    // to the 7-bit value is piecewise, with half the resolution (double the slope) for values mapping
    // to -28 dB or lower, which means that the slider increases volume twice as fast below 50% position.

    // LERP like MCU does, round to nearest integer
    u8 rawPinGain = 127 - (71 * pct + 50) / 100;

    if (rawPinGain <= 90)
        return 36 - rawPinGain;
    else if (rawPinGain >= 91 && rawPinGain <= 126)
        return 126 - 2 * rawPinGain;
    else
        return -128; // mute
}

static s8 SysConfigMenu_AttenuateVolume(s8 volume, s8 balancePct)
{
    // balancePct: 0 to 100, attenuation of the channel opposite to the balance direction
    if (volume == -128 || balancePct >= 100)
        return -128; // mute

    s32 v = (s32)volume - (s32)stereoBalanceAttenuationLut[balancePct];
    return (s8)CLAMP(v, -127, 48);
}

static Result SysConfigMenu_ApplyVolumeOverride(void)
{
    // This feature repurposes the functionality used for the camera shutter sound.
    // As such, it interferes with it:
    //     - shutter volume is set to the override instead of its default 100% value
    //     - due to implementation details, having the shutter sound effect play will
    //       make this feature stop working until the volume override is reapplied by
    //       going back to this menu (or closing then opening the shell)

    // Original credit: profi200

    u8 override = lumaMcuConfig.volumeSliderOverride;
    s8 balancePct = lumaMcuConfig.volumeSliderOverrideStereoBalance;
    bool active = PctOverride_IsActive(override);

    u8 i2s1Mux;
    u8 i2s2Mux;
    Result res = SysConfigMenu_OpenCdcChkSession();
    if (R_FAILED(res))
        return res;

    res = CDCCHK_ReadRegisters2(0,  116, &i2s1Mux, 1); // used for shutter sound in TWL mode, and all GBA/DSi/3DS application
    if (R_SUCCEEDED(res)) res = CDCCHK_ReadRegisters2(100, 49, &i2s2Mux, 1); // used for shutter sound in CTR mode and CTR mode library applets

    if (active)
    {
        i2s1Mux &= ~0x80;
        i2s2Mux |=  0x20;
    }
    else
    {
        i2s1Mux |=  0x80;
        i2s2Mux &= ~0x20;
    }

    s8 i2s1VolumeL;
    s8 i2s1VolumeR;
    s8 i2s2Volume;
    if (active)
    {
        s8 volume = SysConfigMenu_ConvertSliderPercentageToVolume(PctOverride_GetPercentage(override));

        // Stereo balance can only be applied to I2S1
        i2s1VolumeL = balancePct > 0 ? SysConfigMenu_AttenuateVolume(volume, balancePct) : volume;
        i2s1VolumeR = balancePct < 0 ? SysConfigMenu_AttenuateVolume(volume, -balancePct) : volume;
        i2s2Volume = volume;
    }
    else
    {
        // Restore shutter sound volumes. This sould be sourced from cfg,
        // however the values are the same everwhere
        i2s1VolumeL =  -3; // -1.5 dB (115.7%, only used by TWL applications when taking photos)
        i2s1VolumeR =  -3;
        i2s2Volume  = -20; // -10 dB  (100%)
    }

    // Write volume overrides values before writing to the pinmux registers
    if (R_SUCCEEDED(res)) res = CDCCHK_WriteRegisters2(0, 65, &i2s1VolumeL, 1); // CDC_REG_DAC_L_VOLUME_CTRL
    if (R_SUCCEEDED(res)) res = CDCCHK_WriteRegisters2(0, 66, &i2s1VolumeR, 1); // CDC_REG_DAC_R_VOLUME_CTRL
    if (R_SUCCEEDED(res)) res = CDCCHK_WriteRegisters2(100, 123, &i2s2Volume, 1);

    if (R_SUCCEEDED(res)) res = CDCCHK_WriteRegisters2(0, 116, &i2s1Mux, 1);
    if (R_SUCCEEDED(res)) res = CDCCHK_WriteRegisters2(100, 49, &i2s2Mux, 1);

    SysConfigMenu_CloseCdcChkSession();
    return res;
}

void SysConfigMenu_RestoreSettings(void)
{
    // Called on shell open (including on system init). Only apply what is actually overridden,
    // to avoid interfering with the system's own state
    if (isServiceUsable("cdc:CHK"))
    {
        if (PctOverride_IsActive(lumaMcuConfig.volumeSliderOverride))
            SysConfigMenu_ApplyVolumeOverride();

        if (LumaConfig_GetAudioRouting() != AUDIO_ROUTING_AUTO)
        {
            // Wait for CSND to do its job
            svcSleepThread(20 * 1000 * 1000LL);
            SysConfigMenu_ApplyAudioRouting();
        }
    }

    // Entering sleep mode resets the LED master brightness
    if (PctOverride_IsActive(lumaMcuConfig.ledMasterBrightnessOverride) && isServiceUsable("mcu::HWC"))
        SysConfigMenu_ApplyLedMasterBrightnessOverride();
}

static u8 SysConfigMenu_SampleVolumeSliderPercentage(void)
{
    // Precondition: mcu::HWC session open
    // Register 0x09: volume slider position (or override set by TWL), as sent to the codec, calibrated to 0-63
    u8 sliderLevel = 63;
    MCUHWC_ReadRegister(0x09, &sliderLevel, 1);
    return (100 * sliderLevel + 31) / 63; // round to nearest integer
}

void SysConfigMenu_AdjustVolume(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    u32 pos = 0;
    Result res = 0;

    // Keep the mcu::HWC session open while this window is open (the helpers' init/exit then only update the refcount)
    bool mcuHwcOpened = R_SUCCEEDED(mcuHwcInit());

    do
    {
        u8 override = lumaMcuConfig.volumeSliderOverride;
        s8 balancePct = lumaMcuConfig.volumeSliderOverrideStereoBalance;
        u8 curPct = PctOverride_HasValue(override) ? PctOverride_GetPercentage(override) : SysConfigMenu_SampleVolumeSliderPercentage();

        char volumeStr[16];
        char balanceStr[16];
        sprintf(volumeStr, "%u%%", curPct);
        if (balancePct < 0)
            sprintf(balanceStr, "L %d%%", -balancePct);
        else if (balancePct > 0)
            sprintf(balanceStr, "R %d%%", balancePct);
        else
            strcpy(balanceStr, TR("中央", "centered"));

        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("システム設定", "System configuration menu"));
        SysConfigMenu_DrawOverrideStatus(override);
        Draw_DrawFormattedString(SYSCONFIG_COL(0), SYSCONFIG_ROW(3), COLOR_WHITE, TR("   上書きの値:      %-10s    ", "   Override value:  %-10s    "), volumeStr);
        Draw_DrawFormattedString(SYSCONFIG_COL(0), SYSCONFIG_ROW(4), COLOR_WHITE, TR("   左右のバランス:  %-10s    ", "   Stereo balance:  %-10s    "), balanceStr);
        for (u32 i = 0; i < 2; i++)
            Draw_DrawString(SYSCONFIG_COL(0), SYSCONFIG_ROW(3 + i), COLOR_TITLE, i == pos ? ">" : " ");
        Draw_DrawString(SYSCONFIG_COL(0), SYSCONFIG_ROW(6), COLOR_WHITE,
            TR("上下: 変える値を選ぶ\n"
               "左右: 1 ずつ変える\n"
               "R を押したままだと 10 ずつ変わります。\n\n"
               "A: 上書きの入／切\n"
               "X: 音量を消す／今の値にする\n"
               "Y: 左右のバランスを中央にする\n"
               "B: 戻る\n",
               "Press Up/Down to select value.\n"
               "Press Right/Left to +/- by 1 point.\n"
               "Hold R to adjust by 10 points.\n\n"
               "Press A to toggle override.\n"
               "Press X to clear or refresh volume.\n"
               "Press Y to center stereo balance.\n"
               "Press B to go back.\n")
        );
        if (R_FAILED(res))
            Draw_DrawFormattedString(SYSCONFIG_COL(0), SYSCONFIG_ROW(15), COLOR_RED, "Failed: 0x%08lX", res);
        else
            Draw_DrawString(SYSCONFIG_COL(0), SYSCONFIG_ROW(15), COLOR_WHITE, "                  ");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 kHeld = 0;
        u32 pressed = waitInputWithTimeoutEx(&kHeld, 15); // redraw at about frame rate, so that the sampled slider value is live-updated
        bool apply = false;

        if (pressed & KEY_B)
            break;

        override = lumaMcuConfig.volumeSliderOverride;
        balancePct = lumaMcuConfig.volumeSliderOverrideStereoBalance;
        if (pressed != 0)
            curPct = PctOverride_HasValue(override) ? PctOverride_GetPercentage(override) : SysConfigMenu_SampleVolumeSliderPercentage();

        if (pressed & KEY_A)
        {
            override = PctOverride_SetEnabled(override, !PctOverride_IsEnabled(override));

            // Ensure proper value is set when enabling
            if (PctOverride_IsEnabled(override))
                override = PctOverride_SetPercentage(override, curPct);

            apply = true;
        }
        else if (pressed & KEY_UP)
            pos = (2 + pos - 1) % 2;
        else if (pressed & KEY_DOWN)
            pos = (pos + 1) % 2;
        else if (pressed & KEY_Y)
        {
            balancePct = 0;
            apply = true;
        }
        else if (pressed & KEY_X)
        {
            // Unset the volume override, or refresh it if enabled
            override = PctOverride_IsEnabled(override) ? PctOverride_SetPercentage(override, SysConfigMenu_SampleVolumeSliderPercentage()) : PCT_OVERRIDE_UNSET;
            apply = true;
        }
        else
        {
            s32 delta = 0;
            s32 mult = (kHeld & KEY_R) ? 10 : 1;

            if (pressed & KEY_LEFT)
                delta = -mult;
            else if (pressed & KEY_RIGHT)
                delta = +mult;

            if (delta != 0)
            {
                if (pos == 0)
                    override = PctOverride_SetPercentage(override, (u8)CLAMP((s32)curPct + delta, 0, 100));
                else
                    balancePct = (s8)CLAMP((s32)balancePct + delta, -100, 100);

                apply = true;
            }
        }

        if (apply)
        {
            bool changed = override != lumaMcuConfig.volumeSliderOverride || balancePct != lumaMcuConfig.volumeSliderOverrideStereoBalance;
            lumaMcuConfig.volumeSliderOverride = override;
            lumaMcuConfig.volumeSliderOverrideStereoBalance = balancePct;
            res = SysConfigMenu_ApplyVolumeOverride();
            if (changed)
                LumaConfig_SaveMcuConfig();
        }
    } while(!menuShouldExit);

    if (mcuHwcOpened)
        mcuHwcExit();
}

void SysConfigMenu_ChangeScreenBrightness(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    // gsp:LCD GetLuminance is stubbed on O3DS so we have to implement it ourselves... damn it.
    // Assume top and bottom screen luminances are the same (should be; if not, we'll set them to the same values).
    u32 luminance = getCurrentLuminance(false);
    u32 minLum = getMinLuminancePreset();
    u32 maxLum = getMaxLuminancePreset();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, TR("画面の明るさ", "Screen brightness"));
        u32 posY = 30;
        posY = Draw_DrawFormattedString(
            10,
            posY,
            COLOR_WHITE,
            TR("今の明るさ: %lu（最小 %lu、最大 %lu）\n\n", "Current luminance: %lu (min. %lu, max. %lu)\n\n"),
            luminance,
            minLum,
            maxLum
        );
        posY = Draw_DrawString(10, posY, COLOR_WHITE, TR("上下: 1 ずつ   左右: 10 ずつ\n", "Up/Down: +-1   Right/Left: +-10\n"));
        posY = Draw_DrawString(10, posY, COLOR_WHITE, TR("A: 始める   B: 戻る\n\n", "A: start   B: exit\n\n"));

        posY = Draw_DrawString(10, posY, COLOR_RED, TR("警告:\n", "WARNING:\n"));
        posY = Draw_DrawString(10, posY, COLOR_WHITE, TR("  * 値は本体の補正データで制限されます。\n", "  * values are limited by the calibration data\n"));
        posY = Draw_DrawString(10, posY, COLOR_WHITE, TR("  * 終えるまで、下画面には元の画面が出ます。", "  * the bottom screen shows the previous screen\n    until you exit"));
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if (pressed & KEY_A)
            break;

        if (pressed & KEY_B)
            return;
    }
    while (!menuShouldExit);

    Draw_Lock();

    Draw_RestoreFramebuffer();
    Draw_FreeFramebufferCache();

    svcKernelSetState(0x10000, 2); // unblock gsp
    gspLcdInit(); // assume it doesn't fail. If it does, brightness won't change, anyway.

    // gsp:LCD will normalize the brightness between top/bottom screen, handle PWM, etc.

    s32 lum = (s32)luminance;

    do
    {
        u32 pressed = waitInputWithTimeout(1000);
        if (pressed & DIRECTIONAL_KEYS)
        {
            if (pressed & KEY_UP)
                lum += 1;
            else if (pressed & KEY_DOWN)
                lum -= 1;
            else if (pressed & KEY_RIGHT)
                lum += 10;
            else if (pressed & KEY_LEFT)
                lum -= 10;

            lum = lum < (s32)minLum ? (s32)minLum : lum;
            lum = lum > (s32)maxLum ? (s32)maxLum : lum;

            // We need to call gsp here because updating the active duty LUT is a bit tedious (plus, GSP has internal state).
            // This is actually SetLuminance:
            GSPLCD_SetBrightnessRaw(BIT(GSP_SCREEN_TOP) | BIT(GSP_SCREEN_BOTTOM), lum);
        }

        if (pressed & KEY_B)
            break;
    }
    while (!menuShouldExit);

    gspLcdExit();
    svcKernelSetState(0x10000, 2); // block gsp again

    if (R_FAILED(Draw_AllocateFramebufferCache(FB_BOTTOM_SIZE)))
    {
        // Shouldn't happen
        __builtin_trap();
    }
    else
        Draw_SetupFramebuffer();

    Draw_Unlock();
}
