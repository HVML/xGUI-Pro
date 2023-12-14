/*
** BrowserPlainWindow.c -- The implementation of BrowserPlainWindow.
**
** Copyright (C) 2022, 2023 FMSoft <http://www.fmsoft.cn>
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

#include "config.h"

#include "main.h"
#include "BrowserPlainWindow.h"
#include "FloatingToolWindow.h"

#include "BrowserPane.h"
#include "Common.h"
#include "utils/utils.h"
#include <string.h>

enum {
    PROP_0,

    PROP_TITLE,
    PROP_NAME,
    PROP_PARENT_WINDOW,
    PROP_WINDOW_LEVEL,
    PROP_WINDOW_TRANSITION,
    PROP_FOR_HVML,

    N_PROPERTIES
};

struct _BrowserPlainWindow {
    GObject parent;

    WebKitWebViewParam param;
    WebKitWebContext *webContext;
    BrowserPane *browserPane;

    gchar *name;
    gchar *title;
    gchar *level;
    gboolean forHVML;

    HWND parentWindow;
    HWND hwnd;
    HWND toolWindow;

    struct purc_window_transition transition;
};

struct _BrowserPlainWindowClass {
    GObjectClass parent;
};

enum {
    DESTROY,

    LAST_SIGNAL
};

static struct window_level_type {
    const char *name;
    DWORD level;
} window_levels[] = {
    { WINDOW_LEVEL_DOCKER, WS_EX_WINTYPE_DOCKER },
    { WINDOW_LEVEL_GLOBAL, WS_EX_WINTYPE_GLOBAL },
    { WINDOW_LEVEL_HIGHER, WS_EX_WINTYPE_HIGHER },
    { WINDOW_LEVEL_LAUNCHER, WS_EX_WINTYPE_LAUNCHER },
    { WINDOW_LEVEL_NORMAL, WS_EX_WINTYPE_NORMAL },
    { WINDOW_LEVEL_SCREENLOCK, WS_EX_WINTYPE_SCREENLOCK },
    { WINDOW_LEVEL_TOOLTIP, WS_EX_WINTYPE_TOOLTIP },
};


static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(BrowserPlainWindow, browser_plain_window,
        G_TYPE_OBJECT)

static LRESULT PlainWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case MSG_CREATE:
            xgui_window_inc();
            break;

        case MSG_CLOSE:
            {
                BrowserPlainWindow *window = (BrowserPlainWindow *)
                    GetWindowAdditionalData(hWnd);
                WebKitWebView *webView = browser_pane_get_web_view(
                        window->browserPane);
                webkit_web_view_try_close(webView);
            }
            return 0;

        case MSG_DESTROY:
            {
                BrowserPlainWindow *window = (BrowserPlainWindow *)
                    GetWindowAdditionalData(hWnd);
                g_signal_emit(window, signals[DESTROY], 0, NULL);
                g_object_unref(window);
                xgui_window_dec();
            }
            break;

        case MSG_XGUIPRO_IDLE:
            //fprintf(stderr, "#####################################################xgui idle %ld\n", wParam);
            break;
    }

    return DefaultMainWinProc(hWnd, message, wParam, lParam);
}

static void browser_plain_window_init(BrowserPlainWindow *window)
{
}

static DWORD find_window_level(const char *name)
{
    static ssize_t max = sizeof(window_levels)/sizeof(window_levels[0]) - 1;

    ssize_t low = 0, high = max, mid;
    while (low <= high) {
        int cmp;

        mid = (low + high) / 2;
        cmp = strcasecmp(name, window_levels[mid].name);
        if (cmp == 0) {
            goto found;
        }
        else {
            if (cmp < 0) {
                high = mid - 1;
            }
            else {
                low = mid + 1;
            }
        }
    }

    return WS_EX_WINTYPE_NORMAL;

found:
    return window_levels[mid].level;
}

static void browserPlainWindowConstructed(GObject *gObject)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(gObject);
    G_OBJECT_CLASS(browser_plain_window_parent_class)->constructed(gObject);

    RECT rc;
    HWND parent;
    if (window->parentWindow) {
        parent = window->parentWindow;
        GetWindowRect(parent, &rc);
    }
    else {
        parent = g_xgui_main_window;
        rc = xphbd_get_default_window_rect();
    }
    int x = rc.left;
    int y = rc.top;
    int w = RECTW(rc);
    int h = RECTH(rc);

    DWORD window_level;
    if (window->level) {
        window_level = find_window_level(window->level);
    }
    else {
        window_level = WS_EX_WINTYPE_NORMAL;
    }

    MAINWINCREATE CreateInfo;
    CreateInfo.dwStyle = window->forHVML ? WS_VISIBLE : WS_NONE;
    CreateInfo.dwExStyle = window_level;
    CreateInfo.spCaption = window->title ? window->title : BROWSER_DEFAULT_TITLE;
    CreateInfo.hMenu = 0;
    CreateInfo.hCursor = GetSystemCursor(0);
    CreateInfo.hIcon = 0;
    CreateInfo.MainWindowProc = PlainWindowProc;
    CreateInfo.lx = x;
    CreateInfo.ty = y;
    CreateInfo.rx = x + w;
    CreateInfo.by = y + h;
    CreateInfo.iBkColor = COLOR_lightwhite;
    CreateInfo.dwAddData = 0;
    CreateInfo.hHosting = parent;
    window->hwnd = CreateMainWindow(&CreateInfo);
    if (window->hwnd != HWND_INVALID) {
        SetWindowAdditionalData(window->hwnd, (DWORD)window);
        // ShowWindow(window->hwnd, SW_SHOWNORMAL);
    }
}

static void browserPlainWindowSetProperty(GObject *object, guint propId,
        const GValue *value, GParamSpec *pspec)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(object);

    switch (propId) {
    case PROP_TITLE:
        {
            gchar *title = g_value_get_pointer(value);
            if (title) {
                window->title = g_strdup(title);
            }
        }
        break;
    case PROP_NAME:
        {
            gchar *name = g_value_get_pointer(value);
            if (name) {
                window->name = g_strdup(name);
            }
        }
        break;
    case PROP_PARENT_WINDOW:
        {
            gpointer *p = g_value_get_pointer(value);
            if (p) {
                window->parentWindow = (HWND)p;
            }
        }
        break;
    case PROP_WINDOW_LEVEL:
        {
            gchar *level = g_value_get_pointer(value);
            if (level) {
                window->level = g_strdup(level);
            }
        }
        break;
    case PROP_WINDOW_TRANSITION:
        {
            gpointer *p = g_value_get_pointer(value);
            if (p) {
                window->transition = *(struct purc_window_transition*)p;
            }
        }
        break;
    case PROP_FOR_HVML:
        {
            window->forHVML = g_value_get_boolean(value);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
    }
}

static void browserPlainWindowDispose(GObject *gObject)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(gObject);

    if (window->parentWindow) {
        window->parentWindow = NULL;
    }

    G_OBJECT_CLASS(browser_plain_window_parent_class)->dispose(gObject);
}

static void browserPlainWindowFinalize(GObject *gObject)
{
    BrowserPlainWindow *window = BROWSER_PLAIN_WINDOW(gObject);

    if (window->webContext) {
        g_signal_handlers_disconnect_matched(window->webContext,
                G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);
        g_object_unref(window->webContext);
    }

    if (window->browserPane) {
        g_object_unref(window->browserPane);
        window->browserPane = NULL;
    }

    if (window->name) {
        g_free(window->name);
        window->name = NULL;
    }

    if (window->title) {
        g_free(window->title);
        window->title = NULL;
    }

    if (window->level) {
        g_free(window->level);
        window->level = NULL;
    }

    G_OBJECT_CLASS(browser_plain_window_parent_class)->finalize(gObject);
}

static void browser_plain_window_class_init(BrowserPlainWindowClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = browserPlainWindowConstructed;
    gobjectClass->set_property = browserPlainWindowSetProperty;
    gobjectClass->dispose = browserPlainWindowDispose;
    gobjectClass->finalize = browserPlainWindowFinalize;

    g_object_class_install_property(
            gobjectClass,
            PROP_TITLE,
            g_param_spec_pointer(
                "title",
                "PlainWindow title",
                "The plain window title",
                G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(
            gobjectClass,
            PROP_NAME,
            g_param_spec_pointer(
                "name",
                "PlainWindow name",
                "The plain window name",
                G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(
            gobjectClass,
            PROP_PARENT_WINDOW,
            g_param_spec_pointer(
                "parent-window",
                "parent window",
                "The parent window",
                G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(
            gobjectClass,
            PROP_WINDOW_LEVEL,
            g_param_spec_pointer(
                "window-level",
                "window level",
                "The window level",
                G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(
            gobjectClass,
            PROP_WINDOW_TRANSITION,
            g_param_spec_pointer(
                "transition",
                "transition",
                "The window transition",
                G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(
            gobjectClass,
            PROP_FOR_HVML,
            g_param_spec_boolean(
                "for-hvml",
                "for HVML",
                "Boolean for HVML or not",
                FALSE, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

      signals[DESTROY] =
          g_signal_new("destroy",
                       G_TYPE_FROM_CLASS(klass),
                       G_SIGNAL_RUN_LAST,
                       0,
                       0, 0,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

/* Public API. */
BrowserPlainWindow *
browser_plain_window_new(HWND parent, WebKitWebContext *webContext,
        const char *name, const char *title, const char *window_level,
        const struct purc_window_transition *transition,
        BOOL forHVML)
{
    BrowserPlainWindow *window =
          BROWSER_PLAIN_WINDOW(g_object_new(BROWSER_TYPE_PLAIN_WINDOW,
                      "name", name,
                      "title", title,
                      "parent-window", parent,
                      "window-level", window_level,
                      "transition", transition,
                      "for-hvml", forHVML,
                      NULL));

    if (webContext) {
        window->webContext = g_object_ref(webContext);
    }

    return window;
}

