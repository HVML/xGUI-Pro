/*
** AuthWindow.h -- The declaration of AuthWindow.
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

#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>
#include <glib.h>
#include <locale.h>

#include "AuthWindow.h"
#include "xguipro-features.h"

#define DEF_APP_ICON                      "assets/hvml-v-fill-white.png"

#define AUTH_TIMEOUT_ID                   10001
#define TIMEOUT_STR_LEN                   32

static DLGTEMPLATE DlgInitAuth =
{
    WS_BORDER,
    WS_EX_NONE,
    0, 0, 10, 10,
    "",
    0, 0,
    7, NULL,
    0
};

enum {
    CTRL_TITLE,
    CTRL_APP_NAME,
    CTRL_APP_LABEL,
    CTRL_APP_DESC,
    CTRL_APP_HOST_NAME,
    CTRL_BTN_ACCEPT,
    CTRL_BTN_REJECT,
    CTRL_LAST,
};

enum {
    IDC_TITLE = 100,
    IDC_APP_NAME,
    IDC_APP_LABEL,
    IDC_APP_DESC,
    IDC_APP_HOST_NAME,
    IDC_BTN_ACCEPT = IDYES,
    IDC_BTN_REJECT = IDNO
};

const char *cn[] = {
    "收到新的连接请求：",
    "名称：",
    "标签：",
    "描述：",
    "来源：",
    "接受",
    "拒绝",
};

const char *zh[] = {
    "收到新的連結請求：",
    "名稱：",
    "標籤：",
    "描述：",
    "来源：",
    "接受",
    "拒絕",
};

const char *en[] = {
    "New Connection:",
    "Name:",
    "Label:",
    "Desc:",
    "Source:",
    "Accept",
    "Reject",
};

static CTRLDATA CtrlInitAuth [] =
{
    {
        "static",
        WS_VISIBLE | SS_SIMPLE,
        0, 0, 10, 10,
        IDC_TITLE,
        NULL,
        0
    },
    {
        "static",
        WS_VISIBLE | SS_SIMPLE,
        0, 0, 10, 10,
        IDC_APP_NAME,
        NULL,
        0
    },
    {
        "static",
        WS_VISIBLE | SS_SIMPLE,
        0, 0, 10, 10,
        IDC_APP_LABEL,
        NULL,
        0
    },
    {
        "static",
        WS_VISIBLE | SS_SIMPLE,
        0, 0, 10, 10,
        IDC_APP_DESC,
        NULL,
        0
    },
    {
        "static",
        WS_VISIBLE | SS_SIMPLE,
        0, 0, 10, 10,
        IDC_APP_HOST_NAME,
        NULL,
        0
    },
    {
        "button",
        WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 10, 10,
        IDC_BTN_ACCEPT,
        NULL,
        0
    },
    {
        "button",
        WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 10, 10,
        IDC_BTN_REJECT,
        NULL,
        0
    },
};

PLOGFONT lf = NULL;
BITMAP app_icon;
PBITMAP g_app_icon = NULL;
int app_icon_x;
int app_icon_y;
int app_icon_w;
int app_icon_h;
const char **s_res = NULL;
uint64_t g_timeout_seconds = 10;


static int init_font(int size)
{
    if (lf) {
        goto out;
    }

    lf = CreateLogFontEx("ttf", "helvetica", "UTF-8",
                FONT_WEIGHT_REGULAR,
                FONT_SLANT_ROMAN,
                FONT_FLIP_NONE,
                FONT_OTHER_NONE,
                FONT_DECORATE_NONE, FONT_RENDER_SUBPIXEL,
                size, 0);
out:
    return 0;
}

static void destroy_font()
{
    if (lf) {
        DestroyLogFont(lf);
        lf = NULL;
    }
}

static void load_app_icon()
{
    const char *webext_dir = g_getenv("WEBKIT_WEBEXT_DIR");
    if (webext_dir == NULL) {
        webext_dir = WEBKIT_WEBEXT_DIR;
    }

    gchar *path = g_strdup_printf("%s/%s", webext_dir ? webext_dir : ".", DEF_APP_ICON);

    if (path) {
        LoadBitmapFromFile(HDC_SCREEN, &app_icon, path);
        g_app_icon = &app_icon;
        g_free(path);
    }
}

static void unload_app_icon()
{
    if (g_app_icon) {
        UnloadBitmap(g_app_icon);
        g_app_icon = NULL;
    }
}

static LRESULT InitDialogBoxProc (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case MSG_INITDIALOG:
        {
            HWND accept = GetDlgItem(hDlg, IDC_BTN_ACCEPT);
            SetFocus(accept);

            SetWindowFont(GetDlgItem(hDlg, IDC_TITLE), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_APP_NAME), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_APP_LABEL), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_APP_DESC), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_APP_HOST_NAME), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_BTN_ACCEPT), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_BTN_REJECT), lf);

            HWND label = GetDlgItem(hDlg, IDC_APP_LABEL);
            int nr_txt = GetWindowTextLength(label);
            char txt[nr_txt + 1];
            GetWindowText(label, txt, nr_txt);
            SIZE sz;
            HDC hdc = GetDC(GetDlgItem(hDlg, IDC_APP_LABEL));
            GetTabbedTextExtent(hdc, txt, nr_txt, &sz);

            RECT rc;
            GetWindowRect(label, &rc);

            int x = app_icon_x;
            if (sz.cx > app_icon_w) {
                x = app_icon_x - (sz.cx - app_icon_w) / 2;
            }
            else if (sz.cx < app_icon_w) {
                x = app_icon_x + (app_icon_w - sz.cx) / 2;
            }
            MoveWindow(label, x, rc.top, sz.cx, RECTH(rc), TRUE);

            SetTimer(hDlg, AUTH_TIMEOUT_ID, 100);
        }
        return 0;

    case MSG_COMMAND:
        switch (wParam) {
        case IDYES:
        case IDNO:
            EndDialog (hDlg, wParam);
            break;
        }
        break;

    case MSG_TIMER:
        {
            g_timeout_seconds--;
            HWND ctrl = GetDlgItem(hDlg, IDC_BTN_REJECT);
            size_t n = strlen(s_res[CTRL_BTN_REJECT]) + TIMEOUT_STR_LEN + 1;
            char buf[n];
            sprintf(buf, "%s(%ld)", s_res[CTRL_BTN_REJECT], g_timeout_seconds);
            SetWindowText(ctrl, buf);

            if (g_timeout_seconds <= 0) {
                KillTimer(hDlg, AUTH_TIMEOUT_ID);
                EndDialog(hDlg, IDNO);
            }
        }
        break;

    case MSG_PAINT:
        if (g_app_icon) {
            HDC hdc = BeginPaint(hDlg);
            FillBoxWithBitmap(hdc, app_icon_x, app_icon_y, app_icon_w,
                    app_icon_h, g_app_icon);
            EndPaint(hDlg, hdc);
            return 0;
        }
        break;

    case MSG_CLOSE:
        EndDialog (hDlg, IDCANCEL);
        destroy_font();
        unload_app_icon();
        break;
    }

    return DefaultDialogProc (hDlg, message, wParam, lParam);
}

RECT xphbd_get_default_window_rect(void);

const char **get_string_res()
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

int show_auth_window(HWND hWnd, const char *app_name, const char *app_label,
        const char *app_desc, const char *host_name, uint64_t timeout_seconds)
{
    RECT rc = xphbd_get_default_window_rect();

    s_res = get_string_res();
    g_timeout_seconds = timeout_seconds;

    const char *title = s_res[CTRL_TITLE];

    size_t nr = strlen(s_res[CTRL_APP_NAME]) + strlen(app_name) + 1;
    char s_app_name[nr];
    strcpy(s_app_name, s_res[CTRL_APP_NAME]);
    strcat(s_app_name, app_name);

    nr = strlen(s_res[CTRL_APP_DESC]) + strlen(app_desc) + 1;
    char s_app_desc[nr];
    strcpy(s_app_desc, s_res[CTRL_APP_DESC]);
    strcat(s_app_desc, app_desc);

    nr = strlen(s_res[CTRL_APP_HOST_NAME]) + strlen(host_name) + 1;
    char s_host_name[nr];
    strcpy(s_host_name, s_res[CTRL_APP_HOST_NAME]);
    strcat(s_host_name, host_name);

    nr = strlen(s_res[CTRL_BTN_REJECT]) + TIMEOUT_STR_LEN + 1;
    char s_reject[nr];
    sprintf(s_reject, "%s(%ld)", s_res[CTRL_BTN_REJECT], g_timeout_seconds);


    int dlg_w = RECTW(rc) * 3 / 4;
    int dlg_h = RECTH(rc) / 2;
    int dlg_x = rc.left + (RECTW(rc) - dlg_w) / 2;
    int dlg_y = rc.top + (RECTH(rc) - dlg_h) / 2;

    DlgInitAuth.w = dlg_w;
    DlgInitAuth.h = dlg_h;
    DlgInitAuth.x = dlg_x;
    DlgInitAuth.y = dlg_y;

    int font_size = dlg_h * 0.8 / 8;

    int x = dlg_w / 10;
    int y = dlg_h / 10;
    int h = dlg_h * 0.8 / 7;
    int xh = dlg_h * 0.8 / 6;

    init_font(font_size);
    load_app_icon();

    app_icon_w = 2 * xh;
    app_icon_x = x;
    app_icon_y = dlg_h / 10 + 1.5 * xh;
    app_icon_h = app_icon_w;

    if (g_app_icon) {
        int bw = g_app_icon->bmWidth;
        int bh = g_app_icon->bmHeight;
        app_icon_h = bh * app_icon_w / bw;
    }

    int info_w = dlg_w - x;

    PCTRLDATA pctrl;
    /* title */
    pctrl = &CtrlInitAuth[CTRL_TITLE];
    pctrl->caption = title;
    pctrl->x = dlg_w / 15;
    pctrl->y = y;
    pctrl->w = info_w;
    pctrl->h = h;

    /* app label */
    pctrl = &CtrlInitAuth[CTRL_APP_LABEL];
    pctrl->caption = app_label;
    pctrl->x = app_icon_x;
    pctrl->y = app_icon_y + app_icon_h;
    pctrl->w = info_w;
    pctrl->h = h;

    /* app name */
    x = app_icon_x + app_icon_w + app_icon_w / 3;
    y = app_icon_y;
    pctrl = &CtrlInitAuth[CTRL_APP_NAME];
    pctrl->caption = s_app_name;
    pctrl->x = x;
    pctrl->y = y;
    pctrl->w = info_w;
    pctrl->h = h;


    /* app desc */
    y = y + h;
    pctrl = &CtrlInitAuth[CTRL_APP_DESC];
    pctrl->caption = s_app_desc;
    pctrl->x = x;
    pctrl->y = y;
    pctrl->w = dlg_w - x;
    pctrl->h = h;

    /* host */
    y = y + h;
    pctrl = &CtrlInitAuth[CTRL_APP_HOST_NAME];
    pctrl->caption = s_host_name;
    pctrl->x = x;
    pctrl->y = y;
    pctrl->w = info_w;
    pctrl->h = h;

    /* accept */
    int btn_w = dlg_w * 0.3;
    y = dlg_h - dlg_h / 10 - xh;
    x = (dlg_w - 2 * btn_w) / 3;
    pctrl = &CtrlInitAuth[CTRL_BTN_ACCEPT];
    pctrl->caption = s_res[CTRL_BTN_ACCEPT];
    pctrl->x = x;
    pctrl->y = y;
    pctrl->w = btn_w;
    pctrl->h = xh;

    /* reject */
    pctrl = &CtrlInitAuth[CTRL_BTN_REJECT];
    pctrl->caption = s_reject;
    pctrl->x = x + btn_w + x;
    pctrl->y = y;
    pctrl->w = btn_w;
    pctrl->h = xh;

    DlgInitAuth.controlnr = CTRL_LAST;
    DlgInitAuth.controls = CtrlInitAuth;

    return DialogBoxIndirectParam (&DlgInitAuth, hWnd, InitDialogBoxProc, 0L);
}


#ifdef _MGRM_THREADS
#include <minigui/dti.c>
#endif

