/*
** BrowserTabbedWindow.c -- The implementation of BrowserTabbedWindow.
**
** Copyright (C) 2022 FMSoft <http://www.fmsoft.cn>
**
** Author: Vincent Wei <https://github.com/VincentWei>
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

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif
#include "main.h"
#include "BrowserTabbedWindow.h"

#include "BrowserDownloadsBar.h"
#include "BrowserSettingsDialog.h"
#include "BrowserPane.h"
#include "BrowserTab.h"
#include "BrowserWindow.h"
#include <gdk/gdkkeysyms.h>
#include <string.h>

struct _BrowserTabbedWindow {
    GtkApplicationWindow parent;

    WebKitWebContext *webContext;

    GtkWidget *mainBox;
    GtkWidget *mainFixed;

    /* widgets for toolbar */
    GtkWidget *toolbar;
    GtkWidget *uriEntry;
    GtkWidget *backItem;
    GtkWidget *forwardItem;
    GtkWidget *reloadOrStopButton;

    GtkWidget *settingsDialog;

    unsigned nrViews;
    GSList *viewContainers;

    GtkWidget *notebook;
    BrowserTab *activeTab;

    GtkWidget *downloadsBar;
    gboolean searchBarVisible;
    gboolean fullScreenIsEnabled;
#if GTK_CHECK_VERSION(3, 98, 0)
    GdkTexture *favicon;
#else
    GdkPixbuf *favicon;
#endif
    GtkWindow *parentWindow;
    guint resetEntryProgressTimeoutId;

    gchar *name;
    gchar *title;
    gint width;
    gint height;

    GdkRGBA backgroundColor;
};

struct _BrowserTabbedWindowClass {
    GtkApplicationWindowClass parent;
};

static const gdouble minimumZoomLevel = 0.5;
static const gdouble maximumZoomLevel = 3;
static const gdouble defaultZoomLevel = 1;
static const gdouble zoomStep = 1.2;

G_DEFINE_TYPE(BrowserTabbedWindow, browser_tabbed_window, GTK_TYPE_APPLICATION_WINDOW)

static char *
getExternalURI(const char *uri)
{
    /* From the user point of view we support about: prefix. */
    if (uri && g_str_has_prefix(uri, BROWSER_ABOUT_SCHEME))
        return g_strconcat("about", uri + strlen(BROWSER_ABOUT_SCHEME), NULL);

    return g_strdup(uri);
}

static void
browserTabbedWindowSetStatusText(BrowserTabbedWindow *window, const char *text)
{
    browser_tab_set_status_text(window->activeTab, text);
}

static void
activateUriEntryCallback(BrowserTabbedWindow *window)
{
    browser_tabbed_window_load_uri(window, NULL,
#if GTK_CHECK_VERSION(3, 98, 0)
        gtk_editable_get_text(GTK_EDITABLE(window->uriEntry))
#else
        gtk_entry_get_text(GTK_ENTRY(window->uriEntry))
#endif
    );
}

static void
reloadOrStopCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(userData);
    WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
    if (webkit_web_view_is_loading(webView))
        webkit_web_view_stop_loading(webView);
    else
        webkit_web_view_reload(webView);
}

static void
goBackCallback(GSimpleAction *action, GVariant *parameter,
        gpointer userData)
{
    WebKitWebView *webView =
        BRW_TAB2VIEW(BROWSER_TABBED_WINDOW(userData)->activeTab);
    webkit_web_view_go_back(webView);
}

static void
goForwardCallback(GSimpleAction *action, GVariant *parameter,
        gpointer userData)
{
    WebKitWebView *webView =
        BRW_TAB2VIEW(BROWSER_TABBED_WINDOW(userData)->activeTab);
    webkit_web_view_go_forward(webView);
}

static void
settingsCallback(GSimpleAction *action, GVariant *parameter,
        gpointer userData)
{
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(userData);
    if (window->settingsDialog) {
        gtk_window_present(GTK_WINDOW(window->settingsDialog));
        return;
    }

    WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
    window->settingsDialog =
        browser_settings_dialog_new(webkit_web_view_get_settings(webView));
    gtk_window_set_transient_for(GTK_WINDOW(window->settingsDialog),
            GTK_WINDOW(window));
    g_object_add_weak_pointer(G_OBJECT(window->settingsDialog),
            (gpointer *)&window->settingsDialog);
    gtk_widget_show(window->settingsDialog);
}

static void
webViewURIChanged(WebKitWebView *webView, GParamSpec *pspec,
        BrowserTabbedWindow *window)
{
    char *externalURI = getExternalURI(webkit_web_view_get_uri(webView));
#if GTK_CHECK_VERSION(3, 98, 0)
    gtk_editable_set_text(GTK_EDITABLE(window->uriEntry),
            externalURI ? externalURI : "");
#else
    gtk_entry_set_text(GTK_ENTRY(window->uriEntry),
            externalURI ? externalURI : "");
#endif
    g_free(externalURI);
}

static void
webViewTitleChanged(WebKitWebView *webView, GParamSpec *pspec,
        BrowserTabbedWindow *window)
{
    const char *title = webkit_web_view_get_title(webView);
    if (!title)
        title = window->title ? window->title : BROWSER_DEFAULT_TITLE;
    char *privateTitle = NULL;
    if (webkit_web_view_is_controlled_by_automation(webView))
        privateTitle = g_strdup_printf("[Automation] %s", title);
    else if (webkit_web_view_is_ephemeral(webView))
        privateTitle = g_strdup_printf("[Private] %s", title);
    gtk_window_set_title(GTK_WINDOW(window),
            privateTitle ? privateTitle : title);
    g_free(privateTitle);
}

static gboolean
resetEntryProgress(BrowserTabbedWindow *window)
{
    gtk_entry_set_progress_fraction(GTK_ENTRY(window->uriEntry), 0);
    window->resetEntryProgressTimeoutId = 0;
    return FALSE;
}

static void
webViewLoadProgressChanged(WebKitWebView *webView, GParamSpec *pspec,
        BrowserTabbedWindow *window)
{
    gdouble progress = webkit_web_view_get_estimated_load_progress(webView);
    gtk_entry_set_progress_fraction(GTK_ENTRY(window->uriEntry), progress);
    if (progress == 1.0) {
        window->resetEntryProgressTimeoutId =
            g_timeout_add(500, (GSourceFunc)resetEntryProgress, window);
        g_source_set_name_by_id(window->resetEntryProgressTimeoutId,
                "[WebKit] resetEntryProgress");
    }
    else if (window->resetEntryProgressTimeoutId) {
        g_source_remove(window->resetEntryProgressTimeoutId);
        window->resetEntryProgressTimeoutId = 0;
    }
}

static void
downloadStarted(WebKitWebContext *webContext, WebKitDownload *download,
        BrowserTabbedWindow *window)
{
#if !GTK_CHECK_VERSION(3, 98, 0)
    if (!window->downloadsBar) {
        window->downloadsBar = browser_downloads_bar_new();
        gtk_box_pack_start(GTK_BOX(window->mainBox), window->downloadsBar,
                FALSE, FALSE, 0);
        gtk_box_reorder_child(GTK_BOX(window->mainBox),
                window->downloadsBar, 0);
        g_object_add_weak_pointer(G_OBJECT(window->downloadsBar),
                (gpointer *)&(window->downloadsBar));
        gtk_widget_show(window->downloadsBar);
    }
    browser_downloads_bar_add_download(
            BROWSER_DOWNLOADS_BAR(window->downloadsBar), download);
#endif
}

static void
browserTabbedWindowHistoryItemActivated(BrowserTabbedWindow *window,
        GVariant *parameter, GAction *action)
{
    WebKitBackForwardListItem *item =
        g_object_get_data(G_OBJECT(action), "back-forward-list-item");
    if (!item)
        return;

    WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
    webkit_web_view_go_to_back_forward_list_item(webView, item);
}