HWND browser_plain_window_get_hwnd(BrowserPlainWindow *window)
{
    return window->hwnd;
}

WebKitWebContext *
browser_plain_window_get_web_context(BrowserPlainWindow *window)
{
    g_return_val_if_fail(BROWSER_IS_PLAIN_WINDOW(window), NULL);

    return window->webContext;
}

extern HWND g_xgui_main_window;
static void webViewClose(WebKitWebView *webView, BrowserPlainWindow *window)
{
    HWND hwnd = window->hwnd;
    DestroyAllControls(hwnd);
    DestroyMainWindow(hwnd);
    MainWindowCleanup(hwnd);

    if (hwnd == g_xgui_main_window)
        g_xgui_main_window = HWND_NULL;
}

#if 0
static char *getExternalURI(const char *uri)
{
    /* From the user point of view we support about: prefix. */
    if (uri && g_str_has_prefix(uri, BROWSER_ABOUT_SCHEME))
        return g_strconcat("about", uri + strlen(BROWSER_ABOUT_SCHEME), NULL);

    return g_strdup(uri);
}

static void webViewURIChanged(WebKitWebView *webView, GParamSpec *pspec,
        BrowserPlainWindow *window)
{
    char *externalURI = getExternalURI(webkit_web_view_get_uri(webView));
    g_free(externalURI);
}
#endif

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
    SetWindowCaption(window->hwnd,
            privateTitle ? privateTitle : title);
    g_free(privateTitle);

    if (window->toolWindow)
        SendNotifyMessage(window->toolWindow, MSG_XGUIPRO_LOAD_STATE, XGUIPRO_LOAD_STATE_TITLE_CHANGED, (LPARAM)title);
}

