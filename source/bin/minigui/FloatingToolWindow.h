/*
** FloatingToolWindow.h -- The declaration of FloatingToolWindow.
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

#ifndef FloatingToolWindow_h
#define FloatingToolWindow_h

#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>

#define MSG_XGUIPRO_LOAD_STATE          (MSG_USER + 1)

enum {
    XGUIPRO_LOAD_STATE_INITIAL = 0,
    XGUIPRO_LOAD_STATE_REDIRECTED,
    XGUIPRO_LOAD_STATE_COMMITTED,
    XGUIPRO_LOAD_STATE_TITLE_CHANGED,
    XGUIPRO_LOAD_STATE_PROGRESS_CHANGED,
    XGUIPRO_LOAD_STATE_FINISHED,
    XGUIPRO_LOAD_STATE_FAILED,
};

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

HWND create_floating_tool_window(HWND hostingWnd, const char *title);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* FloatingToolWindow_h */

