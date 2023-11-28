/*
** FloatingWindow.c -- The implementation of floating window.
**
** Copyright (C) 2023 FMSoft <http://www.fmsoft.cn>
**
** Author: XueShuming
**
** This file is part of xGUI Pro, an advanced HVML renderer.
**
** xGUI Pro is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** xGUI Pro is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <math.h>

#define _DEBUG
#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <mgeff/mgeff.h>

#include "xguipro-features.h"
#include "FloatingWindow.h"
#include "utils/utils.h"

#define ARRAY_LEFT_IMAGE        "assets/arrow-left.png"
#define TOGGLE_IMAGE            "assets/toggle.png"

static const unsigned char _png_close_data[] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
  0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
  0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x30,
  0x08, 0x06, 0x00, 0x00, 0x00, 0x57, 0x02, 0xf9,
  0x87, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59,
  0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00, 0x0b,
  0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00,
  0x03, 0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c,
  0xed, 0x59, 0xcb, 0x6a, 0x14, 0x41, 0x14, 0x6d,
  0x7c, 0x2d, 0x5c, 0xfa, 0x58, 0xf9, 0xf8, 0x00,
  0x57, 0x42, 0xcf, 0xbd, 0x4e, 0x56, 0x03, 0x55,
  0x6d, 0x70, 0xe1, 0x76, 0x50, 0x74, 0xe9, 0xda,
  0x75, 0x62, 0x02, 0x06, 0x0d, 0x64, 0xb2, 0x37,
  0x61, 0x22, 0x7e, 0x82, 0xa8, 0x0b, 0x13, 0x41,
  0x05, 0xff, 0x41, 0xd1, 0x0f, 0x30, 0xba, 0x32,
  0x89, 0x68, 0xaa, 0x66, 0xe8, 0x71, 0x1e, 0x25,
  0xb7, 0x70, 0x66, 0x74, 0xa6, 0x93, 0xae, 0xea,
  0xae, 0xee, 0x1e, 0x61, 0x2e, 0x14, 0x0c, 0x74,
  0xd1, 0x73, 0x4e, 0xd5, 0x7d, 0x9c, 0x7b, 0xdb,
  0xf3, 0xa6, 0x36, 0xb5, 0xa9, 0xa5, 0x36, 0x55,
  0xad, 0x1e, 0x6d, 0x72, 0x98, 0x91, 0x0c, 0x17,
  0x24, 0xc3, 0x67, 0x92, 0xc3, 0x27, 0xc9, 0xe1,
  0xbb, 0xe0, 0xf8, 0x8b, 0x16, 0xfd, 0x96, 0x1c,
  0x3f, 0xea, 0x67, 0x0c, 0x17, 0x1a, 0x01, 0x96,
  0xd5, 0x92, 0x77, 0xc4, 0x2b, 0xda, 0x9a, 0xb3,
  0xa5, 0x0b, 0x82, 0xe3, 0xaa, 0xe0, 0xf0, 0x55,
  0x72, 0x54, 0x36, 0x4b, 0x70, 0xf8, 0x22, 0x18,
  0xd6, 0x1a, 0x15, 0x3c, 0x9f, 0x3b, 0xf0, 0xfd,
  0x6b, 0x97, 0xcf, 0x0a, 0x06, 0x8f, 0x05, 0xc3,
  0x96, 0x2d, 0xf0, 0x31, 0x22, 0x0c, 0x5b, 0x82,
  0x63, 0x7d, 0xbf, 0xe2, 0x9f, 0xc9, 0x05, 0xbc,
  0x08, 0xe0, 0x96, 0x64, 0xb0, 0x97, 0x16, 0xf8,
  0xd8, 0x62, 0xb8, 0x2b, 0xd8, 0x95, 0x9b, 0x99,
  0x01, 0x57, 0xbe, 0x7f, 0x5c, 0x70, 0x7c, 0xe2,
  0x1c, 0x38, 0x1f, 0x73, 0xad, 0x0d, 0xfa, 0x2f,
  0xb7, 0xe0, 0xaf, 0xfb, 0x27, 0x05, 0xc7, 0x57,
  0x59, 0x83, 0x97, 0x43, 0x12, 0x5b, 0xf4, 0x9f,
  0x2e, 0x4f, 0x3e, 0x37, 0xf0, 0x72, 0xe8, 0x52,
  0x6f, 0x55, 0xf5, 0xd2, 0x89, 0xd4, 0x04, 0xf2,
  0x70, 0x1b, 0x79, 0xe0, 0x4d, 0x60, 0x3d, 0x25,
  0x78, 0xb8, 0x5d, 0x14, 0x78, 0x39, 0x20, 0x51,
  0xba, 0x91, 0x08, 0xfc, 0x4f, 0x0e, 0xa7, 0x25,
  0xc7, 0x9d, 0xa2, 0x09, 0x48, 0x06, 0x7b, 0x89,
  0x52, 0x2c, 0xe5, 0xf9, 0xc2, 0xc1, 0xf3, 0x41,
  0x3c, 0xac, 0x5b, 0x81, 0xa7, 0xea, 0x68, 0x55,
  0xa4, 0x82, 0xb2, 0x6a, 0xde, 0xbd, 0x63, 0x0c,
  0x48, 0xef, 0x0d, 0xca, 0xe6, 0x6e, 0xc4, 0xb0,
  0xd5, 0x0c, 0xfc, 0x8b, 0xe6, 0xa7, 0xcf, 0x71,
  0xd5, 0x06, 0x7c, 0xfb, 0xf5, 0xa6, 0x52, 0xdd,
  0xae, 0x0a, 0x6b, 0x4b, 0xb1, 0xfb, 0xc3, 0x87,
  0xf7, 0x94, 0xea, 0x74, 0x54, 0xfb, 0xdd, 0x1b,
  0x25, 0x67, 0x67, 0x6c, 0x48, 0xd4, 0x8c, 0xc0,
  0x93, 0xc8, 0x22, 0x9d, 0x62, 0x05, 0xbe, 0x6f,
  0x31, 0x24, 0xfa, 0xe0, 0xfb, 0xd6, 0xb6, 0x20,
  0x41, 0x7a, 0x8b, 0x44, 0x63, 0x2c, 0x01, 0xad,
  0x2a, 0x2d, 0x5c, 0xe1, 0x6f, 0x40, 0xda, 0x3a,
  0x1d, 0x15, 0x2e, 0x2f, 0x8e, 0x83, 0x5f, 0x5e,
  0x8c, 0xdc, 0xdb, 0xb4, 0x70, 0xbd, 0x06, 0xf3,
  0x31, 0x96, 0x80, 0x96, 0xc4, 0x16, 0x01, 0x16,
  0x3e, 0x98, 0x1f, 0x07, 0x36, 0x72, 0x13, 0xa3,
  0x27, 0x3f, 0xd8, 0xb3, 0x72, 0xdf, 0x2e, 0x98,
  0x03, 0x98, 0x37, 0xf1, 0xff, 0xe7, 0x56, 0x2f,
  0x8d, 0x21, 0xe1, 0x0c, 0x3c, 0xa7, 0x05, 0x4f,
  0xe3, 0x6f, 0x80, 0x1a, 0x0f, 0xeb, 0x17, 0x1f,
  0x40, 0xa2, 0xd7, 0xd5, 0x60, 0xdd, 0x80, 0x47,
  0xaa, 0x09, 0x1f, 0x0c, 0x5c, 0x28, 0xb9, 0x54,
  0x8e, 0x24, 0xe1, 0x0a, 0x3c, 0xd7, 0x6b, 0x27,
  0xde, 0x85, 0x52, 0x36, 0x29, 0x9a, 0xc4, 0xe8,
  0xa9, 0xeb, 0xdb, 0xe8, 0xa5, 0x05, 0xaf, 0x04,
  0xc3, 0x30, 0x7b, 0x02, 0xe4, 0xf3, 0x91, 0x04,
  0xcc, 0xea, 0x84, 0x4c, 0x4b, 0x20, 0x95, 0x0b,
  0x45, 0x05, 0x6c, 0x44, 0x60, 0x67, 0xea, 0x42,
  0x32, 0x69, 0x10, 0x47, 0x81, 0xef, 0xf5, 0xa2,
  0x83, 0x38, 0x29, 0x09, 0x66, 0x10, 0xc4, 0x89,
  0xd2, 0xe8, 0x21, 0xa9, 0xd2, 0xa4, 0x4e, 0x48,
  0xa7, 0x69, 0xd4, 0xb6, 0x90, 0x19, 0xe4, 0x79,
  0x67, 0x24, 0x18, 0xcc, 0xc5, 0x12, 0xa0, 0xa1,
  0xd3, 0xc4, 0x4a, 0x09, 0x5e, 0x02, 0x33, 0x31,
  0xc7, 0x70, 0x3b, 0xb1, 0x98, 0x3b, 0x24, 0x55,
  0x8e, 0xde, 0x44, 0x9b, 0xc4, 0xdc, 0x55, 0x33,
  0x59, 0x2d, 0x18, 0x7e, 0x36, 0x9e, 0xe6, 0x91,
  0x74, 0x4d, 0x24, 0xa7, 0x0d, 0xf2, 0x7c, 0x9f,
  0x84, 0x0d, 0x78, 0xa9, 0x09, 0xc0, 0x8a, 0x11,
  0xf8, 0x09, 0x6d, 0x68, 0xc2, 0x06, 0x2b, 0x9f,
  0x33, 0x26, 0xa0, 0x6f, 0x81, 0x63, 0xdd, 0x2a,
  0xc0, 0x32, 0x5d, 0xf0, 0xc8, 0xb3, 0xb5, 0x1f,
  0xb3, 0xe5, 0x53, 0x93, 0xd1, 0xd4, 0xe3, 0x6e,
  0xe2, 0xb9, 0x29, 0xcd, 0x2a, 0x8b, 0x26, 0x20,
  0x18, 0x56, 0x13, 0x81, 0x1f, 0x90, 0xe0, 0xb0,
  0x51, 0xa0, 0xeb, 0xac, 0x79, 0x69, 0x8d, 0xfa,
  0x50, 0xc1, 0xe1, 0x45, 0xfe, 0x27, 0x0f, 0x9b,
  0xaa, 0x52, 0x39, 0x96, 0x9a, 0xc0, 0x70, 0xb8,
  0x0b, 0x5b, 0x39, 0xba, 0xcd, 0x4b, 0x67, 0xc3,
  0xdd, 0x91, 0x21, 0x6f, 0x0e, 0x99, 0x09, 0xd6,
  0x9c, 0x9d, 0x7c, 0x94, 0xd1, 0xac, 0x32, 0x93,
  0xec, 0xc4, 0xf0, 0x5b, 0xea, 0x80, 0xb5, 0x9a,
  0x9b, 0x32, 0x5c, 0xa7, 0x02, 0xe3, 0xc0, 0x5d,
  0x42, 0xca, 0xf3, 0x94, 0xb6, 0xbd, 0xbc, 0x8d,
  0xaa, 0x23, 0xc9, 0x0e, 0x63, 0xed, 0xf4, 0x2f,
  0xf0, 0x6d, 0x92, 0x07, 0xd6, 0x15, 0x36, 0x0b,
  0x23, 0x91, 0x45, 0x43, 0x27, 0x9a, 0xdb, 0x90,
  0x66, 0xa7, 0xc6, 0x83, 0x3a, 0x3b, 0xfd, 0x01,
  0x8f, 0x24, 0x89, 0xee, 0xf2, 0xe0, 0xfd, 0x9f,
  0x67, 0x73, 0xa4, 0x2a, 0x27, 0xe2, 0x33, 0xeb,
  0xd4, 0xa6, 0xe6, 0xfd, 0xff, 0xf6, 0x1b, 0x70,
  0x0b, 0x24, 0xc2, 0xb3, 0x56, 0xa0, 0x02, 0x00,
  0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
  0x42, 0x60, 0x82
};

#define WND_WIDTH     48
#define WND_HEIGHT    48

#if USE(ANIMATION)
static void anim_cb(MGEFF_ANIMATION handle, HWND hWnd, int id, RECT *rc)
{
    (void) handle;
    MoveWindow(hWnd, rc->left, rc->top, RECTWP(rc), RECTHP(rc), FALSE);
}

static void move_window_with_transition(HWND hwnd, int x,
        int y, int w, int h)
{
    MGEFF_ANIMATION animation;
    animation = mGEffAnimationCreate((void *)hwnd, (void *)anim_cb, 1,
            MGEFF_RECT);
    if (animation) {
        RECT start_rc, end_rc;
        GetWindowRect(window->hwnd, &start_rc);
        end_rc.left = x;
        end_rc.top = y;
        end_rc.right = end_rc.left + w;
        end_rc.bottom = end_rc.top + h;

        mGEffAnimationSetStartValue(animation, &start_rc);
        mGEffAnimationSetEndValue(animation, &end_rc);
        mGEffAnimationSetDuration(animation, 200);
        mGEffAnimationSetProperty(animation, MGEFF_PROP_LOOPCOUNT, 1);
        mGEffAnimationSetCurve(animation, OutExpo);
        mGEffAnimationSyncRun(animation);

        mGEffAnimationDelete(animation);
    }
}

static void floating_window_layout(HWND hwnd, int x, int y, int w, int h)
{
    move_window_with_transition(hwnd, x, y, w, h);
}
#else
static void floating_window_layout(HWND hwnd, int x, int y, int w, int h)
{
    MoveWindow(hwnd, x, y, w, h, false);
}
#endif /* USE(ANIMATION) */