static void webViewLoadProgressChanged(WebKitWebView *webView, GParamSpec *pspec, BrowserPlainWindow *window)
{
    gdouble progress = webkit_web_view_get_estimated_load_progress(webView);

    unsigned percent = (unsigned)(progress * 100);
    if (window->toolWindow)
        SendNotifyMessage(window->toolWindow, MSG_XGUIPRO_LOAD_STATE, XGUIPRO_LOAD_STATE_PROGRESS_CHANGED, percent);
}

static gboolean webViewLoadFailed(WebKitWebView *webView, WebKitLoadEvent loadEvent, const char *failingURI, GError *error, BrowserPlainWindow *window)
{
    if (window->toolWindow)
        SendNotifyMessage(window->toolWindow, MSG_XGUIPRO_LOAD_STATE, XGUIPRO_LOAD_STATE_FAILED, 0);
    return FALSE;
}

static void webViewLoadChanged(WebKitWebView *webView, WebKitLoadEvent loadEvent, BrowserPlainWindow *window)
{
    const char *uri;

    switch (loadEvent) {
    case WEBKIT_LOAD_STARTED:
        // New load, we have now a provisional URI
        // provisional_uri = webkit_web_view_get_uri (web_view);
        // Here we could start a spinner or update the
        // location bar with the provisional URI
        uri = webkit_web_view_get_uri(webView);
        if (xphbd_use_floating_tool_window() && strncmp(uri, "hvml://", 5)) {
            window->toolWindow = create_floating_tool_window(window->hwnd, "Untitled");
            if (window->toolWindow != HWND_INVALID) {
                g_signal_connect(webView, "notify::estimated-load-progress", G_CALLBACK(webViewLoadProgressChanged), window);
                g_signal_connect(webView, "load-failed", G_CALLBACK(webViewLoadFailed), window);
            }
            else
                window->toolWindow = HWND_NULL;
        }
        break;

    case WEBKIT_LOAD_REDIRECTED:
        if (window->toolWindow)
            SendNotifyMessage(window->toolWindow, MSG_XGUIPRO_LOAD_STATE, XGUIPRO_LOAD_STATE_REDIRECTED, 0);
        break;

    case WEBKIT_LOAD_COMMITTED:
        // The load is being performed. Current URI is
        // the final one and it won't change unless a new
        // load is requested or a navigation within the
        // same page is performed
        if (window->toolWindow)
            SendNotifyMessage(window->toolWindow, MSG_XGUIPRO_LOAD_STATE, XGUIPRO_LOAD_STATE_COMMITTED, 0);
        break;

    case WEBKIT_LOAD_FINISHED:
        // Load finished, we can now stop the spinner
        if (window->toolWindow)
            SendNotifyMessage(window->toolWindow, MSG_XGUIPRO_LOAD_STATE, XGUIPRO_LOAD_STATE_FINISHED, 0);
        break;
    }
}

