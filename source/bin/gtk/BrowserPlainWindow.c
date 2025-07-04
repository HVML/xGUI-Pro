/*
** BrowserPlainWindow.c -- The implementation of BrowserPlainWindow.
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
#include "utils/utils.h"
#include "main.h"
#include "PurcmcCallbacks.h"
#include "BrowserPlainWindow.h"

#include "BrowserDownloadsBar.h"
#include "BrowserSettingsDialog.h"
#include "BrowserPane.h"
#include <gdk/gdkkeysyms.h>
#include <string.h>

struct _BrowserPlainWindow {
    GtkApplicationWindow parent;

    WebKitWebContext *webContext;

    GtkWidget *mainBox;
    GtkWidget *toolbar;
    GtkWidget *uriEntry;
    BrowserPane *browserPane;
    GtkWidget *settingsDialog;
    GtkWidget *downloadsBar;
    guint resetEntryProgressTimeoutId;

    GtkWidget *backItem;
    GtkWidget *forwardItem;
    GtkWidget *reloadOrStopButton;
    GtkWidget *editToolbar;
    GActionGroup *editActionGroup;
    gchar *sessionFile;

    gchar *name;
    gchar *title;

    gboolean fullScreenIsEnabled;
    gboolean activated;

#if GTK_CHECK_VERSION(3, 98, 0)
    GdkTexture *favicon;
#else
    GdkPixbuf *favicon;
#endif

    GtkWindow *parentWindow;
    GdkRGBA backgroundColor;
};

struct _BrowserPlainWindowClass {
    GtkApplicationWindowClass parent;
};

static const gdouble minimumZoomLevel = 0.5;
static const gdouble maximumZoomLevel = 3;
static const gdouble defaultZoomLevel = 1;
static const gdouble zoomStep = 1.2;

G_DEFINE_TYPE(BrowserPlainWindow, browser_plain_window,
        GTK_TYPE_APPLICATION_WINDOW)

static char *getExternalURI(const char *uri)
{
    /* From the user point of view we support about: prefix. */
    if (uri && g_str_has_prefix(uri, BROWSER_ABOUT_SCHEME))
        return g_strconcat("about", uri + strlen(BROWSER_ABOUT_SCHEME), NULL);

    return g_strdup(uri);
}

static void browserPlainWindowSetStatusText(BrowserPlainWindow *window,
        const char *text)
{
    browser_pane_set_status_text(window->browserPane, text);
}

static void activateUriEntryCallback(BrowserPlainWindow *window)
{
    browser_plain_window_load_uri(window,
#if GTK_CHECK_VERSION(3, 98, 0)
        gtk_editable_get_text(GTK_EDITABLE(window->uriEntry))
#else
        gtk_entry_get_text(GTK_ENTRY(window->uriEntry))
#endif
    );
}

static void reloadOrStopCallback(GSimpleAction *action,
        GVariant *parameter, gpointer userData)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(userData);
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    if (webkit_web_view_is_loading(webView))
        webkit_web_view_stop_loading(webView);
    else
        webkit_web_view_reload(webView);
}

static void goBackCallback(GSimpleAction *action,
        GVariant *parameter, gpointer userData)
{
    WebKitWebView *webView =
        browser_pane_get_web_view(BROWSER_PLAIN_WINDOW(userData)->browserPane);
    webkit_web_view_go_back(webView);
}

static void goForwardCallback(GSimpleAction *action,
        GVariant *parameter, gpointer userData)
{
    WebKitWebView *webView =
        browser_pane_get_web_view(BROWSER_PLAIN_WINDOW(userData)->browserPane);
    webkit_web_view_go_forward(webView);
}

static void settingsCallback(GSimpleAction *action,
        GVariant *parameter, gpointer userData)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(userData);
    if (window->settingsDialog) {
        gtk_window_present(GTK_WINDOW(window->settingsDialog));
        return;
    }

    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    window->settingsDialog = browser_settings_dialog_new(
            webkit_web_view_get_settings(webView));
    gtk_window_set_transient_for(GTK_WINDOW(window->settingsDialog),
            GTK_WINDOW(window));
    g_object_add_weak_pointer(G_OBJECT(window->settingsDialog),
            (gpointer *)&window->settingsDialog);
    gtk_widget_show(window->settingsDialog);
}

static void webViewURIChanged(WebKitWebView *webView, GParamSpec *pspec,
        BrowserPlainWindow *window)
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

static void webViewTitleChanged(WebKitWebView *webView,
        GParamSpec *pspec, BrowserPlainWindow *window)
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

static gboolean resetEntryProgress(BrowserPlainWindow *window)
{
    if (window->uriEntry) {
        gtk_entry_set_progress_fraction(GTK_ENTRY(window->uriEntry), 0);
        window->resetEntryProgressTimeoutId = 0;
    }
    return FALSE;
}

static void webViewLoadProgressChanged(WebKitWebView *webView,
        GParamSpec *pspec, BrowserPlainWindow *window)
{
    gdouble progress = webkit_web_view_get_estimated_load_progress(webView);
    gtk_entry_set_progress_fraction(GTK_ENTRY(window->uriEntry), progress);
    if (progress == 1.0) {
        window->resetEntryProgressTimeoutId =
            g_timeout_add(500, (GSourceFunc)resetEntryProgress, window);
        g_source_set_name_by_id(window->resetEntryProgressTimeoutId,
                "[WebKit] resetEntryProgress");
    } else if (window->resetEntryProgressTimeoutId) {
        g_source_remove(window->resetEntryProgressTimeoutId);
        window->resetEntryProgressTimeoutId = 0;
    }
}

static void downloadStarted(WebKitWebContext *webContext,
        WebKitDownload *download, BrowserPlainWindow *window)
{
#if !GTK_CHECK_VERSION(3, 98, 0)
    if (!window->downloadsBar) {
        window->downloadsBar = browser_downloads_bar_new();
        gtk_box_pack_start(GTK_BOX(window->mainBox), window->downloadsBar,
                FALSE, FALSE, 0);
        gtk_box_reorder_child(GTK_BOX(window->mainBox), window->downloadsBar, 0);
        g_object_add_weak_pointer(G_OBJECT(window->downloadsBar),
                (gpointer *)&(window->downloadsBar));
        gtk_widget_show(window->downloadsBar);
    }
    browser_downloads_bar_add_download(
            BROWSER_DOWNLOADS_BAR(window->downloadsBar), download);
#endif
}

static void browserPlainWindowHistoryItemActivated(BrowserPlainWindow *window,
        GVariant *parameter, GAction *action)
{
    WebKitBackForwardListItem *item =
        g_object_get_data(G_OBJECT(action), "back-forward-list-item");
    if (!item)
        return;

    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    webkit_web_view_go_to_back_forward_list_item(webView, item);
}