static void show_full_window(HWND hWnd)
{
    RECT rcHosting;
    GetWindowRect(GetHosting(hWnd), &rcHosting);

    int x = rcHosting.left + RECTW(rcHosting) - WND_WIDTH;
    int y = rcHosting.top;
    floating_window_layout(hWnd, x, y, WND_WIDTH, WND_HEIGHT);
}

static void show_half_window(HWND hWnd)
{
    RECT rcHosting;
    GetWindowRect(GetHosting(hWnd), &rcHosting);

    int x = rcHosting.left + RECTW(rcHosting) - WND_WIDTH / 2;
    int y = rcHosting.top;
    floating_window_layout(hWnd, x, y, WND_WIDTH / 2, WND_HEIGHT);
}

static void on_paint(HWND hWnd, HDC hdc)
{
    RECT client_rc;
    GetClientRect(hWnd, &client_rc);

    SetBrushColor(hdc, DWORD2Pixel(hdc, 0x00000000));
    FillBox(hdc, 0, 0, client_rc.right, client_rc.bottom);

    MG_RWops* area;
    area = MGUI_RWFromMem((void *)_png_close_data, sizeof(_png_close_data));
    if (area) {
        PaintImageEx(hdc, 0, 0, area, "png");
        MGUI_FreeRW(area);
    }

#if 0
    SetBrushColor(hdc, PIXEL_darkred);
    int sx = client_rc.right / 2;
    int sy = client_rc.bottom / 2;
    FillCircle (hdc, sx, sy, sx);
#endif
}