static void webViewReadyToShow(WebKitWebView *webView,
        BrowserPlainWindow *window)
{
    g_signal_connect(webView, "load-changed", G_CALLBACK(webViewLoadChanged), window);
}

static WebKitWebView *webViewCreate(WebKitWebView *webView,
        WebKitNavigationAction *navigation, BrowserPlainWindow *window)
{
    BrowserPlainWindow *newWindow = browser_plain_window_new(NULL,
            window->webContext, window->name, window->title, WINDOW_LEVEL_NORMAL,
            &window->transition, FALSE);
    WebKitWebViewParam param = window->param;
    param.relatedView = webView;
    browser_plain_window_set_view(newWindow, &param);
    WebKitWebView *newWebView = browser_plain_window_get_view(newWindow);
    webkit_web_view_set_settings(newWebView,
            webkit_web_view_get_settings(webView));
    g_signal_connect(newWebView, "ready-to-show",
            G_CALLBACK(webViewReadyToShow), newWindow);
    return newWebView;
}

static void browserPlainWindowSetupSignalHandlers(BrowserPlainWindow *window)
{
    WebKitWebView *webView = browser_pane_get_web_view(window->browserPane);
#if 0
    webViewURIChanged(webView, NULL, window);
    g_signal_connect(webView, "notify::uri",
            G_CALLBACK(webViewURIChanged), window);
#endif
    webViewTitleChanged(webView, NULL, window);
    g_signal_connect(webView, "notify::title",
            G_CALLBACK(webViewTitleChanged), window);
    g_signal_connect(webView, "create",
            G_CALLBACK(webViewCreate), window);
}

