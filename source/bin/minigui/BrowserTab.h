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

#ifndef BrowserTab_h
#define BrowserTab_h

#include "BrowserPane.h"

G_BEGIN_DECLS

#define BROWSER_TYPE_TAB            (browser_tab_get_type())
#define BROWSER_TAB(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), BROWSER_TYPE_TAB, BrowserTab))
#define BROWSER_TAB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  BROWSER_TYPE_TAB, BrowserTabClass))
#define BROWSER_IS_TAB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), BROWSER_TYPE_TAB))
#define BROWSER_IS_TAB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  BROWSER_TYPE_TAB))
#define BROWSER_TAB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  BROWSER_TYPE_TAB, BrowserTabClass))

typedef struct _BrowserTab        BrowserTab;
typedef struct _BrowserTabClass   BrowserTabClass;

GType browser_tab_get_type(void);

BrowserTab* browser_tab_new(HWND psHwnd, WebKitWebViewParam *);
HWND browser_tab_get_title_widget(BrowserTab*);
int browser_tab_get_idx(BrowserTab*);
HWND browser_tab_get_page_hwnd(BrowserTab *tab);
HWND browser_tab_get_propsheet(BrowserTab *tab);

static inline
WebKitWebView* browser_tab_get_web_view(BrowserTab *tab)
{
    return browser_pane_get_web_view(BROWSER_PANE(tab));
}

static inline void
browser_tab_load_uri(BrowserTab *tab, const char* uri)
{
    browser_pane_load_uri(BROWSER_PANE(tab), uri);
}

static inline void
browser_tab_toggle_inspector(BrowserTab *tab)
{
    browser_pane_toggle_inspector(BROWSER_PANE(tab));
}

static inline void
browser_tab_set_background_color(BrowserTab* tab, GAL_Color* rgba)
{
    browser_pane_set_background_color(BROWSER_PANE(tab), rgba);
}

static inline void
browser_tab_set_status_text(BrowserTab *tab, const char* text)
{
    browser_pane_set_status_text(BROWSER_PANE(tab), text);
}

static inline void browser_tab_start_search(BrowserTab* tab)
{
    browser_pane_start_search(BROWSER_PANE(tab));
}

static inline void browser_tab_stop_search(BrowserTab *tab)
{
    browser_pane_stop_search(BROWSER_PANE(tab));
}

static inline void browser_tab_enter_fullscreen(BrowserTab *tab)
{
    browser_pane_enter_fullscreen(BROWSER_PANE(tab));
}

static inline void browser_tab_leave_fullscreen(BrowserTab *tab)
{
    browser_pane_leave_fullscreen(BROWSER_PANE(tab));
}

G_END_DECLS

#define BRW_TAB2VIEW(tab)  browser_tab_get_web_view((tab))

#endif
