/*
** FloatingToolWindow.c -- The implementation of floating tool window.
**
** Copyright (C) 2023 FMSoft <http://www.fmsoft.cn>
**
** Author: Vincent Wei <https://github.com/VincentWei>
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

#include "xguipro-features.h"
#include "FloatingToolWindow.h"

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

#define MINIMIZED_WIDTH     48
#define MINIMIZED_HEIGHT    48

static void animated_cb(MGEFF_ANIMATION handle, HWND hWnd, int id, POINT *pt)
{
    RECT rcWnd;
    GetWindowRect(hWnd, &rcWnd);

    MoveWindow(hWnd, pt->x, pt->y, RECTW(rcWnd), RECTH(rcWnd), FALSE);
}

static void minimize_tool_window(HWND hWnd)
{
    MGEFF_ANIMATION animation;
    RECT rcWnd;
    GetWindowRect(hWnd, &rcWnd);

    animation = mGEffAnimationCreate((void *)hWnd, (void *)animated_cb, 1,
            MGEFF_POINT);
    if (animation) {
        POINT start_pt, end_pt;

        start_pt.x = rcWnd.left;
        start_pt.y = rcWnd.top;
        end_pt.x = rcWnd.left;
        end_pt.y = rcWnd.bottom;
        mGEffAnimationSetStartValue(animation, &start_pt);
        mGEffAnimationSetEndValue(animation, &end_pt);
        mGEffAnimationSetDuration(animation, 200);
        mGEffAnimationSetProperty(animation, MGEFF_PROP_LOOPCOUNT, 1);
        mGEffAnimationSetCurve(animation, OutExpo);
        mGEffAnimationSyncRun(animation);

        IncludeWindowStyle(hWnd, WS_MINIMIZE);
        MoveWindow(hWnd, rcWnd.left, rcWnd.bottom,
                RECTW(rcWnd), MINIMIZED_HEIGHT, TRUE);

        start_pt.x = rcWnd.left;
        start_pt.y = rcWnd.bottom;
        end_pt.x = rcWnd.left;
        end_pt.y = rcWnd.bottom - MINIMIZED_HEIGHT;

        mGEffAnimationSetStartValue(animation, &start_pt);
        mGEffAnimationSetEndValue(animation, &end_pt);
        mGEffAnimationSetDuration(animation, 100);
        mGEffAnimationSetProperty(animation, MGEFF_PROP_LOOPCOUNT, 1);
        mGEffAnimationSetCurve(animation, InExpo);
        mGEffAnimationSyncRun(animation);

        mGEffAnimationDelete(animation);
    }
    else {
        MoveWindow(hWnd,
                rcWnd.left,
                rcWnd.bottom - MINIMIZED_HEIGHT,
                rcWnd.right, MINIMIZED_HEIGHT, FALSE);
        IncludeWindowStyle(hWnd, WS_MINIMIZE);
        UpdateWindow(hWnd, TRUE);
    }
}

#define MARGIN_PADDING  10

#if 0
static void get_title_rc(HWND hWnd, RECT *title_rc)
{
    RECT client_rc;
    int font_height = GetSysFontHeight(SYSLOGFONT_CAPTION);

    GetClientRect(hWnd, &client_rc);
    int center_v = (client_rc.bottom - font_height) / 2;

    SetRect(title_rc, MARGIN_PADDING,
            center_v - font_height / 2 - MARGIN_PADDING / 2,
            client_rc.right - MARGIN_PADDING * 2,
            center_v + font_height / 2 + MARGIN_PADDING / 2);
}

static void get_bar_rc(HWND hWnd, const RECT *title_rc, RECT *bar_rc)
{
    SetRect(bar_rc, title_rc->left, title_rc->bottom,
            title_rc->right, title_rc->bottom + 3);
}
#endif

#define HVML_LOGO_FILE      "assets/hvml-light-h240.png"
#define HVML_LOGO_WIDTH     290
#define HVML_LOGO_HEIGHT    240

static BITMAP bmp_hvml_logo;
static HDC alpha_mask_dc = HDC_INVALID;

static BOOL load_hvml_logo(HDC hdc)
{
    if (alpha_mask_dc != HDC_INVALID)
        return TRUE;

    const char *webext_dir = g_getenv("WEBKIT_WEBEXT_DIR");
    if (webext_dir == NULL) {
        webext_dir = WEBKIT_WEBEXT_DIR;
    }

    gchar *path = g_strdup_printf("%s/%s", webext_dir, HVML_LOGO_FILE);
    if (path) {
        alpha_mask_dc = CreateMemDC(HVML_LOGO_WIDTH, HVML_LOGO_HEIGHT, 32,
                MEMDC_FLAG_HWSURFACE | MEMDC_FLAG_SRCALPHA,
                0x00FF0000, 0x0000FF00, 0x0000FF, 0xFF000000);
        if (alpha_mask_dc == HDC_INVALID) {
            goto failed;
        }

        if (LoadBitmapFromFile(hdc, &bmp_hvml_logo, path))
            goto failed;

        g_free(path);
    }
    else {
        goto failed;
    }

    return TRUE;

failed:
    if (alpha_mask_dc != HDC_INVALID) {
        DeleteMemDC(alpha_mask_dc);
        alpha_mask_dc = HDC_INVALID;
    }

    if (path)
        g_free(path);
    return FALSE;
}

static void get_logo_rect(HWND hWnd, const RECT *client_rc, RECT *logo_rc)
{
    logo_rc->left = (client_rc->right - HVML_LOGO_WIDTH) / 2;
    logo_rc->top = (client_rc->bottom - HVML_LOGO_HEIGHT) / 2;
    logo_rc->right = logo_rc->left + HVML_LOGO_WIDTH;
    logo_rc->bottom = logo_rc->top + HVML_LOGO_HEIGHT;
}

static void get_minimized_bar_rc(HWND hWnd, const RECT *client_rc, RECT *bar_rc)
{
    SetRect(bar_rc, MARGIN_PADDING, client_rc->bottom - 3,
            client_rc->right - MARGIN_PADDING, client_rc->bottom);
}

static void on_paint(HWND hWnd, HDC hdc)
{
    if (GetWindowStyle(hWnd) & WS_MINIMIZE) {
        MG_RWops* area;

        RECT client_rc;
        GetClientRect(hWnd, &client_rc);

        SetBrushColor(hdc, DWORD2Pixel(hdc, 0x00000000));
        FillBox(hdc, 0, 0, client_rc.right, client_rc.bottom);

        unsigned percent = (unsigned)GetWindowAdditionalData(hWnd);
        if (percent <= 100) {
            RECT bar_rc;
            get_minimized_bar_rc(hWnd, &client_rc, &bar_rc);
            SetPenColor(hdc, DWORD2Pixel(hdc, 0xFF2E3436));
            SetBrushColor(hdc, DWORD2Pixel(hdc, 0xFF2E3436));
            Rectangle(hdc, bar_rc.left, bar_rc.top, bar_rc.right, bar_rc.bottom);

            FillBox(hdc, bar_rc.left, bar_rc.top, RECTW(bar_rc) * percent / 100,
                    RECTH(bar_rc));
        }

        area = MGUI_RWFromMem((void *)_png_close_data, sizeof(_png_close_data));
        if (area) {
            PaintImageEx(hdc, client_rc.right - MINIMIZED_HEIGHT, 0, area, "png");
            MGUI_FreeRW(area);
        }
    }
    else {
#if 0
        SelectFont(hdc, GetSystemFont(SYSLOGFONT_CAPTION));

        RECT title_rc, bar_rc;
        get_title_rc(hWnd, &title_rc);

        SetTextColor(hdc, DWORD2Pixel(hdc, 0xFFFFFFFF));
        SetBkMode(hdc, BM_TRANSPARENT);
        DrawText(hdc, GetWindowCaption(hWnd), -1, &title_rc,
                DT_CENTER | DT_TOP | DT_SINGLELINE);

        get_bar_rc(hWnd, &title_rc, &bar_rc);

        SetPenColor(hdc, DWORD2Pixel(hdc, 0xFFFFFFFF));
        SetBrushColor(hdc, DWORD2Pixel(hdc, 0xFFFFFFFF));
        Rectangle(hdc, bar_rc.left, bar_rc.top, bar_rc.right, bar_rc.bottom);

        unsigned percent = (unsigned)GetWindowAdditionalData(hWnd);
        FillBox(hdc, bar_rc.left, bar_rc.top, RECTW(bar_rc) * percent / 100,
                RECTH(bar_rc));
#else
        if (load_hvml_logo(hdc)) {
            RECT logo_rc;

            RECT client_rc;
            GetClientRect(hWnd, &client_rc);

            get_logo_rect(hWnd, &client_rc, &logo_rc);

            FillBoxWithBitmap(hdc, logo_rc.left, logo_rc.top,
                    RECTW(logo_rc), RECTH(logo_rc), &bmp_hvml_logo);

            BYTE alpha;
            DWORD count = GetWindowAdditionalData2(hWnd);
            if (((count / 256) % 2) == 0) {
                alpha = 255 - (count % 256);
            }
            else {
                alpha = (count % 256);
            }

            SetBrushColor(alpha_mask_dc,
                    RGBA2Pixel(alpha_mask_dc, 0x00, 0x00, 0x00, alpha));
            FillBox(alpha_mask_dc, 0, 0, RECTW(logo_rc), RECTH(logo_rc));
            BitBlt(alpha_mask_dc, 0, 0, RECTW(logo_rc), RECTH(logo_rc),
                    hdc, logo_rc.left, logo_rc.top, COLOR_BLEND_PD_SRC_ATOP);
        }
#endif
    }
}

static LRESULT
FloatingToolWinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case MSG_CREATE:
            SetTimer(hWnd, 100, 5);
            SetWindowAdditionalData2(hWnd, 0);
            break;

        case MSG_TIMER:
            if (GetWindowStyle(hWnd) & WS_MINIMIZE) {
            }
            else {
                RECT client_rc, logo_rc;
                GetClientRect(hWnd, &client_rc);
                get_logo_rect(hWnd, &client_rc, &logo_rc);
                InvalidateRect(hWnd, &logo_rc, TRUE);

                DWORD count = GetWindowAdditionalData2(hWnd);
                count += 16;
                SetWindowAdditionalData2(hWnd, count);
            }
            break;

        case MSG_LBUTTONDOWN:
            break;

        case MSG_IDLE:
            if (GetWindowStyle(hWnd) & WS_MINIMIZE) {
                unsigned percent = (unsigned)GetWindowAdditionalData(hWnd);
                if (percent == 100) {
                    RECT client_rc, bar_rc;
                    GetClientRect(hWnd, &client_rc);

                    get_minimized_bar_rc(hWnd, &client_rc, &bar_rc);
                    SetWindowAdditionalData(hWnd, 111);
                    InvalidateRect(hWnd, &bar_rc, FALSE);
                }
            }
            break;

        case MSG_LBUTTONUP:
            if (GetWindowStyle(hWnd) & WS_MINIMIZE) {
                RECT client_rc;
                GetClientRect(hWnd, &client_rc);
                int x = LOSWORD(lParam);
                if (x >= client_rc.right - MINIMIZED_HEIGHT)
                    SendNotifyMessage(GetHosting(hWnd), MSG_CLOSE, 0, 0);
            }
            break;

        case MSG_XGUIPRO_LOAD_STATE:
            if (GetWindowStyle(hWnd) & WS_MINIMIZE) {
                if (wParam == XGUIPRO_LOAD_STATE_PROGRESS_CHANGED) {
                    RECT client_rc, bar_rc;
                    GetClientRect(hWnd, &client_rc);

                    get_minimized_bar_rc(hWnd, &client_rc, &bar_rc);
                    SetWindowAdditionalData(hWnd, lParam);
                    InvalidateRect(hWnd, &bar_rc, FALSE);
                }

                /* do not handle others */
                break;
            }

            if (wParam == XGUIPRO_LOAD_STATE_TITLE_CHANGED) {
                SetWindowCaption(hWnd, (const char *)lParam);
#if 0
                RECT title_rc;
                get_title_rc(hWnd, &title_rc);
                InvalidateRect(hWnd, &title_rc, TRUE);
#endif
            }
            else if (wParam == XGUIPRO_LOAD_STATE_REDIRECTED) {
                SetWindowCaption(hWnd, "Redirected");
#if 0
                RECT title_rc, bar_rc;
                get_title_rc(hWnd, &title_rc);
                get_bar_rc(hWnd, &title_rc, &bar_rc);
                SetWindowAdditionalData(hWnd, 0);
                InvalidateRect(hWnd, &title_rc, TRUE);
                InvalidateRect(hWnd, &bar_rc, TRUE);
#else
#endif
            }
            else if (wParam == XGUIPRO_LOAD_STATE_PROGRESS_CHANGED) {
#if 0
                RECT title_rc, bar_rc;
                get_title_rc(hWnd, &title_rc);
                get_bar_rc(hWnd, &title_rc, &bar_rc);
                SetWindowAdditionalData(hWnd, lParam);
                InvalidateRect(hWnd, &bar_rc, TRUE);
#else
#endif
            }
            else if (wParam == XGUIPRO_LOAD_STATE_COMMITTED ||
                    wParam == XGUIPRO_LOAD_STATE_FINISHED ||
                    wParam == XGUIPRO_LOAD_STATE_FAILED) {
                ShowWindow(GetHosting(hWnd), SW_SHOWNORMAL);
                minimize_tool_window(hWnd);
                KillTimer(hWnd, 100);
            }
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

