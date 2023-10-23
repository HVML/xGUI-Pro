/*
** PopupTipWindow.h -- The declaration of PopupTipWindow.
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

#ifndef PopupTipWindow_h
#define PopupTipWindow_h

#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct sd_remote_service;

HWND create_popup_tip_window(HWND hosting_wnd, struct sd_remote_service *srv);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* PopupTipWindow_h */