static void
browserTabbedWindowCreateBackForwardMenu(BrowserTabbedWindow *window,
        GList *list, gboolean isBack)
{
    if (!list)
        return;

    static guint64 actionId = 0;

    GSimpleActionGroup *actionGroup = g_simple_action_group_new();
    GMenu *menu = g_menu_new();
    GList *listItem;
    for (listItem = list; listItem; listItem = g_list_next(listItem)) {
        WebKitBackForwardListItem *item =
            (WebKitBackForwardListItem *)listItem->data;
        const char *title = webkit_back_forward_list_item_get_title(item);
        if (!title || !*title)
            title = webkit_back_forward_list_item_get_uri(item);

        char *displayTitle;
#define MAX_TITLE 100
#if GTK_CHECK_VERSION(3, 98, 5)
        char *escapedTitle = g_markup_escape_text(title, -1);
        if (strlen(escapedTitle) > MAX_TITLE) {
            displayTitle = g_strndup(escapedTitle, MAX_TITLE);
            g_free(escapedTitle);
            displayTitle[MAX_TITLE - 1] = 0xA6;
            displayTitle[MAX_TITLE - 2] = 0x80;
            displayTitle[MAX_TITLE - 3] = 0xE2;
        } else
            displayTitle = escapedTitle;
#else
        displayTitle = g_strndup(title, MIN(MAX_TITLE, strlen(title)));
        if (strlen(title) > MAX_TITLE) {
            displayTitle[MAX_TITLE - 1] = 0xA6;
            displayTitle[MAX_TITLE - 2] = 0x80;
            displayTitle[MAX_TITLE - 3] = 0xE2;
        }
#endif
#undef MAX_TITLE

        char *actionName = g_strdup_printf("action-%lu", ++actionId);
        GSimpleAction *action = g_simple_action_new(actionName, NULL);
        g_object_set_data_full(G_OBJECT(action), "back-forward-list-item",
                g_object_ref(item), g_object_unref);
        g_signal_connect_swapped(action, "activate",
                G_CALLBACK(browserTabbedWindowHistoryItemActivated), window);
        g_action_map_add_action(G_ACTION_MAP(actionGroup), G_ACTION(action));
        g_object_unref(action);

        char *detailedActionName = g_strdup_printf("%s.%s",
                isBack ? "bf-back" : "bf-forward", actionName);
        GMenuItem *menuItem = g_menu_item_new(displayTitle, detailedActionName);
        g_menu_append_item(menu, menuItem);
        g_object_unref(menuItem);

        g_free(displayTitle);
        g_free(detailedActionName);
        g_free(actionName);
    }

#if GTK_CHECK_VERSION(3, 98, 5)
    GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
#else
    GtkWidget *popover = gtk_popover_menu_new();
    gtk_popover_bind_model(GTK_POPOVER(popover), G_MENU_MODEL(menu), NULL);
#endif
    g_object_unref(menu);
    gtk_widget_insert_action_group(popover,
            isBack ? "bf-back" : "bf-forward", G_ACTION_GROUP(actionGroup));
    g_object_unref(actionGroup);

    GtkWidget *button = isBack ? window->backItem : window->forwardItem;
#if GTK_CHECK_VERSION(3, 98, 5)
    g_object_set_data_full(G_OBJECT(button), "history-popover",
            popover, (GDestroyNotify)gtk_widget_unparent);
    gtk_widget_set_parent(popover, button);
#else
    gtk_popover_set_relative_to(GTK_POPOVER(popover), button);
    g_object_set_data(G_OBJECT(button), "history-popover", popover);
#endif
    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);
}

static void
browserTabbedWindowUpdateNavigationMenu(BrowserTabbedWindow *window,
        WebKitBackForwardList *backForwardlist)
{
    WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
    GAction *action =
        g_action_map_lookup_action(G_ACTION_MAP(window), "go-back");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action),
            webkit_web_view_can_go_back(webView));
    action = g_action_map_lookup_action(G_ACTION_MAP(window), "go-forward");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action),
            webkit_web_view_can_go_forward(webView));

    GList *list = g_list_reverse(
            webkit_back_forward_list_get_back_list_with_limit(backForwardlist, 10));
    browserTabbedWindowCreateBackForwardMenu(window, list, TRUE);
    g_list_free(list);

    list = webkit_back_forward_list_get_forward_list_with_limit(backForwardlist, 10);
    browserTabbedWindowCreateBackForwardMenu(window, list, FALSE);
    g_list_free(list);
}

#if GTK_CHECK_VERSION(3, 98, 5)
static void
navigationButtonPressed(GtkGestureClick *gesture, guint clickCount,
        double x, double y)
{
    GdkEventSequence *sequence =
        gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
    gtk_gesture_set_sequence_state(GTK_GESTURE(gesture),
            sequence, GTK_EVENT_SEQUENCE_CLAIMED);

    GtkWidget *button =
        gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    GtkWidget *popover = g_object_get_data(G_OBJECT(button), "history-popover");
    if (popover)
        gtk_popover_popup(GTK_POPOVER(popover));
}
#else
static gboolean
navigationButtonPressCallback(GtkButton *button, GdkEvent *event,
        BrowserTabbedWindow *window)
{
    guint eventButton;
    gdk_event_get_button(event, &eventButton);
    if (eventButton != GDK_BUTTON_SECONDARY)
        return GDK_EVENT_PROPAGATE;

    GtkWidget *popover = g_object_get_data(G_OBJECT(button), "history-popover");
    if (!popover)
        return GDK_EVENT_PROPAGATE;

    gtk_popover_popup(GTK_POPOVER(popover));

    return GDK_EVENT_STOP;
}
#endif

static void
browserTabbedWindowTryCloseCurrentWebView(GSimpleAction *action,
        GVariant *parameter, gpointer userData)
{
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(userData);
    int currentPage =
        gtk_notebook_get_current_page(GTK_NOTEBOOK(window->notebook));
    BrowserTab *tab = (BrowserTab *)gtk_notebook_get_nth_page(
            GTK_NOTEBOOK(window->notebook), currentPage);
    webkit_web_view_try_close(BRW_TAB2VIEW(tab));
}

static void collectPane(BrowserPane* pane, GSList **webViews)
{
    *webViews = g_slist_prepend(*webViews, BRW_PANE2VIEW(pane));
}

static void
containerTryClose(BrowserTabbedWindow *window, GtkWidget *container)
{
    GSList *webViews = NULL;

    if (GTK_IS_NOTEBOOK(container)) {
        GtkNotebook *notebook = GTK_NOTEBOOK(container);
        int n = gtk_notebook_get_n_pages(notebook);
        for (int i = 0; i < n; ++i) {
            BrowserTab *tab;
            tab = (BrowserTab *)gtk_notebook_get_nth_page(notebook, i);
            webViews = g_slist_prepend(webViews, browser_tab_get_web_view(tab));
        }
    }
    else if (GTK_IS_FIXED(container)) {
#if GTK_CHECK_VERSION(4, 0, 0)
        GtkWidget *pane = gtk_widget_get_first_child(container);
        while (pane) {
            collectPane(BROWSER_PANE(pane), &webViews);
            pane = gtk_widget_get_next_sibling(pane);
        }
#else
        gtk_container_foreach(GTK_CONTAINER(container),
                (GtkCallback)collectPane, &webViews);
#endif
    }

    if (webViews) {
        GSList *link;
        for (link = webViews; link; link = link->next)
            webkit_web_view_try_close(link->data);

        g_slist_free(webViews);
    }
}

static void
windowTryClose(BrowserTabbedWindow *window)
{
    if (window->viewContainers) {
        GSList *link;
        for (link = window->viewContainers; link; link = link->next)
            containerTryClose(window, link->data);
    }
}

