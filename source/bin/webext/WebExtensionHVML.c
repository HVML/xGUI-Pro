/*
** WebExtensionHVML.c -- The WebKit2 web extension for HVML.
**
** Copyright (C) 2022 FMSoft <http://www.fmsoft.cn>
**
** Author: Vincent Wei <https://github.com/VincentWei>
**
** This file is part of xGUI Pro.
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

#include <webkit2/webkit-web-extension.h>

static void
web_page_created_callback(WebKitWebExtension *extension,
        WebKitWebPage *web_page, gpointer user_data)
{
    g_print("Page %llu created for %s\n",
            (unsigned long long)webkit_web_page_get_id(web_page),
            webkit_web_page_get_uri(web_page));
}

G_MODULE_EXPORT void
webkit_web_extension_initialize_user_data(WebKitWebExtension *extension,
        const GVariant *user_data)
{
    (void)user_data;

    g_signal_connect(extension, "page-created",
            G_CALLBACK (web_page_created_callback),
            NULL);
}