static void browserPlainWindowCreateBackForwardMenu(BrowserPlainWindow *window,
        GList *list, gboolean isBack)
{
    if (!list)
        return;

    static guint64 actionId = 0;

    GSimpleActionGroup *actionGroup = g_simple_action_group_new();
    GMenu *menu = g_menu_new();
    GList *listItem;
    for (listItem = list; listItem; listItem = g_list_next(listItem)) {
        WebKitBackForwardListItem *item = (WebKitBackForwardListItem *)listItem->data;
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
                G_CALLBACK(browserPlainWindowHistoryItemActivated), window);
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
    gtk_widget_insert_action_group(popover, isBack ? "bf-back" : "bf-forward",
            G_ACTION_GROUP(actionGroup));
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

static void browserPlainWindowUpdateNavigationMenu(BrowserPlainWindow *window,
        WebKitBackForwardList *backForwardlist)
{
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    GAction *action = g_action_map_lookup_action(G_ACTION_MAP(window), "go-back");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action),
            webkit_web_view_can_go_back(webView));
    action = g_action_map_lookup_action(G_ACTION_MAP(window), "go-forward");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action),
            webkit_web_view_can_go_forward(webView));

    GList *list = g_list_reverse(
            webkit_back_forward_list_get_back_list_with_limit(backForwardlist,
                10));
    browserPlainWindowCreateBackForwardMenu(window, list, TRUE);
    g_list_free(list);

    list = webkit_back_forward_list_get_forward_list_with_limit(
            backForwardlist, 10);
    browserPlainWindowCreateBackForwardMenu(window, list, FALSE);
    g_list_free(list);
}

#if GTK_CHECK_VERSION(3, 98, 5)
static void navigationButtonPressed(GtkGestureClick *gesture,
        guint clickCount, double x, double y)
{
    GdkEventSequence *sequence =
        gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
    gtk_gesture_set_sequence_state(GTK_GESTURE(gesture),
            sequence, GTK_EVENT_SEQUENCE_CLAIMED);

    GtkWidget *button =
        gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    GtkWidget *popover =
        g_object_get_data(G_OBJECT(button), "history-popover");
    if (popover)
        gtk_popover_popup(GTK_POPOVER(popover));
}
#else
static gboolean navigationButtonPressCallback(GtkButton *button,
        GdkEvent *event, BrowserPlainWindow *window)
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

static void browserPlainWindowSaveSession(BrowserPlainWindow *window)
{
    if (!window->sessionFile)
        return;

    GKeyFile *session = g_key_file_new();
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    WebKitWebViewSessionState *state = webkit_web_view_get_session_state(webView);
    GBytes *bytes = webkit_web_view_session_state_serialize(state);
    if (bytes) {
        gsize dataLength;
        gconstpointer data;
        data = g_bytes_get_data(bytes, &dataLength);

        gchar *base64 = g_base64_encode(data, dataLength);
        g_key_file_set_string(session, "plainwin", "state", base64);
        g_free(base64);
        g_bytes_unref(bytes);
    }
    webkit_web_view_session_state_unref(state);
    g_key_file_save_to_file(session, window->sessionFile, NULL);
    g_key_file_free(session);
}

static void browserPlainWindowTryCloseCurrentWebView(GSimpleAction *action,
        GVariant *parameter, gpointer userData)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(userData);
    webkit_web_view_try_close(browser_pane_get_web_view(window->browserPane));
}

static void browserPlainWindowTryClose(GSimpleAction *action,
        GVariant *parameter, gpointer userData)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(userData);
    browserPlainWindowSaveSession(window);

    webkit_web_view_try_close(browser_pane_get_web_view(window->browserPane));
}

static void backForwardlistChanged(WebKitBackForwardList *backForwardlist,
        WebKitBackForwardListItem *itemAdded, GList *itemsRemoved,
        BrowserPlainWindow *window)
{
    browserPlainWindowUpdateNavigationMenu(window, backForwardlist);
}

static void webViewClose(WebKitWebView *webView, BrowserPlainWindow *window)
{
#if GTK_CHECK_VERSION(3, 98, 4)
    gtk_window_destroy(GTK_WINDOW(window));
#else
    gtk_widget_destroy(GTK_WIDGET(window));
#endif

    /* VW: mark uriEntry is destroyed */
    window->uriEntry = NULL;
}

static void webViewRunAsModal(WebKitWebView *webView, BrowserPlainWindow *window)
{
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(window), window->parentWindow);
}

static void webViewReadyToShow(WebKitWebView *webView,
        BrowserPlainWindow *window)
{
    WebKitWindowProperties *windowProperties =
        webkit_web_view_get_window_properties(webView);

    GdkRectangle geometry;
    webkit_window_properties_get_geometry(windowProperties, &geometry);
#if GTK_CHECK_VERSION(3, 99, 5)
    if (geometry.width > 0 && geometry.height > 0)
        gtk_window_set_default_size(GTK_WINDOW(window),
                geometry.width, geometry.height);
#else
    if (geometry.x >= 0 && geometry.y >= 0)
        gtk_window_move(GTK_WINDOW(window), geometry.x, geometry.y);
    if (geometry.width > 0 && geometry.height > 0)
        gtk_window_resize(GTK_WINDOW(window),
                geometry.width, geometry.height);
#endif

    if (!webkit_window_properties_get_toolbar_visible(windowProperties))
        gtk_widget_hide(window->toolbar);
    else if (!webkit_window_properties_get_locationbar_visible(windowProperties))
        gtk_widget_hide(window->uriEntry);

    if (!webkit_window_properties_get_resizable(windowProperties))
        gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    gtk_widget_show(GTK_WIDGET(window));
}

static GtkWidget *webViewCreate(WebKitWebView *webView,
        WebKitNavigationAction *navigation, BrowserPlainWindow *window)
{
    WebKitWebView *newWebView =
        WEBKIT_WEB_VIEW(webkit_web_view_new_with_related_view(webView));
    webkit_web_view_set_settings(newWebView,
            webkit_web_view_get_settings(webView));

    GtkWidget *newWindow = browser_plain_window_new(GTK_WINDOW(window),
            window->webContext, NULL, NULL);
    gtk_window_set_application(GTK_WINDOW(newWindow),
            gtk_window_get_application(GTK_WINDOW(window)));
    browser_plain_window_set_view(BROWSER_PLAIN_WINDOW(newWindow), newWebView);
    gtk_widget_grab_focus(GTK_WIDGET(newWebView));
    g_signal_connect(newWebView, "ready-to-show",
            G_CALLBACK(webViewReadyToShow), newWindow);
    g_signal_connect(newWebView, "run-as-modal",
            G_CALLBACK(webViewRunAsModal), newWindow);
    return GTK_WIDGET(newWebView);
}