static void
browserTabbedWindowTryClose(GSimpleAction *action, GVariant *parameter,
        gpointer userData)
{
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(userData);

    windowTryClose(window);
}

static void
backForwardlistChanged(WebKitBackForwardList *backForwardlist,
        WebKitBackForwardListItem *itemAdded, GList *itemsRemoved,
        BrowserTabbedWindow *window)
{
    browserTabbedWindowUpdateNavigationMenu(window, backForwardlist);
}

#if !GTK_CHECK_VERSION(4, 0, 0)
static void removePane(BrowserPane* pane, WebKitWebView *webView)
{
    if (BRW_PANE2VIEW(pane) == webView) {
        gtk_widget_destroy(GTK_WIDGET(pane));
    }
}
#endif

static BrowserTabbedWindow *get_main_window(GtkWidget *widget)
{
    BrowserTabbedWindow *main_win = NULL;

    while (widget) {
        if (BROWSER_IS_TABBED_WINDOW(widget)) {
            main_win = BROWSER_TABBED_WINDOW(widget);
            break;
        }

        widget = gtk_widget_get_parent(widget);
    }

    return main_win;
}

static void
webViewClose(WebKitWebView *webView, GtkWidget *container)
{
    BrowserTabbedWindow *window = get_main_window(container);
    if (window->nrViews == 1) {
#if GTK_CHECK_VERSION(3, 98, 4)
        gtk_window_destroy(GTK_WINDOW(window));
#else
        gtk_widget_destroy(GTK_WIDGET(window));
#endif
        return;
    }

    if (GTK_IS_NOTEBOOK(container)) {
        int i;
        int tabsCount = gtk_notebook_get_n_pages(GTK_NOTEBOOK(container));
        for (i = 0; i < tabsCount; ++i) {
            BrowserTab *tab = (BrowserTab *)gtk_notebook_get_nth_page(
                    GTK_NOTEBOOK(container), i);
            if (BRW_TAB2VIEW(tab) == webView) {
#if GTK_CHECK_VERSION(3, 98, 4)
                gtk_notebook_remove_page(GTK_NOTEBOOK(container), i);
#else
                gtk_widget_destroy(GTK_WIDGET(tab));
#endif
                break;
            }
        }
    }
    else if (GTK_IS_FIXED(container)) {
#if GTK_CHECK_VERSION(4, 0, 0)
        GtkWidget *pane = gtk_widget_get_first_child(container);
        while (pane) {
            if (BRW_PANE2VIEW(pane) == webView) {
#if GTK_CHECK_VERSION(3, 98, 4)
                gtk_box_remove(GTK_FIXED(container), pane);
#else
                gtk_widget_destroy(GTK_WIDGET(pane));
#endif
                break;
            }

            pane = gtk_widget_get_next_sibling(pane);
        }
#else
        gtk_container_foreach(GTK_CONTAINER(container),
                (GtkCallback)removePane, webView);
#endif
    }
    else {
        g_warning("ODD widget (%p, %s)",
                container, gtk_widget_get_name(container));
        return;
    }

    window->nrViews--;
}

static gboolean
webViewEnterFullScreen(WebKitWebView *webView, BrowserTabbedWindow *window)
{
    if (window->toolbar)
        gtk_widget_hide(window->toolbar);
    browser_tab_enter_fullscreen(window->activeTab);
    return FALSE;
}

static gboolean
webViewLeaveFullScreen(WebKitWebView *webView, BrowserTabbedWindow *window)
{
    browser_tab_leave_fullscreen(window->activeTab);
    if (window->toolbar)
        gtk_widget_show(window->toolbar);
    return FALSE;
}

static gboolean
webViewLoadFailed(WebKitWebView *webView, WebKitLoadEvent loadEvent,
        const char *failingURI, GError *error, BrowserTabbedWindow *window)
{
    gtk_entry_set_progress_fraction(GTK_ENTRY(window->uriEntry), 0.);
    return FALSE;
}

static gboolean
webViewDecidePolicy(WebKitWebView *webView, WebKitPolicyDecision *decision,
        WebKitPolicyDecisionType decisionType, BrowserTabbedWindow *window)
{
    if (decisionType != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
        return FALSE;

    WebKitNavigationAction *navigationAction =
        webkit_navigation_policy_decision_get_navigation_action(
                WEBKIT_NAVIGATION_POLICY_DECISION(decision));
    if (webkit_navigation_action_get_navigation_type(navigationAction) !=
                WEBKIT_NAVIGATION_TYPE_LINK_CLICKED ||
            webkit_navigation_action_get_mouse_button(navigationAction) !=
                GDK_BUTTON_MIDDLE)
        return FALSE;

    /* Multiple tabs are not allowed in editor mode. */
    if (webkit_web_view_is_editable(webView))
        return FALSE;

    /* Opening a new tab if link clicked with the middle button. */
    WebKitWebView *newWebView = WEBKIT_WEB_VIEW(
            g_object_new(WEBKIT_TYPE_WEB_VIEW,
                "web-context", webkit_web_view_get_context(webView),
                "settings", webkit_web_view_get_settings(webView),
                "user-content-manager",
                    webkit_web_view_get_user_content_manager(webView),
                "is-controlled-by-automation",
                    webkit_web_view_is_controlled_by_automation(webView),
                "website-policies",
                    webkit_web_view_get_website_policies(webView),
        NULL));
    browser_tabbed_window_append_view_tab(window, NULL, newWebView);
    webkit_web_view_load_request(newWebView,
            webkit_navigation_action_get_request(navigationAction));

    webkit_policy_decision_ignore(decision);
    return TRUE;
}

static void
webViewMouseTargetChanged(WebKitWebView *webView,
        WebKitHitTestResult *hitTestResult, guint mouseModifiers,
        BrowserTabbedWindow *window)
{
    if (!webkit_hit_test_result_context_is_link(hitTestResult)) {
        browserTabbedWindowSetStatusText(window, NULL);
        return;
    }

    browserTabbedWindowSetStatusText(window,
            webkit_hit_test_result_get_link_uri(hitTestResult));
}

static gboolean
browserTabbedWindowCanZoomIn(BrowserTabbedWindow *window)
{
    WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
    gdouble zoomLevel = webkit_web_view_get_zoom_level(webView) * zoomStep;
    return zoomLevel < maximumZoomLevel;
}

static gboolean
browserTabbedWindowCanZoomOut(BrowserTabbedWindow *window)
{
    WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
    gdouble zoomLevel = webkit_web_view_get_zoom_level(webView) / zoomStep;
    return zoomLevel > minimumZoomLevel;
}

static gboolean
browserTabbedWindowCanZoomDefault(BrowserTabbedWindow *window)
{
    WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
    return webkit_web_view_get_zoom_level(webView) != 1.0;
}

static gboolean
browserTabbedWindowZoomIn(BrowserTabbedWindow *window)
{
    if (browserTabbedWindowCanZoomIn(window)) {
        WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
        gdouble zoomLevel = webkit_web_view_get_zoom_level(webView) * zoomStep;
        webkit_web_view_set_zoom_level(webView, zoomLevel);
        return TRUE;
    }
    return FALSE;
}

static gboolean
browserTabbedWindowZoomOut(BrowserTabbedWindow *window)
{
    if (browserTabbedWindowCanZoomOut(window)) {
        WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
        gdouble zoomLevel = webkit_web_view_get_zoom_level(webView) / zoomStep;
        webkit_web_view_set_zoom_level(webView, zoomLevel);
        return TRUE;
    }
    return FALSE;
}

#if GTK_CHECK_VERSION(3, 98, 5)
static gboolean
scrollEventCallback(BrowserTabbedWindow *window, double deltaX, double deltaY,
        GtkEventController *controller)
{
    GdkModifierType mod = gtk_accelerator_get_default_mod_mask();
    GdkEvent *event = gtk_event_controller_get_current_event(controller);
    if ((gdk_event_get_modifier_state(event) & mod) != GDK_CONTROL_MASK)
        return GDK_EVENT_PROPAGATE;

    return deltaY < 0 ? browserTabbedWindowZoomIn(window) :
        browserTabbedWindowZoomOut(window);
}
#else
static gboolean
scrollEventCallback(WebKitWebView *webView, const GdkEventScroll *event,
        BrowserTabbedWindow *window)
{
    GdkModifierType mod = gtk_accelerator_get_default_mod_mask();

    if ((event->state & mod) != GDK_CONTROL_MASK)
        return FALSE;

    if (event->delta_y < 0)
        return browserTabbedWindowZoomIn(window);

    return browserTabbedWindowZoomOut(window);
}
#endif

static void
browserTabbedWindowUpdateZoomActions(BrowserTabbedWindow *window)
{
    GAction *action =
        g_action_map_lookup_action(G_ACTION_MAP(window), "zoom-in");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action),
            browserTabbedWindowCanZoomIn(window));
    action = g_action_map_lookup_action(G_ACTION_MAP(window), "zoom-out");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action),
            browserTabbedWindowCanZoomOut(window));
    action = g_action_map_lookup_action(G_ACTION_MAP(window), "zoom-default");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action),
            browserTabbedWindowCanZoomDefault(window));
}

