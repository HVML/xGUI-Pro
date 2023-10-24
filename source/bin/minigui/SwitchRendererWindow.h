/*
** SwitchRendererWindow.h -- The declaration of SwitchRendererWindow.
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

#ifndef SwitchRendererWindow_h
#define SwitchRendererWindow_h

#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/control.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct sd_remote_service;
int show_switch_renderer_window(HWND hWnd, struct sd_remote_service *rs);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* SwitchRendererWindow_h */

