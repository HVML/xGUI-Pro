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

    GtkWidget *searchBar;
    GtkWidget *statusLabel;
    gboolean wasSearchingWhenEnteredFullscreen;
    GtkWidget *fullScreenMessageLabel;
    guint fullScreenMessageLabelId;

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
    BrowserTab *tab = BROWSER_TAB(gObject);

    if (tab->fullScreenMessageLabelId)
        g_source_remove(tab->fullScreenMessageLabelId);

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

    tab->searchBar = gtk_search_bar_new();
    GtkWidget *searchBox = browser_search_box_new(BROWSER_PANE(tab)->webView);
    gtk_search_bar_set_show_close_button(GTK_SEARCH_BAR(tab->searchBar), TRUE);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_search_bar_set_child(GTK_SEARCH_BAR(tab->searchBar), searchBox);
    gtk_search_bar_connect_entry(GTK_SEARCH_BAR(tab->searchBar),
        GTK_EDITABLE(browser_search_box_get_entry(BROWSER_SEARCH_BOX(searchBox))));
    gtk_box_prepend(GTK_BOX(tab), tab->searchBar);
#else
    gtk_container_add(GTK_CONTAINER(tab->searchBar), searchBox);
    gtk_widget_show(searchBox);
    gtk_search_bar_connect_entry(GTK_SEARCH_BAR(tab->searchBar),
        browser_search_box_get_entry(BROWSER_SEARCH_BOX(searchBox)));
    gtk_container_add(GTK_CONTAINER(tab), tab->searchBar);
    gtk_widget_show(tab->searchBar);
#endif

    GtkWidget *overlay = BROWSER_PANE(tab)->overlay;

    tab->statusLabel = gtk_label_new(NULL);
    gtk_widget_set_halign(tab->statusLabel, GTK_ALIGN_START);
    gtk_widget_set_valign(tab->statusLabel, GTK_ALIGN_END);
    gtk_widget_set_margin_start(tab->statusLabel, 1);
    gtk_widget_set_margin_end(tab->statusLabel, 1);
    gtk_widget_set_margin_top(tab->statusLabel, 1);
    gtk_widget_set_margin_bottom(tab->statusLabel, 1);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), tab->statusLabel);

    tab->fullScreenMessageLabel = gtk_label_new(NULL);
    gtk_widget_set_halign(tab->fullScreenMessageLabel, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(tab->fullScreenMessageLabel, GTK_ALIGN_CENTER);
#if !GTK_CHECK_VERSION(3, 98, 0)
    gtk_widget_set_no_show_all(tab->fullScreenMessageLabel, TRUE);
#endif
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), tab->fullScreenMessageLabel);

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

void browser_tab_set_status_text(BrowserTab *tab, const char *text)
{
    g_return_if_fail(BROWSER_IS_TAB(tab));

    gtk_label_set_text(GTK_LABEL(tab->statusLabel), text);
    gtk_widget_set_visible(tab->statusLabel, !!text);
}

static gboolean browserTabIsSearchBarOpen(BrowserTab *tab)
{
#if GTK_CHECK_VERSION(3, 98, 5)
    GtkWidget *revealer = gtk_widget_get_first_child(tab->searchBar);
#else
    GtkWidget *revealer = gtk_bin_get_child(GTK_BIN(tab->searchBar));
#endif
    return gtk_revealer_get_reveal_child(GTK_REVEALER(revealer));
}

void browser_tab_start_search(BrowserTab *tab)
{
    g_return_if_fail(BROWSER_IS_TAB(tab));
    if (!browserTabIsSearchBarOpen(tab))
        gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(tab->searchBar), TRUE);
}

void browser_tab_stop_search(BrowserTab *tab)
{
    g_return_if_fail(BROWSER_IS_TAB(tab));
    if (browserTabIsSearchBarOpen(tab))
        gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(tab->searchBar), FALSE);
}

static gboolean fullScreenMessageTimeoutCallback(BrowserTab *tab)
{
    gtk_widget_hide(tab->fullScreenMessageLabel);
    tab->fullScreenMessageLabelId = 0;
    return FALSE;
}

void browser_tab_enter_fullscreen(BrowserTab *tab)
{
    g_return_if_fail(BROWSER_IS_TAB(tab));

    const gchar *titleOrURI = webkit_web_view_get_title(BROWSER_PANE(tab)->webView);
    if (!titleOrURI || !titleOrURI[0])
        titleOrURI = webkit_web_view_get_uri(BROWSER_PANE(tab)->webView);

    gchar *message = g_strdup_printf("%s is now full screen. Press ESC or f to exit.", titleOrURI);
    gtk_label_set_text(GTK_LABEL(tab->fullScreenMessageLabel), message);
    g_free(message);

    gtk_widget_show(tab->fullScreenMessageLabel);

    tab->fullScreenMessageLabelId = g_timeout_add_seconds(2, (GSourceFunc)fullScreenMessageTimeoutCallback, tab);
    g_source_set_name_by_id(tab->fullScreenMessageLabelId, "[WebKit] fullScreenMessageTimeoutCallback");

    tab->wasSearchingWhenEnteredFullscreen = browserTabIsSearchBarOpen(tab);
    browser_tab_stop_search(tab);
}

void browser_tab_leave_fullscreen(BrowserTab *tab)
{
    g_return_if_fail(BROWSER_IS_TAB(tab));

    if (tab->fullScreenMessageLabelId) {
        g_source_remove(tab->fullScreenMessageLabelId);
        tab->fullScreenMessageLabelId = 0;
    }

    gtk_widget_hide(tab->fullScreenMessageLabel);

    if (tab->wasSearchingWhenEnteredFullscreen) {
        /* Opening the search bar steals the focus. Usually, we want
         * this but not when coming back from fullscreen.
         */
#if GTK_CHECK_VERSION(3, 98, 5)
        GtkWindow *window = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(tab)));
#else
        GtkWindow *window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(tab)));
#endif
        GtkWidget *focusWidget = gtk_window_get_focus(window);
        browser_tab_start_search(tab);
        gtk_window_set_focus(window, focusWidget);
    }
}