static void
webViewZoomLevelChanged(GObject *object, GParamSpec *paramSpec,
        BrowserTabbedWindow *window)
{
    browserTabbedWindowUpdateZoomActions(window);
}

static void
updateUriEntryIcon(BrowserTabbedWindow *window)
{
    GtkEntry *entry = GTK_ENTRY(window->uriEntry);
    if (window->favicon)
#if GTK_CHECK_VERSION(3, 98, 0)
        gtk_entry_set_icon_from_paintable(entry, GTK_ENTRY_ICON_PRIMARY,
                GDK_PAINTABLE(window->favicon));
#else
        gtk_entry_set_icon_from_pixbuf(entry, GTK_ENTRY_ICON_PRIMARY,
                window->favicon);
#endif
    else
        gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_PRIMARY,
                "document-new");
}

static void
faviconChanged(WebKitWebView *webView, GParamSpec *paramSpec,
        BrowserTabbedWindow *window)
{
#if GTK_CHECK_VERSION(3, 98, 0)
    GdkTexture *favicon = NULL;
#else
    GdkPixbuf *favicon = NULL;
#endif
    cairo_surface_t *surface = webkit_web_view_get_favicon(webView);

    if (surface) {
        int width = cairo_image_surface_get_width(surface);
        int height = cairo_image_surface_get_height(surface);
#if GTK_CHECK_VERSION(3, 98, 0)
        int stride = cairo_image_surface_get_stride(surface);
        GBytes *bytes =
            g_bytes_new(cairo_image_surface_get_data(surface), stride * height);
        favicon = gdk_memory_texture_new(width, height, GDK_MEMORY_DEFAULT,
                bytes, stride);
        g_bytes_unref(bytes);
#else
        favicon = gdk_pixbuf_get_from_surface(surface, 0, 0, width, height);
#endif
    }

    if (window->favicon)
        g_object_unref(window->favicon);
    window->favicon = favicon;

    updateUriEntryIcon(window);
}

static void
webViewMediaCaptureStateChanged(WebKitWebView* webView, GParamSpec* paramSpec,
        BrowserTabbedWindow* window)
{
    const gchar* name = g_param_spec_get_name(paramSpec);
    // FIXME: the URI entry is not great storage in case more than one capture device is in use,
    // because it can store only one secondary icon.
    GtkEntry *entry = GTK_ENTRY(window->uriEntry);

    if (g_str_has_prefix(name, "microphone")) {
        switch (webkit_web_view_get_microphone_capture_state(webView)) {
        case WEBKIT_MEDIA_CAPTURE_STATE_NONE:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY,
                    NULL);
            break;
        case WEBKIT_MEDIA_CAPTURE_STATE_MUTED:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY,
                    "microphone-sensivity-mutes-symbolic");
            break;
        case WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY,
                    "audio-input-microphone-symbolic");
            break;
        }
    } else if (g_str_has_prefix(name, "camera")) {
        switch (webkit_web_view_get_camera_capture_state(webView)) {
        case WEBKIT_MEDIA_CAPTURE_STATE_NONE:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY,
                    NULL);
            break;
        case WEBKIT_MEDIA_CAPTURE_STATE_MUTED:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY,
                    "camera-disabled-symbolic");
            break;
        case WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY,
                    "camera-web-symbolic");
            break;
        }
    } else if (g_str_has_prefix(name, "display")) {
        switch (webkit_web_view_get_display_capture_state(webView)) {
        case WEBKIT_MEDIA_CAPTURE_STATE_NONE:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY,
                    NULL);
            break;
        case WEBKIT_MEDIA_CAPTURE_STATE_MUTED:
            // FIXME: I found no suitable icon for this.
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY,
                    "media-playback-stop-symbolic");
            break;
        case WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY,
                    "video-display-symbolic");
            break;
        }
    }
}

static void
webViewUriEntryIconPressed(GtkEntry* entry, GtkEntryIconPosition position,
        GdkEvent* event, BrowserTabbedWindow* window)
{
    if (position != GTK_ENTRY_ICON_SECONDARY)
        return;

    // FIXME: What about audio/video?
    WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
    switch (webkit_web_view_get_display_capture_state(webView)) {
    case WEBKIT_MEDIA_CAPTURE_STATE_NONE:
        break;
    case WEBKIT_MEDIA_CAPTURE_STATE_MUTED:
        webkit_web_view_set_display_capture_state(webView,
                WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE);
        break;
    case WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE:
        webkit_web_view_set_display_capture_state(webView,
                WEBKIT_MEDIA_CAPTURE_STATE_MUTED);
        break;
    }
}

static void
webViewIsLoadingChanged(WebKitWebView *webView, GParamSpec *paramSpec,
        BrowserTabbedWindow *window)
{
    gboolean isLoading = webkit_web_view_is_loading(webView);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_button_set_icon_name(GTK_BUTTON(window->reloadOrStopButton),
            isLoading ? "process-stop-symbolic" : "view-refresh-symbolic");
#else
    GtkWidget *image =
        gtk_button_get_image(GTK_BUTTON(window->reloadOrStopButton));
    g_object_set(image, "icon-name",
            isLoading ? "process-stop-symbolic" : "view-refresh-symbolic",
            NULL);
#endif
}

static void
zoomInCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    browserTabbedWindowZoomIn(BROWSER_TABBED_WINDOW(userData));
}

static void
zoomOutCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    browserTabbedWindowZoomOut(BROWSER_TABBED_WINDOW(userData));
}

static void
defaultZoomCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    WebKitWebView *webView = BRW_TAB2VIEW(BROWSER_TABBED_WINDOW(userData)->activeTab);
    webkit_web_view_set_zoom_level(webView, defaultZoomLevel);
}

static void
searchCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    browser_tab_start_search(BROWSER_TABBED_WINDOW(userData)->activeTab);
}

static void
newTabCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(userData);
    WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
    if (webkit_web_view_is_editable(webView))
        return;

    browser_tabbed_window_append_view_tab(window, NULL,
            WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", webkit_web_view_get_context(webView),
        "settings", webkit_web_view_get_settings(webView),
        "user-content-manager",
            webkit_web_view_get_user_content_manager(webView),
        "is-controlled-by-automation",
            webkit_web_view_is_controlled_by_automation(webView),
        "website-policies", webkit_web_view_get_website_policies(webView),
        NULL)));
    gtk_widget_grab_focus(window->uriEntry);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(window->notebook), -1);
}

