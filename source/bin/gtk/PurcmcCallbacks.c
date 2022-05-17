/*
** PurcmcCallbacks.c -- The implementation of callbacks for PurCMC.
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

#include "purcmc/purcmc.h"

struct purcmc_session {
    int dummy;
};

purcmc_session *gtk_create_session(purcmc_endpoint *endpt)
{
    purcmc_session* sess = calloc(1, sizeof(purcmc_session));
    return sess;
}

int gtk_remove_session(purcmc_session *sess)
{
    free(sess);
    return PCRDR_SC_OK;
}

#if 0
purcmc_plainwin *gtk_create_plainwin(purcmc_session *, purcmc_workspace *,
        const char *gid,
        const char *name, const char *title, purc_variant_t properties,
        int *retv);
int gtk_update_plainwin(purcmc_session *, purcmc_workspace *,
        purcmc_plainwin *win, const char *property, const char *value);
int gtk_destroy_plainwin(purcmc_session *, purcmc_workspace *,
        purcmc_plainwin *win);
purcmc_page *gtk_get_plainwin_page(purcmc_session *, purcmc_plainwin *win);

purcmc_dom *gtk_load(purcmc_session *, purcmc_page *,
            const char *content, size_t length, int *retv);
purcmc_dom *gtk_write(purcmc_session *, purcmc_page *,
            int op, const char *content, size_t length, int *retv);

int gtk_operate_dom_element(purcmc_session *, purcmc_dom *,
            int op, const pcrdr_msg *msg);

#endif  /* PurcmcCallbacks_h */