static gboolean webViewEnterFullScreen(WebKitWebView *webView,
        BrowserPlainWindow *window)
{
    gtk_widget_hide(window->toolbar);
    browser_pane_enter_fullscreen(window->browserPane);
    return FALSE;
}

static gboolean webViewLeaveFullScreen(WebKitWebView *webView,
        BrowserPlainWindow *window)
{
    browser_pane_leave_fullscreen(window->browserPane);
    gtk_widget_show(window->toolbar);
    return FALSE;
}

static gboolean webViewLoadFailed(WebKitWebView *webView,
        WebKitLoadEvent loadEvent, const char *failingURI,
        GError *error, BrowserPlainWindow *window)
{
    gtk_entry_set_progress_fraction(GTK_ENTRY(window->uriEntry), 0.);
    return FALSE;
}

static gboolean webViewDecidePolicy(WebKitWebView *webView,
        WebKitPolicyDecision *decision, WebKitPolicyDecisionType decisionType,
        BrowserPlainWindow *window)
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

    if (webkit_web_view_is_editable(webView))
        return FALSE;

    webkit_policy_decision_ignore(decision);
    return TRUE;
}

static void webViewMouseTargetChanged(WebKitWebView *webView, WebKitHitTestResult *hitTestResult, guint mouseModifiers, BrowserPlainWindow *window)
{
    if (!webkit_hit_test_result_context_is_link(hitTestResult)) {
        browserPlainWindowSetStatusText(window, NULL);
        return;
    }
    browserPlainWindowSetStatusText(window, webkit_hit_test_result_get_link_uri(hitTestResult));
}

static gboolean browserPlainWindowCanZoomIn(BrowserPlainWindow *window)
{
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    gdouble zoomLevel = webkit_web_view_get_zoom_level(webView) * zoomStep;
    return zoomLevel < maximumZoomLevel;
}

static gboolean browserPlainWindowCanZoomOut(BrowserPlainWindow *window)
{
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    gdouble zoomLevel = webkit_web_view_get_zoom_level(webView) / zoomStep;
    return zoomLevel > minimumZoomLevel;
}

static gboolean browserPlainWindowCanZoomDefault(BrowserPlainWindow *window)
{
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    return webkit_web_view_get_zoom_level(webView) != 1.0;
}

static gboolean browserPlainWindowZoomIn(BrowserPlainWindow *window)
{
    if (browserPlainWindowCanZoomIn(window)) {
        WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
        gdouble zoomLevel = webkit_web_view_get_zoom_level(webView) * zoomStep;
        webkit_web_view_set_zoom_level(webView, zoomLevel);
        return TRUE;
    }
    return FALSE;
}

static gboolean browserPlainWindowZoomOut(BrowserPlainWindow *window)
{
    if (browserPlainWindowCanZoomOut(window)) {
        WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
        gdouble zoomLevel = webkit_web_view_get_zoom_level(webView) / zoomStep;
        webkit_web_view_set_zoom_level(webView, zoomLevel);
        return TRUE;
    }
    return FALSE;
}

#if GTK_CHECK_VERSION(3, 98, 5)
static gboolean scrollEventCallback(BrowserPlainWindow *window, double deltaX, double deltaY, GtkEventController *controller)
{
    GdkModifierType mod = gtk_accelerator_get_default_mod_mask();
    GdkEvent *event = gtk_event_controller_get_current_event(controller);
    if ((gdk_event_get_modifier_state(event) & mod) != GDK_CONTROL_MASK)
        return GDK_EVENT_PROPAGATE;

    return deltaY < 0 ? browserPlainWindowZoomIn(window) : browserPlainWindowZoomOut(window);
}
#else
static gboolean scrollEventCallback(WebKitWebView *webView, const GdkEventScroll *event, BrowserPlainWindow *window)
{
    GdkModifierType mod = gtk_accelerator_get_default_mod_mask();

    if ((event->state & mod) != GDK_CONTROL_MASK)
        return FALSE;

    if (event->delta_y < 0)
        return browserPlainWindowZoomIn(window);

    return browserPlainWindowZoomOut(window);
}
#endif

static void browserPlainWindowUpdateZoomActions(BrowserPlainWindow *window)
{
    GAction *action = g_action_map_lookup_action(G_ACTION_MAP(window), "zoom-in");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), browserPlainWindowCanZoomIn(window));
    action = g_action_map_lookup_action(G_ACTION_MAP(window), "zoom-out");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), browserPlainWindowCanZoomOut(window));
    action = g_action_map_lookup_action(G_ACTION_MAP(window), "zoom-default");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(action), browserPlainWindowCanZoomDefault(window));
}

static void webViewZoomLevelChanged(GObject *object, GParamSpec *paramSpec, BrowserPlainWindow *window)
{
    browserPlainWindowUpdateZoomActions(window);
}

static void updateUriEntryIcon(BrowserPlainWindow *window)
{
    GtkEntry *entry = GTK_ENTRY(window->uriEntry);
    if (window->favicon)
#if GTK_CHECK_VERSION(3, 98, 0)
        gtk_entry_set_icon_from_paintable(entry, GTK_ENTRY_ICON_PRIMARY, GDK_PAINTABLE(window->favicon));
#else
        gtk_entry_set_icon_from_pixbuf(entry, GTK_ENTRY_ICON_PRIMARY, window->favicon);
#endif
    else
        gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_PRIMARY, "document-new");
}

static void faviconChanged(WebKitWebView *webView, GParamSpec *paramSpec, BrowserPlainWindow *window)
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
        GBytes *bytes = g_bytes_new(cairo_image_surface_get_data(surface), stride * height);
        favicon = gdk_memory_texture_new(width, height, GDK_MEMORY_DEFAULT, bytes, stride);
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

#if WEBKIT_CHECK_VERSION(2, 34, 0)
static void webViewMediaCaptureStateChanged(WebKitWebView* webView, GParamSpec* paramSpec, BrowserPlainWindow* window)
{
    const gchar* name = g_param_spec_get_name(paramSpec);
    // FIXME: the URI entry is not great storage in case more than one capture device is in use,
    // because it can store only one secondary icon.
    GtkEntry *entry = GTK_ENTRY(window->uriEntry);

    if (g_str_has_prefix(name, "microphone")) {
        switch (webkit_web_view_get_microphone_capture_state(webView)) {
        case WEBKIT_MEDIA_CAPTURE_STATE_NONE:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, NULL);
            break;
        case WEBKIT_MEDIA_CAPTURE_STATE_MUTED:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, "microphone-sensivity-mutes-symbolic");
            break;
        case WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, "audio-input-microphone-symbolic");
            break;
        }
    } else if (g_str_has_prefix(name, "camera")) {
        switch (webkit_web_view_get_camera_capture_state(webView)) {
        case WEBKIT_MEDIA_CAPTURE_STATE_NONE:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, NULL);
            break;
        case WEBKIT_MEDIA_CAPTURE_STATE_MUTED:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, "camera-disabled-symbolic");
            break;
        case WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, "camera-web-symbolic");
            break;
        }
    } else if (g_str_has_prefix(name, "display")) {
        switch (webkit_web_view_get_display_capture_state(webView)) {
        case WEBKIT_MEDIA_CAPTURE_STATE_NONE:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, NULL);
            break;
        case WEBKIT_MEDIA_CAPTURE_STATE_MUTED:
            // FIXME: I found no suitable icon for this.
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, "media-playback-stop-symbolic");
            break;
        case WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE:
            gtk_entry_set_icon_from_icon_name(entry, GTK_ENTRY_ICON_SECONDARY, "video-display-symbolic");
            break;
        }
    }
}
#endif /* WebKit >= 2.34.0 */