HWND create_floating_tool_window(HWND hostingWnd, const char *title)
{
    HWND toolWnd;
    MAINWINCREATE CreateInfo;

    RECT rcHosting;
    GetWindowRect(hostingWnd, &rcHosting);
    CreateInfo.dwStyle = WS_VISIBLE;
    CreateInfo.dwExStyle = WS_EX_WINTYPE_HIGHER | WS_EX_TOOLWINDOW;
    CreateInfo.spCaption = title ? title : "";
    CreateInfo.hMenu = 0;
    CreateInfo.hCursor = GetSystemCursor(0);
    CreateInfo.hIcon = 0;
    CreateInfo.MainWindowProc = FloatingToolWinProc;
    CreateInfo.lx = rcHosting.left;
    CreateInfo.ty = rcHosting.top;
    CreateInfo.rx = rcHosting.right;
    CreateInfo.by = rcHosting.bottom;
    CreateInfo.iBkColor = COLOR_black;
    CreateInfo.dwAddData = 0;
    CreateInfo.hHosting = hostingWnd;

    toolWnd = CreateMainWindowEx2(&CreateInfo, 0, NULL, NULL,
            ST_PIXEL_ARGB8888,
            MakeRGBA(0, 0, 0, 0xA0),
            CT_ALPHAPIXEL, COLOR_BLEND_PD_SRC_OVER);

    if (toolWnd != HWND_INVALID)
        ShowWindow(toolWnd, SW_SHOWNORMAL);

    return toolWnd;
}

