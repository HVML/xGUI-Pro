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
#include "AuthWindow.h"

#define AUTH_WINDOW_TITLE                      "Authenticate Connection"
#define MESSAGES                          "New PurC from 192.168.1.100"
#define ACCEPT                            "Accept"
#define REJECT                            "Reject"

static DLGTEMPLATE DlgInitAuth =
{
    WS_BORDER,
    WS_EX_NONE,
    0, 0, 10, 10,
    AUTH_WINDOW_TITLE,
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
        ACCEPT,
        0
    },
    {
        "button",
        WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 10, 10,
        IDC_BTN_REJECT,
        REJECT,
        0
    },
};

PLOGFONT lf = NULL;

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
            SetWindowFont(GetDlgItem(hDlg, IDC_APP_NAME), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_APP_LABEL), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_APP_DESC), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_APP_HOST_NAME), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_BTN_ACCEPT), lf);
            SetWindowFont(GetDlgItem(hDlg, IDC_BTN_REJECT), lf);
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

int show_auth_window(HWND hWnd, const char *app_name, const char *app_label,
        const char *app_desc, const char *host_name)
{
    RECT rc = xphbd_get_default_window_rect();

    int dlg_w = RECTW(rc) / 2;
    int dlg_h = RECTH(rc) / 2;
    int dlg_x = rc.left + (RECTW(rc) - dlg_w) / 2;
    int dlg_y = rc.top + (RECTH(rc) - dlg_h) / 2;

    DlgInitAuth.w = dlg_w;
    DlgInitAuth.h = dlg_h;
    DlgInitAuth.x = dlg_x;
    DlgInitAuth.y = dlg_y;

    int x = dlg_w / 10;
    int y = dlg_h / 10;
    int h = dlg_h * 0.8 / 6;
    int font_size = h * 0.5;

    PCTRLDATA pctrl;
    /* title */
    pctrl = &CtrlInitAuth[CTRL_TITLE];
    pctrl->caption = AUTH_WINDOW_TITLE;
    pctrl->x = x;
    pctrl->y = y;
    pctrl->w = dlg_w - pctrl->x;
    pctrl->h = h;

    x = dlg_w * 0.15;
    /* app name */
    y = y + h;
    pctrl = &CtrlInitAuth[CTRL_APP_NAME];
    pctrl->caption = app_name;
    pctrl->x = x;
    pctrl->y = y;
    pctrl->w = dlg_w - pctrl->x;
    pctrl->h = h;

    /* app label */
    y = y + h;
    pctrl = &CtrlInitAuth[CTRL_APP_LABEL];
    pctrl->caption = app_label;
    pctrl->x = x;
    pctrl->y = y;
    pctrl->w = dlg_w - pctrl->x;
    pctrl->h = h;

    /* app desc */
    y = y + h;
    pctrl = &CtrlInitAuth[CTRL_APP_DESC];
    pctrl->caption = app_desc;
    pctrl->x = x;
    pctrl->y = y;
    pctrl->w = dlg_w - pctrl->x;
    pctrl->h = h;

    /* host */
    y = y + h;
    pctrl = &CtrlInitAuth[CTRL_APP_HOST_NAME];
    pctrl->caption = host_name;
    pctrl->x = x;
    pctrl->y = y;
    pctrl->w = dlg_w - pctrl->x;
    pctrl->h = h;

    /* accept */
    y = y + h;
    int btn_w = dlg_w * 0.3;
    x = (dlg_w - 2 * btn_w) / 3;
    pctrl = &CtrlInitAuth[CTRL_BTN_ACCEPT];
    pctrl->x = x;
    pctrl->y = y;
    pctrl->w = btn_w;
    pctrl->h = h;

    /* reject */
    pctrl = &CtrlInitAuth[CTRL_BTN_REJECT];
    pctrl->x = x + btn_w + x;
    pctrl->y = y;
    pctrl->w = btn_w;
    pctrl->h = h;

    DlgInitAuth.controlnr = CTRL_LAST;
    DlgInitAuth.controls = CtrlInitAuth;

    init_font(font_size);
    return DialogBoxIndirectParam (&DlgInitAuth, hWnd, InitDialogBoxProc, 0L);
}


#ifdef _MGRM_THREADS
#include <minigui/dti.c>
#endif

