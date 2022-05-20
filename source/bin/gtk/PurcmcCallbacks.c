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

#include "config.h"
#include "main.h"
#include "BrowserWindow.h"
#include "BuildRevision.h"
#include "PurcmcCallbacks.h"
#include "HVMLURISchema.h"

#include "purcmc/purcmc.h"

#include <errno.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit2/webkit2.h>

struct purcmc_session {
    WebKitSettings *webSettings;
    WebKitWebContext *webContext;
};

purcmc_session *gtk_create_session(void *context, purcmc_endpoint *endpt)
{
    purcmc_session* sess = calloc(1, sizeof(purcmc_session));

    WebKitSettings *webSettings = context;
    WebKitWebsiteDataManager *manager;
    manager = g_object_get_data(G_OBJECT(webSettings),
            "default-website-data-manager");

    WebKitWebContext *webContext = g_object_new(WEBKIT_TYPE_WEB_CONTEXT,
            "website-data-manager", manager,
            "process-swap-on-cross-site-navigation-enabled", TRUE,
#if !GTK_CHECK_VERSION(3, 98, 0)
            "use-system-appearance-for-scrollbars", FALSE,
#endif
            NULL);
    g_object_unref(manager);

    g_signal_connect(webContext, "initialize-web-extensions",
            G_CALLBACK(initializeWebExtensionsCallback), NULL);

#if 0
    if (enableSandbox)
        webkit_web_context_set_sandbox_enabled(webContext, TRUE);

    if (cookiesPolicy) {
        WebKitCookieManager *cookieManager = webkit_web_context_get_cookie_manager(webContext);
        GEnumClass *enumClass = g_type_class_ref(WEBKIT_TYPE_COOKIE_ACCEPT_POLICY);
        GEnumValue *enumValue = g_enum_get_value_by_nick(enumClass, cookiesPolicy);
        if (enumValue)
            webkit_cookie_manager_set_accept_policy(cookieManager, enumValue->value);
        g_type_class_unref(enumClass);
    }

    if (cookiesFile && !webkit_web_context_is_ephemeral(webContext)) {
        WebKitCookieManager *cookieManager = webkit_web_context_get_cookie_manager(webContext);
        WebKitCookiePersistentStorage storageType = g_str_has_suffix(cookiesFile, ".txt") ? WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT : WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE;
        webkit_cookie_manager_set_persistent_storage(cookieManager, cookiesFile, storageType);
    }

    // Enable the favicon database, by specifying the default directory.
    webkit_web_context_set_favicon_database_directory(webContext, NULL);

    webkit_web_context_register_uri_scheme(webContext, BROWSER_ABOUT_SCHEME, (WebKitURISchemeRequestCallback)aboutURISchemeRequestCallback, webContext, NULL);

    WebKitUserContentManager *userContentManager = webkit_user_content_manager_new();
    webkit_user_content_manager_register_script_message_handler(userContentManager, "aboutData");
    g_signal_connect(userContentManager, "script-message-received::aboutData", G_CALLBACK(aboutDataScriptMessageReceivedCallback), webContext);

    WebKitWebsitePolicies *defaultWebsitePolicies = webkit_website_policies_new_with_policies(
        "autoplay", autoplayPolicy,
        NULL);
#endif

    // hvml schema
    webkit_web_context_register_uri_scheme(webContext,
            BROWSER_HVML_SCHEME,
            (WebKitURISchemeRequestCallback)hvmlURISchemeRequestCallback,
            webContext, NULL);

    sess->webSettings = webSettings;
    sess->webContext = webContext;
    return sess;
}

int gtk_remove_session(purcmc_session *sess)
{
    g_clear_object(&sess->webContext);
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