static void webViewUriEntryIconPressed(GtkEntry* entry, GtkEntryIconPosition position, GdkEvent* event, BrowserPlainWindow* window)
{
    if (position != GTK_ENTRY_ICON_SECONDARY)
        return;

#if WEBKIT_CHECK_VERSION(2, 34, 0)
    // FIXME: What about audio/video?
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    switch (webkit_web_view_get_display_capture_state(webView)) {
    case WEBKIT_MEDIA_CAPTURE_STATE_NONE:
        break;
    case WEBKIT_MEDIA_CAPTURE_STATE_MUTED:
        webkit_web_view_set_display_capture_state(webView, WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE);
        break;
    case WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE:
        webkit_web_view_set_display_capture_state(webView, WEBKIT_MEDIA_CAPTURE_STATE_MUTED);
        break;
    }
#endif /* WebKit >= 2.34.0 */
}

static void webViewIsLoadingChanged(WebKitWebView *webView, GParamSpec *paramSpec, BrowserPlainWindow *window)
{
    gboolean isLoading = webkit_web_view_is_loading(webView);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_button_set_icon_name(GTK_BUTTON(window->reloadOrStopButton), isLoading ? "process-stop-symbolic" : "view-refresh-symbolic");
#else
    GtkWidget *image = gtk_button_get_image(GTK_BUTTON(window->reloadOrStopButton));
    g_object_set(image, "icon-name", isLoading ? "process-stop-symbolic" : "view-refresh-symbolic", NULL);
#endif
}

static void zoomInCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    browserPlainWindowZoomIn(BROWSER_PLAIN_WINDOW(userData));
}

static void zoomOutCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    browserPlainWindowZoomOut(BROWSER_PLAIN_WINDOW(userData));
}

static void defaultZoomCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    WebKitWebView *webView = browser_pane_get_web_view(BROWSER_PLAIN_WINDOW(userData)->browserPane);
    webkit_web_view_set_zoom_level(webView, defaultZoomLevel);
}

static void searchCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    browser_pane_start_search(BROWSER_PLAIN_WINDOW(userData)->browserPane);
}

#if 0
static void newTabCallback(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(userData);
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    if (webkit_web_view_is_editable(webView))
        return;

    // do nothing for plain window.
    return;
}

static void openPrivateWindow(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(userData);
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    WebKitWebView *newWebView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-context", webkit_web_view_get_context(webView),
        "settings", webkit_web_view_get_settings(webView),
        "user-content-manager", webkit_web_view_get_user_content_manager(webView),
        "is-ephemeral", TRUE,
        "is-controlled-by-automation", webkit_web_view_is_controlled_by_automation(webView),
        "website-policies", webkit_web_view_get_website_policies(webView),
        NULL));
    GtkWidget *newWindow = browser_plain_window_new(GTK_WINDOW(window), window->webContext, NULL, NULL);
    gtk_window_set_application(GTK_WINDOW(newWindow), gtk_window_get_application(GTK_WINDOW(window)));
    browser_plain_window_append_view(BROWSER_PLAIN_WINDOW(newWindow), newWebView);
    gtk_widget_grab_focus(GTK_WIDGET(newWebView));
    gtk_widget_show(GTK_WIDGET(newWindow));
}
#endif

static void toggleWebInspector(GSimpleAction *action,
        GVariant *parameter, gpointer userData)
{
    browser_pane_toggle_inspector(BROWSER_PLAIN_WINDOW(userData)->browserPane);
}

static void focusLocationBar(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    gtk_widget_grab_focus(BROWSER_PLAIN_WINDOW(userData)->uriEntry);
}

static void reloadPage(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    WebKitWebView *webView = browser_pane_get_web_view(BROWSER_PLAIN_WINDOW(userData)->browserPane);
    webkit_web_view_reload(webView);
}

static void reloadPageIgnoringCache(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    WebKitWebView *webView = browser_pane_get_web_view(BROWSER_PLAIN_WINDOW(userData)->browserPane);
    webkit_web_view_reload_bypass_cache(webView);
}

static void stopPageLoad(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(userData);
    browser_pane_stop_search(window->browserPane);
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    if (webkit_web_view_is_loading(webView))
        webkit_web_view_stop_loading(webView);
}

static void loadHomePage(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(userData);
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    webkit_web_view_load_uri(webView, BROWSER_DEFAULT_URL);
}

static void toggleFullScreen(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(userData);
    if (!window->fullScreenIsEnabled) {
        gtk_window_fullscreen(GTK_WINDOW(window));
        gtk_widget_hide(window->toolbar);
        window->fullScreenIsEnabled = TRUE;
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(window));
        gtk_widget_show(window->toolbar);
        window->fullScreenIsEnabled = FALSE;
    }
}

static void webKitPrintOperationFailedCallback(WebKitPrintOperation *printOperation, GError *error)
{
    g_warning("Print failed: '%s'", error->message);
}

static void printPage(GSimpleAction *action, GVariant *parameter, gpointer userData)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(userData);
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    WebKitPrintOperation *printOperation = webkit_print_operation_new(webView);

    g_signal_connect(printOperation, "failed", G_CALLBACK(webKitPrintOperationFailedCallback), NULL);
    webkit_print_operation_run_dialog(printOperation, GTK_WINDOW(window));
    g_object_unref(printOperation);
}

static void editingActionCallback(GSimpleAction *action,
        GVariant *prameter, gpointer userData)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(userData);
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    webkit_web_view_execute_editing_command(webView, g_action_get_name(G_ACTION(action)));
}

static void insertImageDialogResponse(GtkDialog *dialog, int response,
        BrowserPlainWindow *window)
{
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
        if (file) {
            char *uri = g_file_get_uri(file);
            WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
            webkit_web_view_execute_editing_command_with_argument(webView,
                    WEBKIT_EDITING_COMMAND_INSERT_IMAGE, uri);
            g_free(uri);
            g_object_unref(file);
        }
    }

