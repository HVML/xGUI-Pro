/*
 * Copyright (C) 2016 Igalia S.L.
** Copyright (C) 2022 FMSoft <http://www.fmsoft.cn>
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
#include "BrowserSearchBox.h"
#include "BrowserWindow.h"

#include <string.h>

enum {
    PROP_0,
    PROP_VIEW
};

struct _BrowserTab {
    BrowserPane parent;

    /* Tab Title */
    GtkWidget *titleBox;
    GtkWidget *titleLabel;
    GtkWidget *titleSpinner;
    GtkWidget *titleAudioButton;
    GtkWidget *titleCloseButton;
};

struct _BrowserTabClass {
    BrowserPaneClass parent;
};

G_DEFINE_TYPE(BrowserTab, browser_tab, BROWSER_TYPE_PANE)

static void titleChanged(WebKitWebView *webView, GParamSpec *pspec, BrowserTab *tab)
{
    const char *title = webkit_web_view_get_title(webView);
    if (title && *title)
        gtk_label_set_text(GTK_LABEL(tab->titleLabel), title);
}

static void isLoadingChanged(WebKitWebView *webView, GParamSpec *paramSpec, BrowserTab *tab)
{
    if (webkit_web_view_is_loading(webView)) {
        gtk_spinner_start(GTK_SPINNER(tab->titleSpinner));
        gtk_widget_show(tab->titleSpinner);
    } else {
        gtk_spinner_stop(GTK_SPINNER(tab->titleSpinner));
        gtk_widget_hide(tab->titleSpinner);
    }
}

static void audioClicked(GtkButton *button, gpointer userData)
{
    BrowserTab *tab = BROWSER_TAB(userData);
    gboolean muted = webkit_web_view_get_is_muted(BROWSER_PANE(tab)->webView);

    webkit_web_view_set_is_muted(BROWSER_PANE(tab)->webView, !muted);
}

static void audioMutedChanged(WebKitWebView *webView, GParamSpec *pspec, gpointer userData)
{
    BrowserTab *tab = BROWSER_TAB(userData);
    gboolean muted = webkit_web_view_get_is_muted(BROWSER_PANE(tab)->webView);

#if GTK_CHECK_VERSION(3, 98, 4)
    gtk_button_set_icon_name(GTK_BUTTON(tab->titleAudioButton), muted ? "audio-volume-muted-symbolic" : "audio-volume-high-symbolic");
#else
    gtk_button_set_image(GTK_BUTTON(tab->titleAudioButton), gtk_image_new_from_icon_name(muted ? "audio-volume-muted-symbolic" : "audio-volume-high-symbolic", GTK_ICON_SIZE_MENU));
#endif
}

#if GTK_CHECK_VERSION(3, 98, 5)
static void tabCloseClicked(BrowserTab *tab)
{
    GtkWidget *parent = gtk_widget_get_parent(GTK_WIDGET(tab));
    while (parent && !GTK_IS_NOTEBOOK(parent))
        parent = gtk_widget_get_parent(parent);

    GtkNotebook *notebook = GTK_NOTEBOOK(parent);
    gtk_notebook_remove_page(notebook, gtk_notebook_page_num(notebook, GTK_WIDGET(tab)));
}
#endif

static void browserTabFinalize(GObject *gObject)
{
    G_OBJECT_CLASS(browser_tab_parent_class)->finalize(gObject);
}

static void browser_tab_init(BrowserTab *tab)
{
    gtk_orientable_set_orientation(GTK_ORIENTABLE(tab), GTK_ORIENTATION_VERTICAL);
}

static void browserTabConstructed(GObject *gObject)
{
    BrowserTab *tab = BROWSER_TAB(gObject);

    G_OBJECT_CLASS(browser_tab_parent_class)->constructed(gObject);

    tab->titleBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(hbox, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(hbox, TRUE);

    tab->titleSpinner = gtk_spinner_new();
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(hbox), tab->titleSpinner);
#else
    gtk_box_pack_start(GTK_BOX(hbox), tab->titleSpinner, FALSE, FALSE, 0);
#endif

    tab->titleLabel = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(tab->titleLabel), PANGO_ELLIPSIZE_END);
    gtk_label_set_single_line_mode(GTK_LABEL(tab->titleLabel), TRUE);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(hbox), tab->titleLabel);
#else
    gtk_box_pack_start(GTK_BOX(hbox), tab->titleLabel, FALSE, FALSE, 0);
    gtk_widget_show(tab->titleLabel);
#endif

#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(tab->titleBox), hbox);
#else
    gtk_box_pack_start(GTK_BOX(tab->titleBox), hbox, TRUE, TRUE, 0);
    gtk_widget_show(hbox);
#endif

#if GTK_CHECK_VERSION(3, 98, 5)
    tab->titleAudioButton = gtk_button_new_from_icon_name("audio-volume-high-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(tab->titleAudioButton), FALSE);
#else
    tab->titleAudioButton = gtk_button_new_from_icon_name("audio-volume-high-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(tab->titleAudioButton), GTK_RELIEF_NONE);
#endif
    g_signal_connect(tab->titleAudioButton, "clicked", G_CALLBACK(audioClicked), tab);
    gtk_widget_set_focus_on_click(tab->titleAudioButton, FALSE);

#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(tab->titleBox), tab->titleAudioButton);
#else
    gtk_box_pack_start(GTK_BOX(tab->titleBox), tab->titleAudioButton, FALSE, FALSE, 0);
#endif

#if GTK_CHECK_VERSION(3, 98, 5)
    tab->titleCloseButton = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_button_set_has_frame(GTK_BUTTON(tab->titleCloseButton), FALSE);
    g_signal_connect_swapped(tab->titleCloseButton, "clicked", G_CALLBACK(tabCloseClicked), tab);
#else
    tab->titleCloseButton = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(tab->titleCloseButton), GTK_RELIEF_NONE);
    g_signal_connect_swapped(tab->titleCloseButton, "clicked", G_CALLBACK(gtk_widget_destroy), tab);
#endif
    gtk_widget_set_focus_on_click(tab->titleCloseButton, FALSE);

#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(tab->titleBox), tab->titleCloseButton);
#else
    gtk_box_pack_start(GTK_BOX(tab->titleBox), tab->titleCloseButton, FALSE, FALSE, 0);
    gtk_widget_show(tab->titleCloseButton);
#endif

    g_signal_connect(BROWSER_PANE(tab)->webView, "notify::title", G_CALLBACK(titleChanged), tab);
    g_signal_connect(BROWSER_PANE(tab)->webView, "notify::is-loading", G_CALLBACK(isLoadingChanged), tab);

    g_object_bind_property(BROWSER_PANE(tab)->webView, "is-playing-audio", tab->titleAudioButton, "visible", G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_signal_connect(BROWSER_PANE(tab)->webView, "notify::is-muted", G_CALLBACK(audioMutedChanged), tab);
}

static void browser_tab_class_init(BrowserTabClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = browserTabConstructed;
    gobjectClass->finalize = browserTabFinalize;
}

/* Public API. */
GtkWidget *browser_tab_new(WebKitWebView *view)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(view), NULL);

    return GTK_WIDGET(g_object_new(BROWSER_TYPE_TAB, "view", view, NULL));
}

GtkWidget *browser_tab_get_title_widget(BrowserTab *tab)
{
    g_return_val_if_fail(BROWSER_IS_TAB(tab), NULL);

    return tab->titleBox;
}

