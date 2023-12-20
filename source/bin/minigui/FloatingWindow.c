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
#define ARRAY_RIGHT_IMAGE        "assets/arrow-right.png"
#define HOME_IMAGE              "assets/home.png"
#define TOGGLE_IMAGE            "assets/toggle.png"

#define MARGIN_DOCK         0              // margin of button in dock bar
#define HEIGHT_DOCKBAR      50              // height of dock bar
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

// Customer Require Id
#define FIXED_FORMAT_REQID          (MAX_SYS_REQID + 1)
#define UNFIXED_FORMAT_REQID        (MAX_SYS_REQID + 2)

// Customer sub require Id
#define REQ_SUBMIT_STATUSBAR_ZNODE  0   // status bar send znode index to server
#define REQ_GET_TOPMOST_TITLE       1   // get topmost normal window title
#define REQ_SUBMIT_TOGGLE           2   // toggle the application
#define REQ_SHOW_PAGE               3   // show target page
#define REQ_SUBMIT_TOPMOST          4   // set the window to topmost

typedef struct tagRequestInfo
{
    int id;                     // sub request ID
    HWND hWnd;                  // the window handle of the sending window
    unsigned int iData0;
    unsigned int iData1;
    unsigned int iData2;
    unsigned int iData3;
} RequestInfo;

typedef struct tagReplyInfo
{
    int id;                     // sub request ID
    unsigned int iData0;
    unsigned int request1;
    unsigned int request2;
    unsigned int request3;
} ReplyInfo;

static int direction = DIRECTION_SHOW;        // the direction of animation
static float factor = 0;                      // DPI / 96
static float arrow_angle = 0;                 // angle of arrow button

static int dockbar_height = 0;                // height of dock bar
static int dockbar_x = 0;                     // the X coordinate of top left corner
static int dockbar_start_x = 0;               // it is only for convenience for animation
static int dockbar_start_y = 0;
static int dockbar_end_x = 0;
static int dockbar_end_y = 0;

static int dockbar_left_length = 0;           // the visible dock bar length when hidden
static int button_interval = 0;               // the interval length between dock buttons

static int dockbar_visible_time = 500;        // 500 * 10ms
static int dockbar_init_visible_time = 20;        // 50 * 10ms

static RECT button_rect[BUTTON_COUNT];               // area for BUTTON
BITMAP button_bitmap[BUTTON_COUNT];
PBITMAP p_button_bitmap[BUTTON_COUNT] = { NULL };

BITMAP arrow_right_bitmap;
PBITMAP p_arrow_right_bitmap = NULL;

static void load_button_bitmap(HDC hdc)
{
    const char *webext_dir = g_getenv("WEBKIT_WEBEXT_DIR");
    if (webext_dir == NULL) {
        webext_dir = WEBKIT_WEBEXT_DIR;
    }

    char path[PATH_MAX+1] = {0};

    sprintf(path, "%s/%s", webext_dir, ARRAY_LEFT_IMAGE);
    LoadBitmapFromFile(hdc, &button_bitmap[0], path);
    p_button_bitmap[0] = &button_bitmap[0];

    sprintf(path, "%s/%s", webext_dir, HOME_IMAGE);
    LoadBitmapFromFile(hdc, &button_bitmap[1], path);
    p_button_bitmap[1] = &button_bitmap[1];

    sprintf(path, "%s/%s", webext_dir, TOGGLE_IMAGE);
    LoadBitmapFromFile(hdc, &button_bitmap[2], path);
    p_button_bitmap[2] = &button_bitmap[2];

    /* arrow right */
    sprintf(path, "%s/%s", webext_dir, ARRAY_RIGHT_IMAGE);
    LoadBitmapFromFile(hdc, &arrow_right_bitmap, path);
    p_arrow_right_bitmap = &arrow_right_bitmap;
}

static void unload_button_bitmap()
{
    for (int i = 0; i < BUTTON_COUNT; i++) {
        if (p_button_bitmap[i]) {
            UnloadBitmap(p_button_bitmap[i]);
            p_button_bitmap[i] = NULL;
        }
    }
}

#if USE(ANIMATION)
static MGEFF_ANIMATION animation = NULL;      // handle of animation
static int dockbar_show_time = 750;
static int dockbar_hide_time = 400;

static void animated_cb(MGEFF_ANIMATION handle, HWND hWnd, int id, int *value)
{
    float factor = 0;
    if(dockbar_x != *value) {
        dockbar_x = *value;
        factor = (float)(dockbar_x - dockbar_start_x) /
            (float)(dockbar_end_x - dockbar_start_x - dockbar_left_length);
        arrow_angle = -1.0 * M_PI * factor;
        MoveWindow(hWnd,
                dockbar_x,
                dockbar_start_y,
                dockbar_end_x - dockbar_start_x,
                dockbar_height, TRUE);
    }
}

