/*
** PopupTipWindow.c -- The implementation of popup tip window.
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

#include <stdio.h>
#include <string.h>
#include <glib.h>

#define _DEBUG
#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <mgeff/mgeff.h>

#include <purc/purc.h>

#include "xguipro-features.h"
#include "PopupTipWindow.h"

#include "sd/sd.h"

#define POPUP_TIP_LIVE_TIMER_ID     101
#define POPUP_TIP_LIVE_TIME         5       // second

static const char *cn = "发现来自 %s : %d 的新渲染器";
static const char *zh = "發現來自 %s : %d 的新渲染器";
static const char *en = "Found a new renderer from %s : %d";

static const char *get_string_res()
{
    char *str = setlocale(LC_ALL, "");
    if (str) {
        if (strstr(str, "CN")) {
            return cn;
        }
        else if (strstr(str, "zh")) {
            return zh;
        }
        else {
            return en;
        }
    }
    else {
        return en;
    }
}

static PLOGFONT create_font(int size)
{
    return CreateLogFontEx("ttf", "helvetica", "UTF-8",
                FONT_WEIGHT_REGULAR,
                FONT_SLANT_ROMAN,
                FONT_FLIP_NONE,
                FONT_OTHER_NONE,
                FONT_DECORATE_NONE, FONT_RENDER_SUBPIXEL,
                size, 0);
}

static void destroy_font(PLOGFONT lf)
{
    if (lf) {
        DestroyLogFont(lf);
    }
}

static void on_paint(HWND hWnd, HDC hdc)
{
    RECT client_rc;
    GetClientRect(hWnd, &client_rc);

    DWORD dw = GetWindowAdditionalData(hWnd);
    struct sd_remote_service *srv = (struct sd_remote_service *)dw;

    const char *s_res = get_string_res();
    size_t nr_buf = strlen(s_res) + strlen(srv->host) + 10;
    char buf[nr_buf];

    sprintf(buf, s_res, srv->host, srv->port);

    dw = GetWindowAdditionalData2(hWnd);
    if (dw) {
        PLOGFONT lf = (PLOGFONT) dw;
        SelectFont(hdc, lf);
    }
    SetTextColor(hdc, DWORD2Pixel(hdc, 0xFFFFFFFF));
    SetBkMode(hdc, BM_TRANSPARENT);
    DrawText(hdc, buf, -1, &client_rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
}

static LRESULT
popup_tip_wndproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case MSG_CREATE:
            {
                SetTimer(hWnd, POPUP_TIP_LIVE_TIMER_ID, 100);
            }
            break;

        case MSG_TIMER:
            {
                DWORD dw = GetWindowAdditionalData(hWnd);
                struct sd_remote_service *srv = (struct sd_remote_service *)dw;
                time_t t = purc_get_monotoic_time();
                if (t - srv->ct >= POPUP_TIP_LIVE_TIME) {
                    KillTimer(hWnd, 101);
                    SendNotifyMessage(hWnd, MSG_CLOSE, 0, 0);
                }
            }
            break;

        case MSG_CLOSE:
            {
                DWORD dw = GetWindowAdditionalData2(hWnd);
                if (dw) {
                    destroy_font((PLOGFONT)dw);
                }
                DestroyMainWindow(hWnd);
                MainWindowCleanup(hWnd);
            }
            return 0;

        case MSG_DESTROY:
            break;

        case MSG_PAINT: {
            HDC hdc = BeginPaint(hWnd);
            on_paint(hWnd, hdc);
            EndPaint(hWnd, hdc);
            return 0;
        }

        default:
            break;
    }

    return DefaultMainWinProc(hWnd, message, wParam, lParam);
}

RECT xphbd_get_default_window_rect(void);

HWND create_popup_tip_window(HWND hosting_wnd, struct sd_remote_service *srv)
{
    HWND tip_wnd;
    MAINWINCREATE CreateInfo;

    RECT rc = xphbd_get_default_window_rect();

    /* TODO calc popup height */
    int height = RECTH(rc) / 6.6;

    CreateInfo.dwStyle = WS_VISIBLE;
    CreateInfo.dwExStyle = WS_EX_WINTYPE_HIGHER | WS_EX_TOOLWINDOW;
    CreateInfo.spCaption = "";
    CreateInfo.hMenu = 0;
    CreateInfo.hCursor = GetSystemCursor(0);
    CreateInfo.hIcon = 0;
    CreateInfo.MainWindowProc = popup_tip_wndproc;
    CreateInfo.lx = rc.left;
    CreateInfo.ty = rc.top;
    CreateInfo.rx = rc.right;
    CreateInfo.by = rc.top + height;
    CreateInfo.iBkColor = COLOR_black;
    CreateInfo.dwAddData = 0;
    CreateInfo.hHosting = hosting_wnd;

    tip_wnd = CreateMainWindowEx2(&CreateInfo, 0, NULL, NULL,
            ST_PIXEL_ARGB8888,
            MakeRGBA(0x00, 0x00, 0x00, 0xC0),
            CT_ALPHAPIXEL, COLOR_BLEND_PD_SRC_OVER);

    if (tip_wnd != HWND_INVALID) {
        PLOGFONT lf = create_font( height / 4);
        SetWindowAdditionalData2(tip_wnd, (DWORD)lf);

        SetWindowAdditionalData(tip_wnd, (DWORD)srv);
        ShowWindow(tip_wnd, SW_SHOWNORMAL);
    }

    return tip_wnd;
}

