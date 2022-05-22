/*
** PurcmcCallbacks.h -- The declarations of structure and callbacks for PurCMC.
**
** Copyright (C) 2022 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
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

#ifndef PurcmcCallbacks_h
#define PurcmcCallbacks_h

#include "purcmc/purcmc.h"

#ifdef __cplusplus
extern "C" {
#endif

purcmc_session *gtk_create_session(void *context, purcmc_endpoint *);
int gtk_remove_session(purcmc_session *);

purcmc_plainwin *gtk_create_plainwin(purcmc_session *, purcmc_workspace *,
        const char *gid,
        const char *name, const char *title, purc_variant_t properties,
        int *retv);
int gtk_update_plainwin(purcmc_session *, purcmc_workspace *,
        purcmc_plainwin *win, const char *property, const char *value);
int gtk_destroy_plainwin(purcmc_session *, purcmc_workspace *,
        purcmc_plainwin *win);
purcmc_page *gtk_get_plainwin_page(purcmc_session *,
        purcmc_plainwin *win, int *retv);

purcmc_dom *gtk_load_or_write(purcmc_session *, purcmc_page *,
            int op, const char *op_name,
            const char *content, size_t length, int *retv);
int gtk_update_dom(purcmc_session *, purcmc_dom *,
            int op, const char *op_name, const pcrdr_msg *msg);

#ifdef __cplusplus
}
#endif

#endif  /* PurcmcCallbacks_h */