#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_window_destroy(GTK_WINDOW(dialog));
#else
    gtk_widget_destroy(GTK_WIDGET(dialog));
#endif
}

static void insertImageCommandCallback(GtkWidget *widget,
        BrowserPlainWindow *window)
{
    GtkWidget *fileChooser = gtk_file_chooser_dialog_new("Insert Image",
            GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_pixbuf_formats(filter);
    gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(fileChooser), filter);

    g_signal_connect(fileChooser, "response",
            G_CALLBACK(insertImageDialogResponse), window);
    gtk_widget_show(fileChooser);
}

typedef struct {
    GtkWidget *entry;
    BrowserPlainWindow *window;
} InsertLinkDialogData;

static void insertLinkDialogResponse(GtkDialog *dialog, int response,
        InsertLinkDialogData *data)
{
    if (response == GTK_RESPONSE_ACCEPT) {
#if GTK_CHECK_VERSION(3, 98, 5)
        const char *url = gtk_editable_get_text(GTK_EDITABLE(data->entry));
#else
        const char *url = gtk_entry_get_text(GTK_ENTRY(data->entry));
#endif
        if (url && *url) {
            WebKitWebView *webView =
                browser_pane_get_web_view(data->window->browserPane);
            webkit_web_view_execute_editing_command_with_argument(webView,
                    WEBKIT_EDITING_COMMAND_CREATE_LINK, url);
        }
    }

#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_window_destroy(GTK_WINDOW(dialog));
#else
    gtk_widget_destroy(GTK_WIDGET(dialog));
#endif

    g_free(data);
}

static void
insertLinkCommandCallback(GtkWidget *widget, BrowserPlainWindow *window)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Insert Link",
            GTK_WINDOW(window), GTK_DIALOG_MODAL,
            "Insert", GTK_RESPONSE_ACCEPT, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "URL");
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
            entry);
#else
    gtk_container_add(GTK_CONTAINER(
                gtk_dialog_get_content_area(GTK_DIALOG(dialog))), entry);
    gtk_widget_show(entry);
#endif

    InsertLinkDialogData *data = g_new(InsertLinkDialogData, 1);
    data->entry = entry;
    data->window = window;
    g_signal_connect(dialog, "response",
            G_CALLBACK(insertLinkDialogResponse), data);
    gtk_widget_show(dialog);
}

static void typingAttributesChanged(WebKitEditorState *editorState,
        GParamSpec *spec, BrowserPlainWindow *window)
{
    unsigned typingAttributes =
        webkit_editor_state_get_typing_attributes(editorState);
    GAction *action = g_action_map_lookup_action(
            G_ACTION_MAP(window->editActionGroup), "Bold");
    g_simple_action_set_state(G_SIMPLE_ACTION(action),
            g_variant_new_boolean(typingAttributes &
                WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD));
    action = g_action_map_lookup_action(G_ACTION_MAP(window->editActionGroup),
            "Italic");
    g_simple_action_set_state(G_SIMPLE_ACTION(action),
            g_variant_new_boolean(typingAttributes &
                WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC));
    action = g_action_map_lookup_action(G_ACTION_MAP(window->editActionGroup),
            "Underline");
    g_simple_action_set_state(G_SIMPLE_ACTION(action),
            g_variant_new_boolean(typingAttributes &
                WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE));
    action = g_action_map_lookup_action(G_ACTION_MAP(window->editActionGroup),
            "Strikethrough");
    g_simple_action_set_state(G_SIMPLE_ACTION(action),
            g_variant_new_boolean(typingAttributes &
                WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH));
}

static void browserPlainWindowFinalize(GObject *gObject)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(gObject);

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

    g_clear_object(&window->editActionGroup);
    g_clear_pointer(&window->sessionFile, g_free);

    G_OBJECT_CLASS(browser_plain_window_parent_class)->finalize(gObject);
}

static void browserPlainWindowDispose(GObject *gObject)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(gObject);

    if (window->parentWindow) {
        g_object_remove_weak_pointer(G_OBJECT(window->parentWindow),
                (gpointer *)&window->parentWindow);
        window->parentWindow = NULL;
    }

#if GTK_CHECK_VERSION(3, 98, 5)
    g_object_set_data(G_OBJECT(window->backItem), "history-popover", NULL);
    g_object_set_data(G_OBJECT(window->forwardItem), "history-popover", NULL);
#endif

    G_OBJECT_CLASS(browser_plain_window_parent_class)->dispose(gObject);
}

typedef enum {
    TOOLBAR_BUTTON_NORMAL,
    TOOLBAR_BUTTON_TOGGLE,
    TOOLBAR_BUTTON_MENU
} ToolbarButtonType;

static GtkWidget *addToolbarButton(GtkWidget *box, ToolbarButtonType type,
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

static const GActionEntry editActions[] = {
    { "Bold", NULL, NULL, "false", editingActionCallback, { 0 } },
    { "Italic", NULL, NULL, "false", editingActionCallback, { 0 } },
    { "Underline", NULL, NULL, "false", editingActionCallback, { 0 } },
    { "Strikethrough", NULL, NULL, "false", editingActionCallback, { 0 } },
    { "Cut", editingActionCallback, NULL, NULL, NULL, { 0 } },
    { "Copy", editingActionCallback, NULL, NULL, NULL, { 0 } },
    { "Paste", editingActionCallback, NULL, NULL, NULL, { 0 } },
    { "Undo", editingActionCallback, NULL, NULL, NULL, { 0 } },
    { "Redo", editingActionCallback, NULL, NULL, NULL, { 0 } },
    { "JustifyLeft", NULL, NULL, "false", editingActionCallback, { 0 } },
    { "JustifyCenter", NULL, NULL, "false", editingActionCallback, { 0 } },
    { "JustifyRight", NULL, NULL, "false", editingActionCallback, { 0 } },
    { "Indent", editingActionCallback, NULL, NULL, NULL, { 0 } },
    { "Outdent", editingActionCallback, NULL, NULL, NULL, { 0 } },
    { "InsertUnorderedList", editingActionCallback, NULL, NULL, NULL, { 0 } },
    { "InsertOrderedList", editingActionCallback, NULL, NULL, NULL, { 0 } },
};

static void browserPlainWindowSetupEditorToolbar(BrowserPlainWindow *window)
{
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    window->editToolbar = toolbar;
    gtk_widget_set_margin_top(toolbar, 2);
    gtk_widget_set_margin_bottom(toolbar, 2);
    gtk_widget_set_margin_start(toolbar, 2);
    gtk_widget_set_margin_end(toolbar, 2);

    GSimpleActionGroup *actionGroup = g_simple_action_group_new();
    window->editActionGroup = G_ACTION_GROUP(actionGroup);
    g_action_map_add_action_entries(G_ACTION_MAP(actionGroup),
            editActions, G_N_ELEMENTS(editActions), window);
    gtk_widget_insert_action_group(toolbar,
            "edit", G_ACTION_GROUP(actionGroup));

    GtkWidget *groupBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_widget_add_css_class(groupBox, "linked");
#else
    gtk_style_context_add_class(gtk_widget_get_style_context(groupBox),
            GTK_STYLE_CLASS_LINKED);
#endif
    addToolbarButton(groupBox, TOOLBAR_BUTTON_TOGGLE,
            "format-text-bold-symbolic", "edit.Bold");
    addToolbarButton(groupBox, TOOLBAR_BUTTON_TOGGLE,
            "format-text-italic-symbolic", "edit.Italic");
    addToolbarButton(groupBox, TOOLBAR_BUTTON_TOGGLE,
            "format-text-underline-symbolic", "edit.Underline");
    addToolbarButton(groupBox, TOOLBAR_BUTTON_TOGGLE,
            "format-text-strikethrough-symbolic", "edit.Strikethrough");
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(toolbar), groupBox);
#else
    gtk_box_pack_start(GTK_BOX(toolbar), groupBox, FALSE, FALSE, 0);
    gtk_widget_show(groupBox);
#endif

    groupBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_widget_add_css_class(groupBox, "linked");
#else
    gtk_style_context_add_class(gtk_widget_get_style_context(groupBox),
            GTK_STYLE_CLASS_LINKED);
#endif
    addToolbarButton(groupBox, TOOLBAR_BUTTON_NORMAL,
            "edit-cut-symbolic", "edit.Cut");
    addToolbarButton(groupBox, TOOLBAR_BUTTON_NORMAL,
            "edit-copy-symbolic", "edit.Copy");
    addToolbarButton(groupBox, TOOLBAR_BUTTON_NORMAL,
            "edit-paste-symbolic", "edit.Paste");
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(toolbar), groupBox);
#else
    gtk_box_pack_start(GTK_BOX(toolbar), groupBox, FALSE, FALSE, 0);
    gtk_widget_show(groupBox);