// the function which will be invoked at the end of animation
static void animated_end(MGEFF_ANIMATION handle)
{
    HWND hWnd = (HWND)mGEffAnimationGetTarget(handle);
    mGEffAnimationDelete(animation);
    animation = NULL;

    if((direction == DIRECTION_SHOW) && hWnd) {
        SetTimer(hWnd, ID_SHOW_TIMER, dockbar_visible_time);
    }
}

// create an animation and start, it is asynchronous
static void create_animation(HWND hWnd)
{
    if(animation) {
        mGEffAnimationDelete(animation);
        animation = NULL;
    }

    animation = mGEffAnimationCreate((void *)hWnd, (void *)animated_cb, 2,
            MGEFF_INT);
    if (animation) {
        int start = 0;
        int end = 0;
        int duration = 0;
        enum EffMotionType motionType = InCirc;

        start = dockbar_x;
        if(direction == DIRECTION_HIDE)
        {
            end = g_rcScr.right - dockbar_left_length;
            motionType = OutCirc;
            duration = dockbar_hide_time *
                (g_rcScr.right - dockbar_left_length - dockbar_x) /
                (g_rcScr.right - dockbar_left_length - dockbar_start_x);
        }
        else {
            end = dockbar_start_x;
            motionType = OutCirc;
            duration = dockbar_show_time * (dockbar_x - dockbar_start_x) /
                (g_rcScr.right -  dockbar_left_length- dockbar_start_x);
        }

        if(duration == 0) {
            duration = dockbar_show_time;
        }

        mGEffAnimationSetStartValue(animation, &start);
        mGEffAnimationSetEndValue(animation, &end);
        mGEffAnimationSetDuration(animation, duration);
        mGEffAnimationSetCurve(animation, motionType);
        mGEffAnimationSetFinishedCb(animation, animated_end);
        mGEffAnimationAsyncRun(animation);
    }
}
#else
static void create_animation(HWND hWnd)
{
    if(direction == DIRECTION_HIDE) {
        MoveWindow(hWnd,
                g_rcScr.right - dockbar_left_length,
                dockbar_start_y,
                dockbar_end_x - dockbar_start_x,
                dockbar_height, TRUE);
    }
    else {
        MoveWindow(hWnd,
                dockbar_start_x,
                dockbar_start_y,
                dockbar_end_x - dockbar_start_x,
                dockbar_height, TRUE);
        SetTimer(hWnd, ID_SHOW_TIMER, dockbar_visible_time);
    }
}
#endif

static void toggle_application(HWND floating_hWnd)
{
#ifdef _MGRM_PROCESSES
    REQUEST request;
    RequestInfo requestinfo;
    ReplyInfo replyInfo;

    requestinfo.id = REQ_SUBMIT_TOGGLE;
    requestinfo.hWnd = floating_hWnd;
    requestinfo.iData0 = 0;
    request.id = FIXED_FORMAT_REQID;
    request.data = (void *)&requestinfo;
    request.len_data = sizeof(requestinfo);

    memset(&replyInfo, 0, sizeof(ReplyInfo));
    ClientRequest(&request, &replyInfo, sizeof(ReplyInfo));
    if((replyInfo.id == REQ_SUBMIT_TOGGLE) && (replyInfo.iData0)) {
    }
    else {
    }
#else
    xgutils_show_windows_window();
#endif
}

static void paintDockBarIcon(HDC hdc)
{
    if (!p_button_bitmap[0]) {
        load_button_bitmap(hdc);
    }

    for (int i = 0; i < BUTTON_COUNT; i++) {
        PBITMAP p = p_button_bitmap[i];
        if (i == 0) {
            if (direction == DIRECTION_HIDE || dockbar_x == dockbar_start_x) {
                p = p_arrow_right_bitmap;
            }
            if (dockbar_x == (g_rcScr.right - dockbar_left_length)) {
                p = p_button_bitmap[i];
            }
        }
        if (p) {
            int x = i * button_interval + MARGIN_DOCK + (dockbar_height - DOCK_ICON_WIDTH) * factor / 2;
            int y = (int)((dockbar_height - DOCK_ICON_HEIGHT) * factor / 2);
            FillBoxWithBitmap(hdc, x, y, DOCK_ICON_WIDTH, DOCK_ICON_HEIGHT, p);
        }
    }
}

