/*
** BrowserPane.c -- The implementation of BrowserPane.
**
** Copyright (C) 2023 FMSoft <http://www.fmsoft.cn>
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
** MERCHANPANEILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see http://www.gnu.org/licenses/.
*/

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif
#include "main.h"
#include "BrowserPane.h"

#include <string.h>

enum {
    PROP_0,

    PROP_WEBVIEW_PARAM

};

G_DEFINE_TYPE(BrowserPane, browser_pane, G_TYPE_OBJECT)

static void browser_pane_init(BrowserPane *pane)
{
}

static void browserPaneSetProperty(GObject *object, guint propId,
        const GValue *value, GParamSpec *pspec)
{
    BrowserPane *pane = BROWSER_PANE(object);

    switch (propId) {
    case PROP_WEBVIEW_PARAM:
        {
            WebKitWebViewParam *param = (WebKitWebViewParam *)
                g_value_get_pointer(value);
            pane->param = *param;
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
    }
}

static void browserPaneConstructed(GObject *gObject)
{
    G_OBJECT_CLASS(browser_pane_parent_class)->constructed(gObject);

    BrowserPane *pane = BROWSER_PANE(gObject);
    pane->webView = xgui_create_webview(&pane->param);
    pane->hwnd = webkit_web_view_get_hwnd(pane->webView);
    pane->parentHwnd = pane->param.webViewParent;
}

static void browserPaneFinalize(GObject *gObject)
{
//    BrowserPane *pane = BROWSER_PANE(gObject);

    G_OBJECT_CLASS(browser_pane_parent_class)->finalize(gObject);
}

static void browser_pane_class_init(BrowserPaneClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = browserPaneConstructed;
    gobjectClass->set_property = browserPaneSetProperty;
    gobjectClass->finalize = browserPaneFinalize;

    g_object_class_install_property(
        gobjectClass,
        PROP_WEBVIEW_PARAM,
        g_param_spec_pointer(
            "param",
            "WebView Param",
            "The web view param",
            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static char *getInternalURI(const char *uri)
{
    if (g_str_equal(uri, "about:gpu"))
        return g_strdup("webkit://gpu");

    /* Internally we use xguipro-about: as about: prefix is ignored by WebKit. */
    if (g_str_has_prefix(uri, "about:") && !g_str_equal(uri, "about:blank"))
        return g_strconcat(BROWSER_ABOUT_SCHEME, uri + strlen ("about"), NULL);

    return g_strdup(uri);
}

/* Public API. */
BrowserPane *browser_pane_new(WebKitWebViewParam *param)
{
    BrowserPane *pane = BROWSER_PANE(g_object_new(BROWSER_TYPE_PANE, "param", param, NULL));
    return pane;
}

WebKitWebView *browser_pane_get_web_view(BrowserPane *pane)
{
    g_return_val_if_fail(BROWSER_IS_PANE(pane), NULL);

    return pane->webView;
}

void browser_pane_load_uri(BrowserPane *pane, const char *uri)
{
    g_return_if_fail(BROWSER_IS_PANE(pane));
    g_return_if_fail(uri);

    if (!g_str_has_prefix(uri, "javascript:")) {
        char *internalURI = getInternalURI(uri);
        webkit_web_view_load_uri(pane->webView, internalURI);
        g_free(internalURI);
        return;
    }

    webkit_web_view_run_javascript(pane->webView,
            strstr(uri, "javascript:"), NULL, NULL, NULL);
}

void browser_pane_set_status_text(BrowserPane *pane, const char *text)
{
}

void browser_pane_toggle_inspector(BrowserPane *pane)
{
}

void browser_pane_set_background_color(BrowserPane *pane, GAL_Color *rgba)
{
}

void browser_pane_start_search(BrowserPane *pane)
{
}

void browser_pane_stop_search(BrowserPane *pane)
{
}

void browser_pane_enter_fullscreen(BrowserPane *pane)
{
}

void browser_pane_leave_fullscreen(BrowserPane *pane)
{
}

