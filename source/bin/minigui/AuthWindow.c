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

#define WINDOW_TITLE                      "Authenticate Connection"
#define MESSAGES                          "New PurC from 192.168.1.100"
#define ACCEPT                            "Accept"
#define REJECT                            "Reject"

static DLGTEMPLATE DlgInitProgress =
{
    WS_BORDER | WS_CAPTION,
    WS_EX_NONE,
    120, 150, 400, 140,
    WINDOW_TITLE,
    0, 0,
    3, NULL,
    0
};

#define IDC_PROMPTINFO  100
#define IDC_PROGRESS    110

static CTRLDATA CtrlInitProgress [] =
{
    {
        "static",
        WS_VISIBLE | SS_SIMPLE,
        10, 10, 380, 16,
        IDC_PROMPTINFO,
        MESSAGES,
        0
    },
    {
        "button",
        WS_TABSTOP | WS_VISIBLE | BS_DEFPUSHBUTTON,
        90, 70, 60, 25,
        IDYES,
        ACCEPT,
        0
    },
    {
        "button",
        WS_TABSTOP | WS_VISIBLE | BS_DEFPUSHBUTTON,
        250, 70, 60, 25,
        IDNO,
        REJECT,
        0
    }
};

static LRESULT InitDialogBoxProc (HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case MSG_INITDIALOG:
        return 1;

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
        break;
    }

    return DefaultDialogProc (hDlg, message, wParam, lParam);
}

int create_auth_window(HWND hWnd)
{
    DlgInitProgress.controls = CtrlInitProgress;

    return DialogBoxIndirectParam (&DlgInitProgress, hWnd, InitDialogBoxProc, 0L);
}


#ifdef _MGRM_THREADS
#include <minigui/dti.c>
#endif