static LRESULT DockBarWinProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int i = 0;
    int x = 0;
    int y = 0;
    HDC hdc;

    switch (message)
    {
        case MSG_PAINT:
            hdc = BeginPaint (hWnd);
            paintDockBarIcon(hdc);
            EndPaint (hWnd, hdc);
            return 0;

        case MSG_CREATE:
            SetTimer(hWnd, ID_SHOW_TIMER, dockbar_init_visible_time);
            direction = DIRECTION_HIDE;
            dockbar_x = dockbar_start_x;
            arrow_angle = 0;
            break;

        case MSG_LBUTTONUP:
            x = LOSWORD (lParam);
            y = HISWORD (lParam);
            for(i = 0; i < BUTTON_COUNT; i++) {
                if(PtInRect(button_rect + i, x, y)) {
                    break;
                }
            }
            if(i < BUTTON_COUNT) {
                switch(i) {
                    case ID_DISPLAY_BUTTON:
                        if(direction == DIRECTION_HIDE)
                            direction = DIRECTION_SHOW;
                        else
                            direction = DIRECTION_HIDE;
                        create_animation(hWnd);
                        break;
                    case ID_HOME_BUTTON:
                        xgutils_show_runners_window();
                        break;
                    case ID_TOGGLE_BUTTON:
                        toggle_application(hWnd);
                        break;
                }
            }

            break;

        case MSG_XGUIPRO_NEW_RDR:
            direction = DIRECTION_SHOW;
            create_animation(hWnd);
            break;

        case MSG_COMMAND:
            break;

        case MSG_TIMER:
            if(wParam == ID_SHOW_TIMER)
            {
                direction = DIRECTION_HIDE;
                create_animation(hWnd);
                KillTimer(hWnd, ID_SHOW_TIMER);
            }
            break;

        case MSG_CLOSE:
            unload_button_bitmap();
            KillTimer (hWnd, ID_SHOW_TIMER);
            DestroyAllControls (hWnd);
            DestroyMainWindow (hWnd);
            PostQuitMessage (hWnd);
            return 0;
    }
    return DefaultMainWinProc (hWnd, message, wParam, lParam);
}

HWND create_floating_window(HWND hostingWnd, const char *title)
{
    (void) hostingWnd;
    MAINWINCREATE CreateInfo;
    HWND hDockBar;
    int i = 0;

    factor = (float)GetGDCapability(HDC_SCREEN, GDCAP_DPI) / 96.0;
    dockbar_height = HEIGHT_DOCKBAR * factor;

    CreateInfo.dwStyle = WS_ABSSCRPOS | WS_VISIBLE;
    CreateInfo.dwExStyle = WS_EX_WINTYPE_DOCKER | WS_EX_TROUNDCNS | WS_EX_BROUNDCNS;
    CreateInfo.spCaption = "DockBar" ;
    CreateInfo.hMenu = 0;
    CreateInfo.hCursor = GetSystemCursor (0);
    CreateInfo.hIcon = 0;
    CreateInfo.MainWindowProc = DockBarWinProc;
//    CreateInfo.lx = g_rcScr.right * (1 - 0.618);
    CreateInfo.lx = g_rcScr.right * 0.618;
    CreateInfo.ty = 0;
    CreateInfo.rx = g_rcScr.right;
    CreateInfo.by = CreateInfo.ty + dockbar_height;;

    dockbar_start_x = CreateInfo.lx;
    dockbar_start_y = CreateInfo.ty;
    dockbar_end_x = CreateInfo.rx;
    dockbar_end_y = CreateInfo.by;

    //dockbar_left_length = 2 * MARGIN_DOCK + dockbar_height * factor;
    dockbar_left_length = dockbar_height / 2 + DOCK_ICON_WIDTH / 4;
    button_interval = (dockbar_end_x - dockbar_start_x) / BUTTON_COUNT;


    CreateInfo.iBkColor = RGBA2Pixel(HDC_SCREEN, 0xFF, 0xFF, 0xFF, 0xFF);
    CreateInfo.dwAddData = 0;
    CreateInfo.hHosting = HWND_DESKTOP;

    hDockBar = CreateMainWindowEx2 (&CreateInfo, 0L, NULL, NULL, ST_PIXEL_ARGB8888,
                                MakeRGBA (140, 140, 140, 0xE0),
                                CT_ALPHAPIXEL, COLOR_BLEND_PD_SRC_OVER);

    for(i = 0; i < BUTTON_COUNT; i++) {
        button_rect[i].left = i * button_interval + MARGIN_DOCK +
            (dockbar_height - DOCK_ICON_WIDTH) * factor / 2;
        button_rect[i].top = (dockbar_height - DOCK_ICON_HEIGHT) * factor / 2;
        button_rect[i].right = button_rect[i].left + DOCK_ICON_WIDTH * factor;
        button_rect[i].bottom = button_rect[i].top + DOCK_ICON_HEIGHT * factor;
    }

    return hDockBar;
}