#endif

    groupBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_widget_add_css_class(groupBox, "linked");
#else
    gtk_style_context_add_class(gtk_widget_get_style_context(groupBox),
            GTK_STYLE_CLASS_LINKED);
#endif
    addToolbarButton(groupBox, TOOLBAR_BUTTON_NORMAL,
            "edit-undo-symbolic", "edit.Undo");
    addToolbarButton(groupBox, TOOLBAR_BUTTON_NORMAL,
            "edit-redo-symbolic", "edit.Redo");
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(toolbar), groupBox);
#else
    gtk_box_pack_start(GTK_BOX(toolbar), groupBox, FALSE, FALSE, 0);
    gtk_widget_show(groupBox);
#endif

    groupBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_widget_add_css_class(groupBox, "linked");
#else
    gtk_style_context_add_class(gtk_widget_get_style_context(groupBox),
            GTK_STYLE_CLASS_LINKED);
#endif
    addToolbarButton(groupBox, TOOLBAR_BUTTON_NORMAL,
            "format-justify-left-symbolic", "edit.JustifyLeft");
    addToolbarButton(groupBox, TOOLBAR_BUTTON_NORMAL,
            "format-justify-center-symbolic", "edit.JustifyCenter");
    addToolbarButton(groupBox, TOOLBAR_BUTTON_NORMAL,
            "format-justify-right-symbolic", "edit.JustifyRight");
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(toolbar), groupBox);
#else
    gtk_box_pack_start(GTK_BOX(toolbar), groupBox, FALSE, FALSE, 0);
    gtk_widget_show(groupBox);
#endif

    groupBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_widget_add_css_class(groupBox, "linked");
#else
    gtk_style_context_add_class(gtk_widget_get_style_context(groupBox),
            GTK_STYLE_CLASS_LINKED);
#endif
    addToolbarButton(groupBox, TOOLBAR_BUTTON_NORMAL,
            "format-indent-more-symbolic", "edit.Indent");
    addToolbarButton(groupBox, TOOLBAR_BUTTON_NORMAL,
            "format-indent-less-symbolic", "edit.Outdent");
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(toolbar), groupBox);
#else
    gtk_box_pack_start(GTK_BOX(toolbar), groupBox, FALSE, FALSE, 0);
    gtk_widget_show(groupBox);
#endif

    groupBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_widget_add_css_class(groupBox, "linked");
#else
    gtk_style_context_add_class(gtk_widget_get_style_context(groupBox),
            GTK_STYLE_CLASS_LINKED);
#endif
    /* Not the best icons for these, but we don't have insert list icons in GTK. */
    addToolbarButton(groupBox, TOOLBAR_BUTTON_NORMAL,
            "media-record-symbolic", "edit.InsertUnorderedList");
    addToolbarButton(groupBox, TOOLBAR_BUTTON_NORMAL,
            "zoom-original-symbolic", "edit.InsertOrderedList");
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(toolbar), groupBox);
#else
    gtk_box_pack_start(GTK_BOX(toolbar), groupBox, FALSE, FALSE, 0);
    gtk_widget_show(groupBox);
#endif

    groupBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_widget_add_css_class(groupBox, "linked");
#else
    gtk_style_context_add_class(gtk_widget_get_style_context(groupBox),
            GTK_STYLE_CLASS_LINKED);
#endif
    GtkWidget *button = addToolbarButton(groupBox, TOOLBAR_BUTTON_NORMAL,
            "insert-image-symbolic", NULL);
    g_signal_connect(button, "clicked",
            G_CALLBACK(insertImageCommandCallback), window);
    button = addToolbarButton(groupBox, TOOLBAR_BUTTON_NORMAL,
            "insert-link-symbolic", NULL);
    g_signal_connect(button, "clicked",
            G_CALLBACK(insertLinkCommandCallback), window);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(toolbar), groupBox);
#else
    gtk_box_pack_start(GTK_BOX(toolbar), groupBox, FALSE, FALSE, 0);
    gtk_widget_show(groupBox);
#endif

#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_insert_child_after(GTK_BOX(window->mainBox), toolbar,
            window->toolbar);
#else
    gtk_box_pack_start(GTK_BOX(window->mainBox), toolbar, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(window->mainBox), toolbar, 1);
    gtk_widget_show(toolbar);
#endif
}

static void browserPlainWindowSetupSignalHandlers(BrowserPlainWindow *window)
{
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
    if (webkit_web_view_is_editable(webView)) {
        browserPlainWindowSetupEditorToolbar(window);
        g_signal_connect(webkit_web_view_get_editor_state(webView),
                "notify::typing-attributes",
                G_CALLBACK(typingAttributesChanged), window);
    }
    webViewURIChanged(webView, NULL, window);
    webViewTitleChanged(webView, NULL, window);
    webViewIsLoadingChanged(webView, NULL, window);
    faviconChanged(webView, NULL, window);
    browserPlainWindowUpdateZoomActions(window);
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
            G_CALLBACK(webViewCreate), window);
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