static void
toggleWebInspector(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    browser_tab_toggle_inspector(BROWSER_TABBED_WINDOW(userData)->activeTab);
}

static void
openPrivateWindow(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(userData);
    WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
    WebKitWebView *newWebView =
        WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
            "web-context", webkit_web_view_get_context(webView),
            "settings", webkit_web_view_get_settings(webView),
            "user-content-manager",
                webkit_web_view_get_user_content_manager(webView),
            "is-ephemeral", TRUE,
            "is-controlled-by-automation",
                webkit_web_view_is_controlled_by_automation(webView),
            "website-policies", webkit_web_view_get_website_policies(webView),
        NULL));

    GtkWidget *newWindow =
        browser_window_new(GTK_WINDOW(window), window->webContext);
    gtk_window_set_application(GTK_WINDOW(newWindow),
            gtk_window_get_application(GTK_WINDOW(window)));
    browser_tabbed_window_append_view_tab(BROWSER_TABBED_WINDOW(newWindow),
            NULL, newWebView);
    gtk_widget_grab_focus(GTK_WIDGET(newWebView));
    gtk_widget_show(GTK_WIDGET(newWindow));
}

static void
focusLocationBar(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    gtk_widget_grab_focus(BROWSER_TABBED_WINDOW(userData)->uriEntry);
}

static void
reloadPage(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    WebKitWebView *webView =
        BRW_TAB2VIEW(BROWSER_TABBED_WINDOW(userData)->activeTab);
    webkit_web_view_reload(webView);
}

static void
reloadPageIgnoringCache(GSimpleAction *action,
        GVariant *parameter, gpointer userData)
{
    WebKitWebView *webView =
        BRW_TAB2VIEW(BROWSER_TABBED_WINDOW(userData)->activeTab);
    webkit_web_view_reload_bypass_cache(webView);
}

static void
stopPageLoad(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(userData);
    browser_tab_stop_search(window->activeTab);
    WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
    if (webkit_web_view_is_loading(webView))
        webkit_web_view_stop_loading(webView);
}

static void
loadHomePage(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(userData);
    WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
    webkit_web_view_load_uri(webView, BROWSER_DEFAULT_URL);
}

static void
toggleFullScreen(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(userData);
    if (!window->fullScreenIsEnabled) {
        gtk_window_fullscreen(GTK_WINDOW(window));
        if (window->toolbar)
            gtk_widget_hide(window->toolbar);
        window->fullScreenIsEnabled = TRUE;
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(window));
        if (window->toolbar)
            gtk_widget_show(window->toolbar);
        window->fullScreenIsEnabled = FALSE;
    }
}

static void
webKitPrintOperationFailedCallback(WebKitPrintOperation *printOperation,
        GError *error)
{
    g_warning("Print failed: '%s'", error->message);
}

static void
printPage(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(userData);
    WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
    WebKitPrintOperation *printOperation = webkit_print_operation_new(webView);

    g_signal_connect(printOperation, "failed",
            G_CALLBACK(webKitPrintOperationFailedCallback), NULL);
    webkit_print_operation_run_dialog(printOperation, GTK_WINDOW(window));
    g_object_unref(printOperation);
}

static void browserTabbedWindowFinalize(GObject *gObject)
{
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(gObject);

    g_signal_handlers_disconnect_matched(window->webContext,
            G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);
    g_object_unref(window->webContext);

    if (window->name) {
        g_free(window->name);
        window->name = NULL;
    }

    if (window->title) {
        g_free(window->title);
        window->title = NULL;
    }

    if (window->favicon) {
        g_object_unref(window->favicon);
        window->favicon = NULL;
    }

    if (window->resetEntryProgressTimeoutId)
        g_source_remove(window->resetEntryProgressTimeoutId);

    G_OBJECT_CLASS(browser_tabbed_window_parent_class)->finalize(gObject);
}

static void browserTabbedWindowDispose(GObject *gObject)
{
    BrowserTabbedWindow *window = BROWSER_TABBED_WINDOW(gObject);

    if (window->parentWindow) {
        g_object_remove_weak_pointer(G_OBJECT(window->parentWindow),
                (gpointer *)&window->parentWindow);
        window->parentWindow = NULL;
    }

#if GTK_CHECK_VERSION(3, 98, 5)
    g_object_set_data(G_OBJECT(window->backItem), "history-popover", NULL);
    g_object_set_data(G_OBJECT(window->forwardItem), "history-popover", NULL);
#endif

    G_OBJECT_CLASS(browser_tabbed_window_parent_class)->dispose(gObject);
}

typedef enum {
    TOOLBAR_BUTTON_NORMAL,
    TOOLBAR_BUTTON_TOGGLE,
    TOOLBAR_BUTTON_MENU
} ToolbarButtonType;

static GtkWidget *
addToolbarButton(GtkWidget *box, ToolbarButtonType type,
        const char *iconName, const char *actionName)
{
    GtkWidget *button;
    switch (type) {
    case TOOLBAR_BUTTON_NORMAL:
#if GTK_CHECK_VERSION(3, 98, 5)
        button = gtk_button_new_from_icon_name(iconName);
#else
        button = gtk_button_new_from_icon_name(iconName, GTK_ICON_SIZE_MENU);
#endif
        break;
    case TOOLBAR_BUTTON_TOGGLE:
        button = gtk_toggle_button_new();
#if GTK_CHECK_VERSION(3, 98, 5)
        gtk_button_set_icon_name(GTK_BUTTON(button), iconName);
#else
        gtk_button_set_image(GTK_BUTTON(button),
                gtk_image_new_from_icon_name(iconName, GTK_ICON_SIZE_MENU));
#endif
        break;
    case TOOLBAR_BUTTON_MENU:
        button = gtk_menu_button_new();
#if GTK_CHECK_VERSION(3, 98, 5)
        gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(button), iconName);
#else
        gtk_button_set_image(GTK_BUTTON(button),
                gtk_image_new_from_icon_name(iconName, GTK_ICON_SIZE_MENU));
#endif

        break;
    }

    gtk_widget_set_focus_on_click(button, FALSE);
    if (actionName)
        gtk_actionable_set_action_name(GTK_ACTIONABLE(button), actionName);

#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(box), button);
#else
    gtk_container_add(GTK_CONTAINER(box), button);
    gtk_widget_show(button);
#endif

    return button;
}

