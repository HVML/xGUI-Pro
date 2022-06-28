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

purcmc_session *gtk_create_session(purcmc_server *, purcmc_endpoint *);
int gtk_remove_session(purcmc_session *);

purcmc_plainwin *gtk_create_plainwin(purcmc_session *, purcmc_workspace *,
        const char *request_id, const char *gid, const char *name,
        const char *class_name, const char *title, const char *layout_style,
        purc_variant_t toolkit_style, int *retv);
int gtk_update_plainwin(purcmc_session *, purcmc_workspace *,
        purcmc_plainwin *win, const char *property, purc_variant_t value);
int gtk_destroy_plainwin(purcmc_session *, purcmc_workspace *,
        purcmc_plainwin *win);
purcmc_page *gtk_get_plainwin_page(purcmc_session *,
        purcmc_plainwin *win, int *retv);

int gtk_set_page_groups(purcmc_session *, purcmc_workspace *,
        const char *content, size_t length);
int gtk_add_page_groups(purcmc_session *, purcmc_workspace *,
        const char *content, size_t length);
int gtk_remove_page_group(purcmc_session *, purcmc_workspace *,
        const char* gid);

purcmc_page *gtk_create_widget(purcmc_session *, purcmc_workspace *,
            const char *request_id, const char *gid, const char *name,
            const char *class_name, const char *title, const char *layout_style,
            purc_variant_t toolkit_style, int *retv);
int gtk_update_widget(purcmc_session *, purcmc_workspace *,
            purcmc_page *page, const char *property, purc_variant_t value);
int gtk_destroy_widget(purcmc_session *, purcmc_workspace *,
            purcmc_page *page);

purcmc_dom *gtk_load_or_write(purcmc_session *, purcmc_page *,
            int op, const char *op_name, const char *request_id,
            const char *content, size_t length, int *retv);
int gtk_update_dom(purcmc_session *, purcmc_dom *,
            int op, const char *op_name, const char* request_id,
            const char* element_type, const char* element_value,
            const char* property,
            const char *content, size_t length);

purc_variant_t gtk_call_method_in_dom(purcmc_session *, const char *,
        purcmc_dom *, const char* element_type, const char* element_value,
        const char *method, purc_variant_t arg, int* retv);

purc_variant_t gtk_get_property_in_dom(purcmc_session *, const char *,
        purcmc_dom *, const char* element_type, const char* element_value,
        const char *property, int *retv);

purc_variant_t gtk_set_property_in_dom(purcmc_session *, const char *,
        purcmc_dom *, const char* element_type, const char* element_value,
        const char *property, purc_variant_t value, int *retv);

bool gtk_pend_response(purcmc_session* sess, const char *operation,
        const char *request_id, void *result_value);

/*
 * Use this function to retrieve the endpoint of a session.
 * the endpoint might be deleted earlier than session.
 */
purcmc_endpoint* purcmc_get_endpoint_by_session(purcmc_session *sess);

#ifdef __cplusplus
}
#endif

#endif  /* PurcmcCallbacks_h */