#if WEBKIT_CHECK_VERSION(2, 34, 0)
    g_signal_connect_object(webView, "notify::camera-capture-state",
            G_CALLBACK(webViewMediaCaptureStateChanged), window, 0);
    g_signal_connect_object(webView, "notify::microphone-capture-state",
            G_CALLBACK(webViewMediaCaptureStateChanged), window, 0);
    g_signal_connect_object(webView, "notify::display-capture-state",
            G_CALLBACK(webViewMediaCaptureStateChanged), window, 0);
#endif

    g_object_set(window->uriEntry, "secondary-icon-activatable",
            TRUE, NULL);
    g_signal_connect(window->uriEntry, "icon-press",
            G_CALLBACK(webViewUriEntryIconPressed), window);

    WebKitBackForwardList *backForwardlist =
        webkit_web_view_get_back_forward_list(webView);
    browserPlainWindowUpdateNavigationMenu(window, backForwardlist);
    g_signal_connect(backForwardlist, "changed",
            G_CALLBACK(backForwardlistChanged), window);
}

static void browserPlainWindowBuildPopoverMenu(BrowserPlainWindow *window,
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

    GMenuItem *sectionItem = g_menu_item_new_section(NULL,
            G_MENU_MODEL(section));
    g_menu_item_set_attribute(sectionItem,
            "display-hint", "s", "horizontal-buttons");
    g_menu_append_item(menu, sectionItem);
    g_object_unref(sectionItem);
    g_object_unref(section);

    section = g_menu_new();
    /*
     * VW disabled for plainwin.
    g_menu_insert(section, -1, "_New Private Window", "win.open-private-window");
     */
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
    // { "open-private-window", openPrivateWindow, NULL, NULL, NULL, { 0 } },
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
    // { "new-tab", newTabCallback, NULL, NULL, NULL, { 0 } },
    { "toggle-fullscreen", toggleFullScreen, NULL, NULL, NULL, { 0 } },
    { "print", printPage, NULL, NULL, NULL, { 0 } },
    { "close", browserPlainWindowTryCloseCurrentWebView, NULL, NULL, NULL, { 0 } },
    { "quit", browserPlainWindowTryClose, NULL, NULL, NULL, { 0 } },
};

static void browser_plain_window_init(BrowserPlainWindow *window)
{
    window->backgroundColor.red =
        window->backgroundColor.green =
        window->backgroundColor.blue = 255;
    window->backgroundColor.alpha = 1;

    gtk_window_set_title(GTK_WINDOW(window),
            window->title ? window->title : BROWSER_DEFAULT_TITLE);
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);

    g_action_map_add_action_entries(G_ACTION_MAP(window),
            actions, G_N_ELEMENTS(actions), window);

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
    GtkWidget *button = addToolbarButton(toolbar,
            TOOLBAR_BUTTON_MENU, "open-menu-symbolic", NULL);
    browserPlainWindowBuildPopoverMenu(window, button);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    window->mainBox = vbox;
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(vbox), toolbar);
#else
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
    // gtk_widget_show(toolbar);
#endif

#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_window_set_child(GTK_WINDOW(window), vbox);
#else
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show(vbox);
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
browserPlainWindowCloseRequest(GtkWindow *window)
{
    browserPlainWindowTryClose(NULL, NULL, BROWSER_PLAIN_WINDOW(window));
    return FALSE;
}
#else
static gboolean
browserPlainWindowDeleteEvent(GtkWidget *widget, GdkEventAny* event)
{
    browserPlainWindowTryClose(NULL, NULL, BROWSER_PLAIN_WINDOW(widget));
    return TRUE;
}
#endif

static void browser_plain_window_class_init(BrowserPlainWindowClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->dispose = browserPlainWindowDispose;
    gobjectClass->finalize = browserPlainWindowFinalize;

#if GTK_CHECK_VERSION(3, 98, 5)
    GtkWindowClass *windowClass = GTK_WINDOW_CLASS(klass);
    windowClass->close_request = browserPlainWindowCloseRequest;
#else
    GtkWidgetClass *widgetClass = GTK_WIDGET_CLASS(klass);
    widgetClass->delete_event = browserPlainWindowDeleteEvent;
#endif
}

static void post_rdr_page_activate_event(struct purcmc_server *server,
        purcmc_endpoint *endpoint, BrowserPlainWindow *window,
        const char *type)
{
    pcrdr_msg event = { };
    event.type = PCRDR_MSG_TYPE_EVENT;
    event.target = PCRDR_MSG_TARGET_PLAINWINDOW;
    event.targetValue = PTR2U64(window);
    event.eventName = purc_variant_make_string_static(type, false);
    /* TODO: use real URI for the sourceURI */
    event.sourceURI = purc_variant_make_string_static(PCRDR_APP_RENDERER,
            false);
    event.elementType = PCRDR_MSG_ELEMENT_TYPE_VOID;
    event.elementValue = PURC_VARIANT_INVALID;
    event.property = PURC_VARIANT_INVALID;
    event.dataType = PCRDR_MSG_DATA_TYPE_VOID;

    purcmc_endpoint_post_event(server, endpoint, &event);
}

int browser_plain_window_post_activate_event(BrowserPlainWindow *window)
{
    WebKitWebView *webview = browser_plain_window_get_view(window);
    purcmc_session *sess = g_object_get_data(G_OBJECT(webview),
            "purcmc-session");
    if (!sess) {
        goto out;
    }
    purcmc_endpoint *endpoint;
    endpoint = purcmc_get_endpoint_by_session(sess);

    struct purcmc_server *server;
    server = xguitls_get_purcmc_server();

    const char *type = window->activated ? "pageActivated" : "pageDeactivated";
    post_rdr_page_activate_event(server, endpoint, window, type);

out:
    return 0;
}

static gboolean on_window_state_event(GtkWidget *widget,
        GdkEventWindowState *event, gpointer user_data) {
    if (event->changed_mask & GDK_WINDOW_STATE_FOCUSED) {
        BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(widget);

        if (event->new_window_state & GDK_WINDOW_STATE_FOCUSED) {
            window->activated = true;
        } else {
            window->activated = false;
        }
        browser_plain_window_post_activate_event(window);
    }

    return FALSE;
}