static LRESULT
FloatingWinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case MSG_CREATE:
            show_half_window(hWnd);
            break;

        case MSG_TIMER:
            KillTimer(hWnd, 100);
            show_half_window(hWnd);
            break;

        case MSG_LBUTTONDOWN:
            break;

        case MSG_LBUTTONUP:
            {
                xgutils_show_runners_window();
            }
            break;

        case MSG_XGUIPRO_NEW_RDR:
            SetTimer(hWnd, 100, 500);
            show_full_window(hWnd);
            break;

        case MSG_PAINT: {
            HDC hdc = BeginPaint(hWnd);
            on_paint(hWnd, hdc);
            EndPaint(hWnd, hdc);
            return 0;
        }

        case MSG_CLOSE:
            DestroyMainWindow(hWnd);
            MainWindowCleanup(hWnd);
            return 0;

        case MSG_DESTROY:
            break;

        default:
            break;
    }

    return DefaultMainWinProc(hWnd, message, wParam, lParam);
}

HWND create_floating_window(HWND hostingWnd, const char *title)
{
    HWND toolWnd;
    MAINWINCREATE CreateInfo;

    RECT rcHosting;
    GetWindowRect(hostingWnd, &rcHosting);
    CreateInfo.dwStyle = WS_VISIBLE;
    CreateInfo.dwExStyle = WS_EX_WINTYPE_TOOLTIP | WS_EX_TOOLWINDOW;
    CreateInfo.spCaption = title ? title : "";
    CreateInfo.hMenu = 0;
    CreateInfo.hCursor = GetSystemCursor(0);
    CreateInfo.hIcon = 0;
    CreateInfo.MainWindowProc = FloatingWinProc;
    CreateInfo.lx = rcHosting.left + RECTW(rcHosting) - WND_WIDTH;
    CreateInfo.ty = rcHosting.top;
    CreateInfo.rx = rcHosting.right;
    CreateInfo.by = rcHosting.top + WND_HEIGHT;
    CreateInfo.iBkColor = COLOR_black;
    CreateInfo.dwAddData = 0;
    CreateInfo.hHosting = hostingWnd;

    toolWnd = CreateMainWindowEx2(&CreateInfo, 0, NULL, NULL,
            ST_PIXEL_ARGB8888,
            MakeRGBA(0, 0, 0, 0xC0),
            CT_ALPHAPIXEL, COLOR_BLEND_PD_SRC_OVER);

    if (toolWnd != HWND_INVALID)
        ShowWindow(toolWnd, SW_SHOWNORMAL);

    return toolWnd;
}