static void browserTabbedWindowSwitchTab(GtkNotebook *notebook,
        BrowserTab *tab, guint tabIndex, BrowserTabbedWindow *window)
{
    if (window->activeTab == tab)
        return;

    if (window->activeTab) {
        browser_tab_set_status_text(window->activeTab, NULL);
        g_clear_object(&window->favicon);

        WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
        g_signal_handlers_disconnect_by_data(webView, window);

        /* We always want close to be connected even for not active tabs */
        g_signal_connect_after(webView, "close",
                G_CALLBACK(webViewClose), window);

        WebKitBackForwardList *backForwardlist =
            webkit_web_view_get_back_forward_list(webView);
        g_signal_handlers_disconnect_by_data(backForwardlist, window);
    }

    window->activeTab = tab;

    WebKitWebView *webView = BRW_TAB2VIEW(window->activeTab);
    if (webkit_web_view_is_editable(webView)) {
        g_warning("editable webView is not allowed");
    }
    webViewURIChanged(webView, NULL, window);
    webViewTitleChanged(webView, NULL, window);
    webViewIsLoadingChanged(webView, NULL, window);
    faviconChanged(webView, NULL, window);
    browserTabbedWindowUpdateZoomActions(window);
    if (webkit_web_view_is_loading(webView))
        webViewLoadProgressChanged(webView, NULL, window);

    g_signal_connect(webView, "notify::uri",
            G_CALLBACK(webViewURIChanged), window);
    g_signal_connect(webView, "notify::estimated-load-progress",
            G_CALLBACK(webViewLoadProgressChanged), window);
    g_signal_connect(webView, "notify::title",
            G_CALLBACK(webViewTitleChanged), window);
    g_signal_connect(webView, "notify::is-loading",
            G_CALLBACK(webViewIsLoadingChanged), window);
    g_signal_connect(webView, "create",
            G_CALLBACK(browser_window_webview_create), window);
    g_signal_connect(webView, "load-failed",
            G_CALLBACK(webViewLoadFailed), window);
    g_signal_connect(webView, "decide-policy",
            G_CALLBACK(webViewDecidePolicy), window);
    g_signal_connect(webView, "mouse-target-changed",
            G_CALLBACK(webViewMouseTargetChanged), window);
    g_signal_connect(webView, "notify::zoom-level",
            G_CALLBACK(webViewZoomLevelChanged), window);
    g_signal_connect(webView, "notify::favicon",
            G_CALLBACK(faviconChanged), window);
    g_signal_connect(webView, "enter-fullscreen",
            G_CALLBACK(webViewEnterFullScreen), window);
    g_signal_connect(webView, "leave-fullscreen",
            G_CALLBACK(webViewLeaveFullScreen), window);
#if !GTK_CHECK_VERSION(3, 98, 0)
    g_signal_connect(webView, "scroll-event",
            G_CALLBACK(scrollEventCallback), window);
#endif
    g_signal_connect_object(webView, "notify::camera-capture-state",
            G_CALLBACK(webViewMediaCaptureStateChanged), window, 0);
    g_signal_connect_object(webView, "notify::microphone-capture-state",
            G_CALLBACK(webViewMediaCaptureStateChanged), window, 0);
    g_signal_connect_object(webView, "notify::display-capture-state",
            G_CALLBACK(webViewMediaCaptureStateChanged), window, 0);

    g_object_set(window->uriEntry, "secondary-icon-activatable", TRUE, NULL);
    g_signal_connect(window->uriEntry, "icon-press",
            G_CALLBACK(webViewUriEntryIconPressed), window);

    WebKitBackForwardList *backForwardlist =
        webkit_web_view_get_back_forward_list(webView);
    browserTabbedWindowUpdateNavigationMenu(window, backForwardlist);
    g_signal_connect(backForwardlist, "changed",
            G_CALLBACK(backForwardlistChanged), window);
}

static void
browserTabbedWindowTabAddedOrRemoved(GtkNotebook *notebook, BrowserTab *tab,
        guint tabIndex, BrowserTabbedWindow *window)
{
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(window->notebook),
            gtk_notebook_get_n_pages(notebook) > 1);
}

static void
browserTabbedWindowBuildPopoverMenu(BrowserTabbedWindow *window,
        GtkWidget *parent)
{
    GMenu *menu = g_menu_new();
    GMenu *section = g_menu_new();
    GMenuItem *item = g_menu_item_new("Zoom Out", "win.zoom-out");
    g_menu_item_set_attribute(item, "verb-icon", "s", "zoom-out-symbolic");
    g_menu_append_item(section, item);
    g_object_unref(item);

    item = g_menu_item_new("Zoom Original", "win.zoom-default");
    g_menu_item_set_attribute(item, "verb-icon", "s", "zoom-original-symbolic");
    g_menu_append_item(section, item);
    g_object_unref(item);

    item = g_menu_item_new("Zoom In", "win.zoom-in");
    g_menu_item_set_attribute(item, "verb-icon", "s", "zoom-in-symbolic");
    g_menu_append_item(section, item);
    g_object_unref(item);

    GMenuItem *sectionItem = g_menu_item_new_section(NULL, G_MENU_MODEL(section));
    g_menu_item_set_attribute(sectionItem, "display-hint", "s", "horizontal-buttons");
    g_menu_append_item(menu, sectionItem);
    g_object_unref(sectionItem);
    g_object_unref(section);

    section = g_menu_new();
    g_menu_insert(section, -1, "_New Private Window", "win.open-private-window");
    g_menu_insert(section, -1, "_Print…", "win.print");
    g_menu_insert(section, -1, "Prefere_nces…", "win.preferences");
    g_menu_insert(section, -1, "_Quit", "win.quit");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

#if GTK_CHECK_VERSION(3, 98, 5)
    GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
#else
    GtkWidget *popover = gtk_popover_menu_new();
    gtk_popover_bind_model(GTK_POPOVER(popover), G_MENU_MODEL(menu), NULL);
#endif
    g_object_unref(menu);

    gtk_menu_button_set_popover(GTK_MENU_BUTTON(parent), popover);
}

static const GActionEntry actions[] = {
    { "reload", reloadPage, NULL, NULL, NULL, { 0 } },
    { "reload-no-cache", reloadPageIgnoringCache, NULL, NULL, NULL, { 0 } },
    { "reload-stop", reloadOrStopCallback, NULL, NULL, NULL, { 0 } },
    { "toggle-inspector", toggleWebInspector, NULL, NULL, NULL, { 0 } },
    { "open-private-window", openPrivateWindow, NULL, NULL, NULL, { 0 } },
    { "focus-location", focusLocationBar, NULL, NULL, NULL, { 0 } },
    { "stop-load", stopPageLoad, NULL, NULL, NULL, { 0 } },
    { "load-homepage", loadHomePage, NULL, NULL, NULL, { 0 } },
    { "go-back", goBackCallback, NULL, NULL, NULL, { 0 } },
    { "go-forward", goForwardCallback, NULL, NULL, NULL, { 0 } },
    { "zoom-in", zoomInCallback, NULL, NULL, NULL, { 0 } },
    { "zoom-out", zoomOutCallback, NULL, NULL, NULL, { 0 } },
    { "zoom-default", defaultZoomCallback, NULL, NULL, NULL, { 0 } },
    { "find", searchCallback, NULL, NULL, NULL, { 0 } },
    { "preferences", settingsCallback, NULL, NULL, NULL, { 0 } },
    { "new-tab", newTabCallback, NULL, NULL, NULL, { 0 } },
    { "toggle-fullscreen", toggleFullScreen, NULL, NULL, NULL, { 0 } },
    { "print", printPage, NULL, NULL, NULL, { 0 } },
    { "close", browserTabbedWindowTryCloseCurrentWebView, NULL, NULL, NULL, { 0 } },
    { "quit", browserTabbedWindowTryClose, NULL, NULL, NULL, { 0 } },
};

static void browser_tabbed_window_init(BrowserTabbedWindow *window)
{
    window->backgroundColor.red =
        window->backgroundColor.green = window->backgroundColor.blue = 255;
    window->backgroundColor.alpha = 1;

    gtk_window_set_title(GTK_WINDOW(window),
            window->title ? window->title : BROWSER_DEFAULT_TITLE);
    gtk_window_set_default_size(GTK_WINDOW(window),
            (window->width > 0) ? window->width : 1024,
            (window->height > 0) ? window->height : 768);

    g_action_map_add_action_entries(G_ACTION_MAP(window),
            actions, G_N_ELEMENTS(actions), window);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    window->mainBox = vbox;
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_window_set_child(GTK_WINDOW(window), vbox);
#else
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show(vbox);
#endif

    window->mainFixed = gtk_fixed_new();
    gtk_widget_set_size_request(window->mainFixed,
            (window->width > 0) ? window->width : 1024,
            (window->height > 0) ? window->height : 768);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(vbox), window->mainFixed);
#else
    gtk_box_pack_start(GTK_BOX(vbox), window->mainFixed, FALSE, FALSE, 0);
    gtk_widget_show(window->mainFixed);
#endif

#if GTK_CHECK_VERSION(3, 98, 5)
    GtkEventController *controller =
        gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    gtk_event_controller_set_propagation_phase(controller, GTK_PHASE_CAPTURE);
    g_signal_connect_swapped(controller, "scroll",
            G_CALLBACK(scrollEventCallback), window);
    gtk_widget_add_controller(GTK_WIDGET(window), controller);
