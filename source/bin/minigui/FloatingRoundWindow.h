/*
** FloatingRoundWindow.h -- The declaration of FloatingRoundWindow.
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

#ifndef FloatingRoundWindow_h
#define FloatingRoundWindow_h

#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>

#define MSG_XGUIPRO_NEW_RDR          (MSG_USER + 1)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

HWND create_floating_round_window(HWND hostingWnd, const char *title);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* FloatingRoundWindow_h */