#define MARGIN_DOCK         15              // margin of button in dock bar
#define HEIGHT_DOCKBAR      60              // height of dock bar
#define DOCK_ICON_WIDTH     42              // the width of icon in dock bar
#define DOCK_ICON_HEIGHT    42              // the height of icon in dock bar

#define DIRECTION_SHOW      0               // show the bar
#define DIRECTION_HIDE      1               // hide the bar

// control ID in dock bar
 #define BUTTON_COUNT        3               // the number of button on the dock bar
 #define ID_DISPLAY_BUTTON   0               // show and hide button
 #define ID_HOME_BUTTON      1               // home button
 #define ID_TOGGLE_BUTTON    2               // toggle apps

// timer
#define ID_TIMER            100             // for time display
#define ID_SHOW_TIMER       101             // for display status and dock bar

static int m_DockBar_Height = 0;                // height of dock bar
static int m_DockBar_X = 0;                     // the X coordinate of top left corner
static MGEFF_ANIMATION m_animation = NULL;      // handle of animation
static int m_direction = DIRECTION_SHOW;        // the direction of animation
static float m_factor = 0;                      // DPI / 96
static RECT m_rect[BUTTON_COUNT];               // area for BUTTON
static float m_Arrow_angle = 0;                 // angle of arrow button

