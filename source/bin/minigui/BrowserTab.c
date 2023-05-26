/*
 * Copyright (C) 2016 Igalia S.L.
** Copyright (C) 2022, 2023 FMSoft <http://www.fmsoft.cn>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif
#include "main.h"

#include "BrowserPane.h"
#include "BrowserTab.h"
#include "BrowserWindow.h"

#include <string.h>

enum {
    PROP_0,
    PROP_VIEW
};

struct _BrowserTab {
    BrowserPane parent;

    int idx;
    HWND psHwnd;
    HWND pageHwnd;
};

struct _BrowserTabClass {
    BrowserPaneClass parent;
};

G_DEFINE_TYPE(BrowserTab, browser_tab, BROWSER_TYPE_PANE)

static void browser_tab_init(BrowserTab *tab)
{
}

static void browserTabConstructed(GObject *gObject)
{
    //BrowserTab *tab = BROWSER_TAB(gObject);

    G_OBJECT_CLASS(browser_tab_parent_class)->constructed(gObject);
}

static void browserTabFinalize(GObject *gObject)
{
    G_OBJECT_CLASS(browser_tab_parent_class)->finalize(gObject);
}

static void browser_tab_class_init(BrowserTabClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = browserTabConstructed;
    gobjectClass->finalize = browserTabFinalize;
}

DLGTEMPLATE DlgWebView = {
    WS_BORDER | WS_CAPTION,
    WS_EX_NONE,
    0, 0, 100, 100,
    "",
    0, 0,
    0, NULL,
    0
};

static LRESULT tabPageProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefaultPageProc(hDlg, message, wParam, lParam);
}

/* Public API. */
HWND browser_tab_new(HWND psHwnd, WebKitWebViewParam *param)
{
    int idx = SendMessage(psHwnd, PSM_ADDPAGE, (WPARAM)&DlgWebView, (LPARAM)tabPageProc);
    HWND pageHwnd = (HWND) SendMessage(psHwnd, PSM_GETPAGE, (WPARAM)idx, 0);
    RECT rcPage;
    GetClientRect(pageHwnd, &rcPage);
    SetRect(&param->webViewRect, 0, 0, RECTW(rcPage), RECTH(rcPage));
    param->webViewParent = pageHwnd;

    BrowserTab *tab = BROWSER_TAB(g_object_new(BROWSER_TYPE_TAB, "param", param, NULL));
    tab->idx = idx;
    tab->psHwnd = psHwnd;
    tab->pageHwnd = pageHwnd;
    return tab->pageHwnd;
}

HWND browser_tab_get_title_widget(BrowserTab *tab)
{
    return NULL;
}