#endif
}

#if GTK_CHECK_VERSION(3, 98, 5)
static gboolean
browserTabbedWindowCloseRequest(GtkWindow *window)
{
    browserTabbedWindowTryClose(NULL, NULL, BROWSER_TABBED_WINDOW(window));
    return FALSE;
}
#else
static gboolean
browserTabbedWindowDeleteEvent(GtkWidget *widget, GdkEventAny* event)
{
    browserTabbedWindowTryClose(NULL, NULL, BROWSER_TABBED_WINDOW(widget));
    return TRUE;
}
#endif

static void
browser_tabbed_window_class_init(BrowserTabbedWindowClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->dispose = browserTabbedWindowDispose;
    gobjectClass->finalize = browserTabbedWindowFinalize;

#if GTK_CHECK_VERSION(3, 98, 5)
    GtkWindowClass *windowClass = GTK_WINDOW_CLASS(klass);
    windowClass->close_request = browserTabbedWindowCloseRequest;
#else
    GtkWidgetClass *widgetClass = GTK_WIDGET_CLASS(klass);
    widgetClass->delete_event = browserTabbedWindowDeleteEvent;
#endif
}

/* Public API. */
GtkWidget *
browser_tabbed_window_new(GtkWindow *parent, WebKitWebContext *webContext,
        const char *name, const char *title, gint width, gint height)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(webContext), NULL);

    BrowserTabbedWindow *window =
        BROWSER_TABBED_WINDOW(g_object_new(BROWSER_TYPE_TABBED_WINDOW,
#if !GTK_CHECK_VERSION(3, 98, 0)
            "type", GTK_WINDOW_TOPLEVEL,
#endif
            NULL));

    window->webContext = g_object_ref(webContext);
    g_signal_connect(window->webContext, "download-started",
            G_CALLBACK(downloadStarted), window);
    if (parent) {
        window->parentWindow = parent;
        g_object_add_weak_pointer(G_OBJECT(parent),
                (gpointer *)&window->parentWindow);
    }

    if (name)
        window->name = g_strdup(name);

    if (title)
        window->title = g_strdup(title);

    if (width > 0)
        window->width = width;
    if (height > 0)
        window->height = height;

    return GTK_WIDGET(window);
}

WebKitWebContext *
browser_tabbed_window_get_web_context(BrowserTabbedWindow *window)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);

    return window->webContext;
}

GtkWidget*
browser_tabbed_window_create_or_get_toolbar(BrowserTabbedWindow *window)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);

    if (window->toolbar)
        return window->toolbar;

    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    window->toolbar = toolbar;
    gtk_widget_set_margin_top(toolbar, 2);
    gtk_widget_set_margin_bottom(toolbar, 2);
    gtk_widget_set_margin_start(toolbar, 2);
    gtk_widget_set_margin_end(toolbar, 2);

    GtkWidget *navigationBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_widget_add_css_class(navigationBox, "linked");
#else
    gtk_style_context_add_class(gtk_widget_get_style_context(navigationBox),
            GTK_STYLE_CLASS_LINKED);
#endif

    window->backItem = addToolbarButton(navigationBox,
            TOOLBAR_BUTTON_NORMAL, "go-previous-symbolic", "win.go-back");
    window->forwardItem = addToolbarButton(navigationBox,
            TOOLBAR_BUTTON_NORMAL, "go-next-symbolic", "win.go-forward");
#if GTK_CHECK_VERSION(3, 98, 5)
    GtkGesture *gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture),
            GDK_BUTTON_SECONDARY);
    g_signal_connect(gesture, "pressed",
            G_CALLBACK(navigationButtonPressed), NULL);
    gtk_widget_add_controller(window->backItem, GTK_EVENT_CONTROLLER(gesture));

    gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture),
            GDK_BUTTON_SECONDARY);
    g_signal_connect(gesture, "pressed",
            G_CALLBACK(navigationButtonPressed), NULL);
    gtk_widget_add_controller(window->forwardItem,
            GTK_EVENT_CONTROLLER(gesture));
#else
    g_signal_connect(window->backItem, "button-press-event",
            G_CALLBACK(navigationButtonPressCallback), window);
    g_signal_connect(window->forwardItem, "button-press-event",
            G_CALLBACK(navigationButtonPressCallback), window);
#endif
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(toolbar), navigationBox);
#else
    gtk_box_pack_start(GTK_BOX(toolbar), navigationBox, FALSE, FALSE, 0);
    gtk_widget_show(navigationBox);
#endif

    addToolbarButton(toolbar, TOOLBAR_BUTTON_NORMAL,
            "go-home-symbolic", "win.load-homepage");
    addToolbarButton(toolbar, TOOLBAR_BUTTON_NORMAL,
            "tab-new-symbolic", "win.new-tab");

    window->uriEntry = gtk_entry_new();
    gtk_widget_set_halign(window->uriEntry, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(window->uriEntry, TRUE);
    g_signal_connect_swapped(window->uriEntry, "activate",
            G_CALLBACK(activateUriEntryCallback), (gpointer)window);
    gtk_entry_set_icon_activatable(GTK_ENTRY(window->uriEntry),
            GTK_ENTRY_ICON_PRIMARY, FALSE);
    updateUriEntryIcon(window);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(toolbar), window->uriEntry);
#else
    gtk_container_add(GTK_CONTAINER(toolbar), window->uriEntry);
    gtk_widget_show(window->uriEntry);
#endif

    window->reloadOrStopButton = addToolbarButton(toolbar,
            TOOLBAR_BUTTON_NORMAL, "view-refresh-symbolic", "win.reload-stop");
    addToolbarButton(toolbar, TOOLBAR_BUTTON_NORMAL,
            "edit-find-symbolic", "win.find");
    GtkWidget *button = addToolbarButton(toolbar, TOOLBAR_BUTTON_MENU,
            "open-menu-symbolic", NULL);
    browserTabbedWindowBuildPopoverMenu(window, button);

    GtkWidget *vbox = window->mainBox;
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(vbox), toolbar);
#else
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    gtk_widget_show(toolbar);
#endif

    return window->toolbar;
}

#if 0
static GtkWidget *create_layout_container(const char *klass)
{
    const char *occurrence;
    GtkOrientation orit = GTK_ORIENTATION_VERTICAL;

    if (klass == NULL) {
        //
    }
    else if ((occurrence = strstr(klass, "vbox"))) {
        if (occurrence[4] == '\0' || g_ascii_isspace(occurrence[4])) {
            orit = GTK_ORIENTATION_VERTICAL;
        }
    }
    else if ((occurrence = strstr(klass, "hbox"))) {
        if (occurrence[4] == '\0' || g_ascii_isspace(occurrence[4])) {
            orit = GTK_ORIENTATION_HORIZONTAL;
        }
    }

    return gtk_box_new(orit, 0);
}
#endif

GtkWidget*
browser_tabbed_window_create_layout_container(BrowserTabbedWindow *window,
        GtkWidget *container, const char *klass, const GdkRectangle *geometry)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);

    if (container == NULL || (void *)container == (void *)window) {
        container = window->mainFixed;
    }

    if (!GTK_IS_FIXED(container)) {
        g_warning("The container is not a GtkFixed: %p", container);
        return NULL;
    }

    GtkWidget *fixed = gtk_fixed_new();

    gtk_fixed_put(GTK_FIXED(container), fixed, geometry->x, geometry->y);
    gtk_widget_set_size_request(fixed, geometry->width, geometry->height);
    gtk_widget_show(fixed);
    return fixed;
}