void browser_plain_window_set_view(BrowserPlainWindow *window, WebKitWebViewParam *param)
{
    g_return_if_fail(BROWSER_IS_PLAIN_WINDOW(window));

    if (window->browserPane) {
        g_assert(browser_pane_get_web_view(window->browserPane));
        g_warning("Only one webView allowed in a plainwin.");
        return;
    }

    param->webViewParent = window->hwnd;
    param->webContext = window->webContext;

    RECT rc;
    GetClientRect(window->hwnd, &rc);
    SetRect(&param->webViewRect, rc.left, rc.top, RECTW(rc), RECTH(rc));
    window->browserPane = (BrowserPane*)browser_pane_new(param);
    webkit_web_view_set_draw_background_callback(
            browser_pane_get_web_view(window->browserPane),
            xgui_webview_draw_background_callback);
    g_signal_connect_after(window->browserPane->webView, "close", G_CALLBACK(webViewClose), window);

    window->param = *param;
    // ShowWindow(window->hwnd, SW_SHOWNORMAL);
    SetFocus(window->hwnd);

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
        GAL_Color *rgba)
{
}

void browser_plain_window_hide(BrowserPlainWindow *window)
{
    g_return_if_fail(BROWSER_IS_PLAIN_WINDOW(window));
    ShowWindow(window->hwnd, SW_HIDE);
}

void browser_plain_window_show(BrowserPlainWindow *window)
{
    g_return_if_fail(BROWSER_IS_PLAIN_WINDOW(window));
    ShowWindow(window->hwnd, SW_SHOWNORMAL);
}

void browser_plain_window_set_transition(BrowserPlainWindow *window,
        struct purc_window_transition *transition)
{
    window->transition = *transition;
}

struct purc_window_transition *
browser_plain_window_get_transition(BrowserPlainWindow *window)
{
    return &window->transition;
}

#if USE(ANIMATION)
static void animated_cb(MGEFF_ANIMATION handle, HWND hWnd, int id, RECT *rc)
{
    (void) handle;
    MoveWindow(hWnd, rc->left, rc->top, RECTWP(rc), RECTHP(rc), FALSE);
}

static void move_window_with_transition(BrowserPlainWindow *window, int x,
        int y, int w, int h)
{
    struct purc_window_transition *transition;
    transition = browser_plain_window_get_transition(window);
    if (transition->move_func == PURC_WINDOW_TRANSITION_FUNCTION_NONE
            || transition->move_duration == 0) {
        MoveWindow(window->hwnd, x, y, w, h, false);
        return;
    }

    enum EffMotionType motion = xgui_get_motion_type(transition->move_func);
    MGEFF_ANIMATION animation;
    animation = mGEffAnimationCreate((void *)window->hwnd, (void *)animated_cb, 1,
            MGEFF_RECT);
    if (animation) {
        RECT start_rc, end_rc;
        GetWindowRect(window->hwnd, &start_rc);
        end_rc.left = x;
        end_rc.top = y;
        end_rc.right = end_rc.left + w;
        end_rc.bottom = end_rc.top + h;

        mGEffAnimationSetStartValue(animation, &start_rc);
        mGEffAnimationSetEndValue(animation, &end_rc);
        mGEffAnimationSetDuration(animation, transition->move_duration);
        mGEffAnimationSetProperty(animation, MGEFF_PROP_LOOPCOUNT, 1);
        mGEffAnimationSetCurve(animation, motion);
        mGEffAnimationSyncRun(animation);

        mGEffAnimationDelete(animation);
    }

}

void browser_plain_window_layout(BrowserPlainWindow *window, int x, int y, int w,
        int h, bool enable_transition)
{
    (void) enable_transition;
    HWND hwnd = window->hwnd;
    if (enable_transition && enable_transition) {
        move_window_with_transition(window, x, y, w, h);
    }
    else {
        MoveWindow(hwnd, x, y, w, h, false);
    }
}
#else
void browser_plain_window_layout(BrowserPlainWindow *window, int x, int y, int w,
        int h, bool enable_transition)
{
    (void) enable_transition;
    HWND hwnd = window->hwnd;
    MoveWindow(hwnd, x, y, w, h, false);
}
#endif /* USE(ANIMATION) */