/* Public API. */
GtkWidget *
browser_plain_window_new(GtkWindow *parent, WebKitWebContext *webContext,
        const char *name, const char *title)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_CONTEXT(webContext), NULL);

    BrowserPlainWindow *window =
        BROWSER_PLAIN_WINDOW(g_object_new(BROWSER_TYPE_PLAIN_WINDOW,
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

    g_signal_connect(GTK_WINDOW(window), "window-state-event",
                    G_CALLBACK(on_window_state_event),
                    window->webContext);

    if (name)
        window->name = g_strdup(name);

    if (title)
        window->title = g_strdup(title);

    return GTK_WIDGET(window);
}

WebKitWebContext *
browser_plain_window_get_web_context(BrowserPlainWindow *window)
{
    g_return_val_if_fail(BROWSER_IS_PLAIN_WINDOW(window), NULL);

    return window->webContext;
}

void browser_plain_window_set_view(BrowserPlainWindow *window,
        WebKitWebView *webView)
{
    g_return_if_fail(BROWSER_IS_PLAIN_WINDOW(window));
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    if (window->browserPane) {
        g_assert(browser_pane_get_web_view(window->browserPane));
        g_warning("Only one webView allowed in a plainwin.");
        return;
    }

    g_signal_connect_after(webView, "close", G_CALLBACK(webViewClose), window);

    window->browserPane = (BrowserPane*)browser_pane_new(webView);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(window->mainBox), window->browserPane);
#else
    gtk_box_pack_start(GTK_BOX(window->mainBox),
            GTK_WIDGET(window->browserPane), TRUE, TRUE, 0);
#endif
#if !GTK_CHECK_VERSION(3, 98, 0)
    if (gtk_widget_get_app_paintable(GTK_WIDGET(window)))
#endif
        browser_pane_set_background_color(BROWSER_PANE(window->browserPane),
                &window->backgroundColor);
    gtk_widget_show(GTK_WIDGET(window->browserPane));

    browserPlainWindowSetupSignalHandlers(window);
}

WebKitWebView *browser_plain_window_get_view(BrowserPlainWindow *window)
{
    g_return_val_if_fail(BROWSER_IS_PLAIN_WINDOW(window), NULL);
    if (window->browserPane)
        return browser_pane_get_web_view(window->browserPane);

    return NULL;
}

void browser_plain_window_load_uri(BrowserPlainWindow *window, const char *uri)
{
    g_return_if_fail(BROWSER_IS_PLAIN_WINDOW(window));
    g_return_if_fail(window->browserPane);
    g_return_if_fail(uri);

    browser_pane_load_uri(window->browserPane, uri);
}

void browser_plain_window_load_session(BrowserPlainWindow *window,
        const char *sessionFile)
{
    g_return_if_fail(BROWSER_IS_PLAIN_WINDOW(window));
    g_return_if_fail(sessionFile);

    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);

    window->sessionFile = g_strdup(sessionFile);
    GKeyFile *session = g_key_file_new();
    GError *error = NULL;
    if (!g_key_file_load_from_file(session, sessionFile, G_KEY_FILE_NONE,
                &error)) {
        if (!g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
            g_warning("Failed to open session file: %s", error->message);
        g_error_free(error);
        webkit_web_view_load_uri(webView, BROWSER_DEFAULT_URL);
        g_key_file_free(session);
        return;
    }

    gsize groupCount;
    gchar **groups = g_key_file_get_groups(session, &groupCount);
    if (!groupCount || groupCount > 1) {
        webkit_web_view_load_uri(webView, BROWSER_DEFAULT_URL);
        g_strfreev(groups);
        g_key_file_free(session);
        return;
    }

    WebKitWebView *previousWebView = NULL;
    WebKitWebViewSessionState *state = NULL;
    gchar *base64 = g_key_file_get_string(session, "plainwin", "state", NULL);
    if (base64) {
        gsize stateDataLength;
        guchar *stateData = g_base64_decode(base64, &stateDataLength);
        GBytes *bytes = g_bytes_new_take(stateData, stateDataLength);
        state = webkit_web_view_session_state_new(bytes);
        g_bytes_unref(bytes);
        g_free(base64);
    }

    if (!webView) {
        webView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
            "web-context",
            webkit_web_view_get_context(previousWebView),
            "settings",
            webkit_web_view_get_settings(previousWebView),
            "user-content-manager",
            webkit_web_view_get_user_content_manager(previousWebView),
#if WEBKIT_CHECK_VERSION(2, 30, 0)
            "website-policies",
            webkit_web_view_get_website_policies(previousWebView),
#endif
            NULL));
        browser_plain_window_set_view(window, webView);
    }

    if (state) {
        webkit_web_view_restore_session_state(webView, state);
        webkit_web_view_session_state_unref(state);
    }

    WebKitBackForwardList *bfList =
        webkit_web_view_get_back_forward_list(webView);
    WebKitBackForwardListItem *item =
        webkit_back_forward_list_get_current_item(bfList);
    if (item)
        webkit_web_view_go_to_back_forward_list_item(webView, item);
    else
        webkit_web_view_load_uri(webView, "about:blank");

    previousWebView = webView;
    webView = NULL;

    g_strfreev(groups);
    g_key_file_free(session);
}

const char* browser_plain_window_get_name(BrowserPlainWindow *window)
{
    g_return_val_if_fail(BROWSER_IS_PLAIN_WINDOW(window), NULL);

    return window->name;
}

void browser_plain_window_set_title(BrowserPlainWindow *window,
        const char *title)
{
    g_return_if_fail(BROWSER_IS_PLAIN_WINDOW(window));

    if (window->title) {
        g_free(window->title);
        window->title = NULL;
    }

    if (title) {
        window->title = g_strdup(title);
    }
}

void browser_plain_window_set_background_color(BrowserPlainWindow *window,
        GdkRGBA *rgba)
{
    g_return_if_fail(BROWSER_IS_PLAIN_WINDOW(window));
    g_return_if_fail(window->browserPane);
    g_return_if_fail(rgba);

    g_assert(!window->browserPane);

    if (gdk_rgba_equal(rgba, &window->backgroundColor))
        return;

    window->backgroundColor = *rgba;

#if GTK_CHECK_VERSION(3, 98, 5)
    /* FIXME: transparent colors don't work. In GTK4 there's no
     * gtk_widget_set_app_paintable(), what we can do instead is removing
     * the background css class from the window, but that would affect other
     * parts of the window, like toolbar or even title bar background.
     */
#else
    GdkVisual *rgbaVisual =
        gdk_screen_get_rgba_visual(gtk_window_get_screen(GTK_WINDOW(window)));
    if (!rgbaVisual)
        return;

    gtk_widget_set_visual(GTK_WIDGET(window), rgbaVisual);
    gtk_widget_set_app_paintable(GTK_WIDGET(window), TRUE);
#endif
}

void browser_plain_window_move(BrowserPlainWindow *window, int x, int y, int w,
        int h, bool sync_webview)
{
    (void) sync_webview;
    if (x >= 0 && y >= 0) {
        gtk_window_move(GTK_WINDOW(window), x, y);
    }
    if (w > 0 && h > 0) {
        gtk_window_resize(GTK_WINDOW(window), w, h);
    }
}

