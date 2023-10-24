/*
** SwitchRendererWindow.h -- The declaration of SwitchRendererWindow.
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

#include "SwitchRendererWindow.h"
#include "xguipro-features.h"

#include "sd/sd.h"

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
    CTRL_RDR_TXT,
    CTRL_RDR_PORT,
    CTRL_RDR_HOST,
    CTRL_BTN_ACCEPT,
    CTRL_BTN_REJECT,
    CTRL_LAST,
};

enum {
    IDC_TITLE = 100,
    IDC_RDR_TXT,
    IDC_RDR_PORT,
    IDC_RDR_HOST,
    IDC_BTN_ACCEPT = IDYES,
    IDC_BTN_REJECT = IDNO
};

static const char *cn[] = {
    "发现新的渲染器：",
    "描述：",
    "来源：",
    "端口：",
    "接受",
    "拒绝",
};

static const char *zh[] = {
    "發現新的渲染器：",
    "描述：",
    "来源：",
    "端口：",
    "接受",
    "拒絕",
};

static const char *en[] = {
    "New Renderer:",
    "Desc:",
    "Source:",
    "Port:",
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
        IDC_RDR_TXT,
        NULL,
        0
    },
    {
        "static",
        WS_VISIBLE | SS_SIMPLE,
        0, 0, 10, 10,
        IDC_RDR_HOST,
        NULL,
        0
    },
    {
        "static",
        WS_VISIBLE | SS_SIMPLE,
        0, 0, 10, 10,
        IDC_RDR_PORT,
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

static PLOGFONT lf = NULL;
static BITMAP app_icon;
static PBITMAP g_app_icon = NULL;
static int app_icon_x;
static int app_icon_y;
static int app_icon_w;
static int app_icon_h;
static const char **s_res = NULL;


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

static LRESULT InitDialogBoxProc (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case MSG_INITDIALOG:
        {
            HWND accept = GetDlgItem(hDlg, IDC_BTN_ACCEPT);
            SetFocus(accept);

            SetWindowFont(GetDlgItem(hDlg, IDC_TITLE), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_RDR_TXT), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_RDR_HOST), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_RDR_PORT), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_BTN_ACCEPT), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_BTN_REJECT), lf);

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

    case MSG_CLOSE:
        EndDialog (hDlg, IDCANCEL);
        destroy_font();
        break;
    }

    return DefaultDialogProc (hDlg, message, wParam, lParam);
}

RECT xphbd_get_default_window_rect(void);

static const char **get_string_res()
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

int show_switch_renderer_window(HWND hWnd, struct sd_remote_service *rs)
{
    RECT rc = xphbd_get_default_window_rect();

    s_res = get_string_res();

    const char *title = s_res[CTRL_TITLE];

    size_t nr = strlen(s_res[CTRL_RDR_TXT]) + strlen(rs->txt) + 1;
    char s_rdr_txt[nr];
    strcpy(s_rdr_txt, s_res[CTRL_RDR_TXT]);
    strcat(s_rdr_txt, rs->txt);

    nr = strlen(s_res[CTRL_RDR_PORT]) + 10 + 1;
    char s_rdr_port[nr];
    sprintf(s_rdr_port, "%s%d\n", s_res[CTRL_RDR_PORT], rs->port);

    nr = strlen(s_res[CTRL_RDR_HOST]) + strlen(rs->host) + 1;
    char s_rdr_host[nr];
    strcpy(s_rdr_host, s_res[CTRL_RDR_HOST]);
    strcat(s_rdr_host, rs->host);

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

    /* rdr txt */
    x = app_icon_x + app_icon_w + app_icon_w / 3;
    y = app_icon_y;
    pctrl = &CtrlInitAuth[CTRL_RDR_TXT];
    pctrl->caption = s_rdr_txt;
    pctrl->x = x;
    pctrl->y = y;
    pctrl->w = info_w;
    pctrl->h = h;


    /* host */
    y = y + h;
    pctrl = &CtrlInitAuth[CTRL_RDR_HOST];
    pctrl->caption = s_rdr_host;
    pctrl->x = x;
    pctrl->y = y;
    pctrl->w = info_w;
    pctrl->h = h;

    /* port */
    y = y + h;
    pctrl = &CtrlInitAuth[CTRL_RDR_PORT];
    pctrl->caption = s_rdr_port;
    pctrl->x = x;
    pctrl->y = y;
    pctrl->w = dlg_w - x;
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
    pctrl->caption = s_res[CTRL_BTN_REJECT];;
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