static int m_DockBar_Start_x = 0;               // it is only for convenience for animation
static int m_DockBar_Start_y = 0;
static int m_DockBar_End_x = 0;
static int m_DockBar_End_y = 0;

static int m_DockBar_Left_Length = 0;           // the visible dock bar length when hidden
static int m_Button_Interval = 0;               // the interval length between dock buttons

static int m_dockbar_visible_time = 400;        // 400 * 10ms
static int m_dockbar_show_time = 750;           // 500 ms
static int m_dockbar_hide_time = 400;           // 500 ms

static void animated_cb(MGEFF_ANIMATION handle, HWND hWnd, int id, int *value)
{
    float factor = 0;
    if(m_DockBar_X != *value) {
        m_DockBar_X = *value;
        factor = (float)(m_DockBar_X - m_DockBar_Start_x) /
            (float)(m_DockBar_End_x - m_DockBar_Start_x - m_DockBar_Left_Length);
        m_Arrow_angle = -1.0 * M_PI * factor;
        MoveWindow(hWnd,
                m_DockBar_X,
                m_DockBar_Start_y,
                m_DockBar_End_x - m_DockBar_Start_x,
                m_DockBar_Height, TRUE);
    }
}

// the function which will be invoked at the end of animation
static void animated_end(MGEFF_ANIMATION handle)
{
    HWND hWnd = (HWND)mGEffAnimationGetTarget(handle);
    mGEffAnimationDelete(m_animation);
    m_animation = NULL;

    if((m_direction == DIRECTION_SHOW) && hWnd) {
        SetTimer(hWnd, ID_SHOW_TIMER, m_dockbar_visible_time);
    }
}