GtkWidget*
browser_tabbed_window_create_pane_container(BrowserTabbedWindow *window,
        GtkWidget *container, const char *klass, const GdkRectangle *geometry)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);

    if (container == NULL || (void *)container == (void *)window) {
        container = window->mainFixed;
    }
    else if (!GTK_IS_FIXED(container)) {
        g_warning("The container is not a GtkFixed: %p", container);
        return NULL;
    }

    GtkWidget *fixed = gtk_fixed_new();
    window->viewContainers = g_slist_append(window->viewContainers, fixed);

    g_warning("Creating pan container: (%d, %d; %d x %d)",
            geometry->x, geometry->y, geometry->width, geometry->height);

    gtk_fixed_put(GTK_FIXED(container), fixed, geometry->x, geometry->y);
    gtk_widget_set_size_request(fixed, geometry->width, geometry->height);
    gtk_widget_show(fixed);
    return fixed;
}

GtkWidget*
browser_tabbed_window_create_tab_container(BrowserTabbedWindow *window,
        GtkWidget *container, const GdkRectangle *geometry)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);

    if (container == NULL || (void *)container == (void *)window) {
        container = window->mainFixed;
    }
    else if (!GTK_IS_FIXED(container)) {
        g_warning("The container is not a GtkFixed: %p", container);
        return NULL;
    }

    GtkWidget *notebook = gtk_notebook_new();
    g_signal_connect(notebook, "switch-page",
            G_CALLBACK(browserTabbedWindowSwitchTab), window);
    g_signal_connect(notebook, "page-added",
            G_CALLBACK(browserTabbedWindowTabAddedOrRemoved), window);
    g_signal_connect(notebook, "page-removed",
            G_CALLBACK(browserTabbedWindowTabAddedOrRemoved), window);
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook), FALSE);

    window->viewContainers = g_slist_append(window->viewContainers, notebook);

    gtk_fixed_put(GTK_FIXED(container), notebook, geometry->x, geometry->y);
    gtk_widget_set_size_request(notebook, geometry->width, geometry->height);
    gtk_widget_show(notebook);
    return notebook;
}

GtkWidget*
browser_tabbed_window_append_view_pane(BrowserTabbedWindow *window,
        GtkWidget* container, WebKitWebView *webView,
        const GdkRectangle *geometry)
{
    g_return_val_if_fail(BROWSER_IS_TABBED_WINDOW(window), NULL);
    g_return_val_if_fail(GTK_IS_FIXED(container), NULL);

    if (webkit_web_view_is_editable(webView)) {
        g_warning("Editable webView is not allowed");
        return NULL;
    }

    if (!GTK_IS_FIXED(container)) {
        g_warning("Container is not a GtkFixed: %p", container);
        return NULL;
    }

    g_signal_connect_after(webView, "close",
            G_CALLBACK(webViewClose), container);

    GtkWidget *pane = browser_pane_new(webView);
    gtk_fixed_put(GTK_FIXED(container), pane, geometry->x, geometry->y);
    gtk_widget_set_size_request(pane, geometry->width, geometry->height);

    g_warning("Creating pan: (%d, %d; %d x %d)",
            geometry->x, geometry->y, geometry->width, geometry->height);

#if !GTK_CHECK_VERSION(3, 98, 0)
    if (gtk_widget_get_app_paintable(GTK_WIDGET(window)))
#endif
        browser_pane_set_background_color(BROWSER_PANE(pane),
                &window->backgroundColor);
    gtk_widget_show(pane);

    window->nrViews++;
    return pane;
}

GtkWidget *
browser_tabbed_window_append_view_tab(BrowserTabbedWindow *window,
        GtkWidget *container, WebKitWebView *webView)
{
    g_return_if_fail(BROWSER_IS_TABBED_WINDOW(window));
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    if (webkit_web_view_is_editable(webView)) {
        g_warning("Editable webView is not allowed");
        return NULL;
    }

    if (GTK_IS_NOTEBOOK(container)) {
        g_warning("Container is not a GtkNotebook");
        return NULL;
    }

    g_signal_connect_after(webView, "close",
            G_CALLBACK(webViewClose), container);

    GtkWidget *tab = browser_tab_new(webView);
#if !GTK_CHECK_VERSION(3, 98, 0)
    if (gtk_widget_get_app_paintable(GTK_WIDGET(window)))
#endif
        browser_tab_set_background_color(BROWSER_TAB(tab),
                &window->backgroundColor);
    gtk_notebook_append_page(GTK_NOTEBOOK(container), tab,
            browser_tab_get_title_widget(BROWSER_TAB(tab)));
#if GTK_CHECK_VERSION(3, 98, 5)
    g_object_set(gtk_notebook_get_page(GTK_NOTEBOOK(container), tab),
            "tab-expand", TRUE, NULL);
#else
    gtk_container_child_set(GTK_CONTAINER(container), tab,
            "tab-expand", TRUE, NULL);
#endif
    gtk_widget_show(tab);

    window->nrViews++;
    return tab;
}

void
browser_tabbed_window_clear_container(BrowserTabbedWindow *window,
        GtkWidget *container)
{
    g_return_if_fail(BROWSER_IS_TABBED_WINDOW(window));

    if ((void *)window == (void *)container || container == NULL) {
        windowTryClose(window);
    }
    else {
        g_return_if_fail(GTK_IS_WIDGET(container));
        containerTryClose(window, container);
    }
}

void browser_tabbed_window_clear_pane_or_tab(BrowserTabbedWindow *window,
        GtkWidget *widget)
{
    g_return_if_fail(BROWSER_IS_TABBED_WINDOW(window));
    g_return_if_fail(GTK_IS_WIDGET(widget));

    WebKitWebView* web_view = NULL;
    if (BROWSER_IS_TAB(widget)) {
        BrowserTab *tab = BROWSER_TAB(widget);

        web_view = browser_tab_get_web_view(tab);
    }
    else if (BROWSER_IS_PANE(widget)) {
        BrowserPane *pane = BROWSER_PANE(widget);

        web_view = browser_pane_get_web_view(pane);
    }

    if (web_view)
        webkit_web_view_try_close(web_view);
    else
        g_warning("Bad widget: %s", gtk_widget_get_name(widget));
}

void
browser_tabbed_window_load_uri(BrowserTabbedWindow *window,
        GtkWidget *widget, const char *uri)
{
    g_return_if_fail(BROWSER_IS_TABBED_WINDOW(window));
    g_return_if_fail(GTK_IS_WIDGET(widget));
    g_return_if_fail(uri);

    if (widget == NULL)
        browser_tab_load_uri(window->activeTab, uri);
    else if (BROWSER_IS_TAB(widget)) {
        browser_tab_load_uri(BROWSER_TAB(widget), uri);
    }
    else if (BROWSER_IS_PANE(widget)) {
        browser_pane_load_uri(BROWSER_PANE(widget), uri);
    }
    else {
        g_warning("Bad widget type: %s", gtk_widget_get_name(widget));
    }
}

void browser_tabbed_window_set_background_color(BrowserTabbedWindow *window,
        GdkRGBA *rgba)
{
    g_return_if_fail(BROWSER_IS_TABBED_WINDOW(window));
    g_return_if_fail(rgba);

    g_assert(!window->activeTab);

    if (gdk_rgba_equal(rgba, &window->backgroundColor))
        return;

    window->backgroundColor = *rgba;

#if GTK_CHECK_VERSION(3, 98, 5)
    /* FIXME: transparent colors don't work. In GTK4 there's no
     * gtk_widget_set_app_paintable(),
     * what we can do instead is removing the background css class from
     * the window, but that
     * would affect other parts of the window, like toolbar or even title bar
     * background.
     */
#else
    GdkVisual *rgbaVisual = gdk_screen_get_rgba_visual(
            gtk_window_get_screen(GTK_WINDOW(window)));
    if (!rgbaVisual)
        return;

    gtk_widget_set_visual(GTK_WIDGET(window), rgbaVisual);
    gtk_widget_set_app_paintable(GTK_WIDGET(window), TRUE);
#endif
}

