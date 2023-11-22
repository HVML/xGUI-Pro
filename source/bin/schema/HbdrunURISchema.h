/*
** HbdrunURISchema.h -- header for hbdrun URI schema.
**
** Copyright (C) 2023 FMSoft (http://www.fmsoft.cn)
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

#ifndef hbdrunURISchema_h
#define hbdrunURISchema_h

#include <webkit2/webkit2.h>

#define BROWSER_HBDRUN_SCHEME                           "hbdrun"

#define BROWSER_HBDRUN_ACTION_PARAM_TYPE                "type"
#define BROWSER_HBDRUN_ACTION_PARAM_RESULT              "result"

#define BROWSER_HBDRUN_ACTION_TYPE_CONFIRM              "confirm"

#define CONFIRM_RESULT_ACCEPT_ONCE                      "AcceptOnce"
#define CONFIRM_RESULT_ACCEPT_ALWAYS                    "AcceptALWAYS"
#define CONFIRM_RESULT_DECLINE                          "Decline"

#ifdef __cplusplus
extern "C" {
#endif

void hbdrunURISchemeRequestCallback(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext);

#ifdef __cplusplus
}
#endif

#endif  /* hbdrunURISchema_h */