// create an animation and start, it is asynchronous
static void create_animation(HWND hWnd)
{
    if(m_animation) {
        mGEffAnimationDelete(m_animation);
        m_animation = NULL;
    }

    m_animation = mGEffAnimationCreate((void *)hWnd, (void *)animated_cb, 1,
            MGEFF_INT);
    if (m_animation) {
        int start = 0;
        int end = 0;
        int duration = 0;
        enum EffMotionType motionType = InCirc;

        start = m_DockBar_X;
        if(m_direction == DIRECTION_HIDE)
        {
            end = g_rcScr.right - m_DockBar_Left_Length;
            motionType = OutCirc;
            duration = m_dockbar_hide_time *
                (g_rcScr.right - m_DockBar_Left_Length - m_DockBar_X) /
                (g_rcScr.right - m_DockBar_Left_Length - m_DockBar_Start_x);
        }
        else {
            end = m_DockBar_Start_x;
            motionType = OutCirc;
            duration = m_dockbar_show_time * (m_DockBar_X - m_DockBar_Start_x) /
                (g_rcScr.right -  m_DockBar_Left_Length- m_DockBar_Start_x);
        }

        if(duration == 0) {
            duration = m_dockbar_show_time;
        }

        mGEffAnimationSetStartValue(m_animation, &start);
        mGEffAnimationSetEndValue(m_animation, &end);
        mGEffAnimationSetDuration(m_animation, duration);
        mGEffAnimationSetCurve(m_animation, motionType);
        mGEffAnimationSetFinishedCb(m_animation, animated_end);
        mGEffAnimationAsyncRun(m_animation);
    }
}

static LRESULT DockBarWinProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefaultMainWinProc (hWnd, message, wParam, lParam);
}

HWND create_dock_bar (void)
{
    MAINWINCREATE CreateInfo;
    HWND hDockBar;
    int i = 0;

    m_factor = (float)GetGDCapability(HDC_SCREEN, GDCAP_DPI) / 96.0;
    m_DockBar_Height = HEIGHT_DOCKBAR * m_factor;

    CreateInfo.dwStyle = WS_ABSSCRPOS | WS_VISIBLE;
    CreateInfo.dwExStyle = WS_EX_WINTYPE_DOCKER | WS_EX_TROUNDCNS | WS_EX_BROUNDCNS;
    CreateInfo.spCaption = "DockBar" ;
    CreateInfo.hMenu = 0;
    CreateInfo.hCursor = GetSystemCursor (0);
    CreateInfo.hIcon = 0;
    CreateInfo.MainWindowProc = DockBarWinProc;
    CreateInfo.lx = g_rcScr.right * (1 - 0.618);
    CreateInfo.ty = g_rcScr.bottom - m_DockBar_Height;
    CreateInfo.rx = g_rcScr.right;
    CreateInfo.by = g_rcScr.bottom;

    m_DockBar_Start_x = CreateInfo.lx;
    m_DockBar_Start_y = CreateInfo.ty;
    m_DockBar_End_x = CreateInfo.rx;
    m_DockBar_End_y = CreateInfo.by;
    m_DockBar_Left_Length = 2 * MARGIN_DOCK + m_DockBar_Height * m_factor;
    m_Button_Interval = (m_DockBar_End_x - m_DockBar_Start_x) / BUTTON_COUNT;

    CreateInfo.iBkColor = RGBA2Pixel(HDC_SCREEN, 0xFF, 0xFF, 0xFF, 0xFF);
    CreateInfo.dwAddData = 0;
    CreateInfo.hHosting = HWND_DESKTOP;

    hDockBar = CreateMainWindowEx2 (&CreateInfo, 0L, NULL, NULL, ST_PIXEL_ARGB8888,
                                MakeRGBA (140, 140, 140, 0xE0),
                                CT_ALPHAPIXEL, 0xFF);

    for(i = 0; i < BUTTON_COUNT; i++) {
        m_rect[i].left = i * m_Button_Interval + MARGIN_DOCK +
            (m_DockBar_Height - DOCK_ICON_WIDTH) * m_factor / 2;
        m_rect[i].top = (m_DockBar_Height - DOCK_ICON_HEIGHT) * m_factor / 2;
        m_rect[i].right = m_rect[i].left + DOCK_ICON_WIDTH * m_factor;
        m_rect[i].bottom = m_rect[i].top + DOCK_ICON_HEIGHT * m_factor;
    }

    return hDockBar;
}
