/*
** BrowserPane.c -- The implementation of BrowserPane.
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

#include "BrowserSearchBox.h"
#include "BrowserWindow.h"
#include <string.h>

enum {
    PROP_0,

    PROP_VIEW
};

G_DEFINE_TYPE(BrowserPane, browser_pane, GTK_TYPE_BOX)

static GHashTable *userMediaPermissionGrantedOrigins;
static GHashTable *mediaKeySystemPermissionGrantedOrigins;

typedef struct {
    WebKitPermissionRequest *request;
    gchar *origin;
} PermissionRequestData;

static PermissionRequestData *
permissionRequestDataNew(WebKitPermissionRequest *request, gchar *origin)
{
    PermissionRequestData *data = g_malloc0(sizeof(PermissionRequestData));

    data->request = g_object_ref(request);
    data->origin = origin;

    return data;
}

static void
permissionRequestDataFree(PermissionRequestData *data)
{
    g_clear_object(&data->request);
    g_clear_pointer(&data->origin, g_free);
    g_free(data);
}

static gchar *getWebViewOrigin(WebKitWebView *webView)
{
    WebKitSecurityOrigin *origin =
        webkit_security_origin_new_for_uri(webkit_web_view_get_uri(webView));
    gchar *originStr = webkit_security_origin_to_string(origin);
    webkit_security_origin_unref(origin);

    return originStr;
}

static gboolean
decidePolicy(WebKitWebView *webView, WebKitPolicyDecision *decision,
        WebKitPolicyDecisionType decisionType, BrowserPane *pane)
{
    if (decisionType != WEBKIT_POLICY_DECISION_TYPE_RESPONSE)
        return FALSE;

    WebKitResponsePolicyDecision *responseDecision =
        WEBKIT_RESPONSE_POLICY_DECISION(decision);
    if (webkit_response_policy_decision_is_mime_type_supported(responseDecision))
        return FALSE;

    WebKitWebResource *mainResource =
        webkit_web_view_get_main_resource(webView);
    WebKitURIRequest *request =
        webkit_response_policy_decision_get_request(responseDecision);
    const char *requestURI = webkit_uri_request_get_uri(request);
    if (g_strcmp0(webkit_web_resource_get_uri(mainResource), requestURI))
        return FALSE;

    webkit_policy_decision_download(decision);
    return TRUE;
}

#if !GTK_CHECK_VERSION(3, 98, 5)
static void removeChildIfInfoBar(GtkWidget *child, GtkContainer *pane)
{
    if (GTK_IS_INFO_BAR(child))
        gtk_container_remove(pane, child);
}
#endif

static void loadChanged(WebKitWebView *webView, WebKitLoadEvent loadEvent,
        BrowserPane *pane)
{
    if (loadEvent != WEBKIT_LOAD_STARTED)
        return;

#if GTK_CHECK_VERSION(3, 98, 5)
    GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(pane));
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        if (GTK_IS_INFO_BAR(child))
            gtk_widget_unparent(child);
        child = next;
    }
#else
    gtk_container_foreach(GTK_CONTAINER(pane),
            (GtkCallback)removeChildIfInfoBar, pane);
#endif
}

static GtkWidget *createInfoBarQuestionMessage(const char *title, const char *text)
{
    GtkWidget *dialog = gtk_info_bar_new_with_buttons("No", GTK_RESPONSE_NO, "Yes", GTK_RESPONSE_YES, NULL);
    gtk_info_bar_set_message_type(GTK_INFO_BAR(dialog), GTK_MESSAGE_QUESTION);

#if GTK_CHECK_VERSION(3, 98, 5)
    GtkWidget *contentBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_spacing(GTK_BOX(contentBox), 0);
#else
    GtkWidget *contentBox = gtk_info_bar_get_content_area(GTK_INFO_BAR(dialog));
    gtk_orientable_set_orientation(GTK_ORIENTABLE(contentBox), GTK_ORIENTATION_VERTICAL);
    gtk_box_set_spacing(GTK_BOX(contentBox), 0);
#endif

    GtkLabel *label = GTK_LABEL(gtk_label_new(NULL));
    gchar *markup = g_strdup_printf("<span size='xx-large' weight='bold'>%s</span>", title);
    gtk_label_set_markup(label, markup);
    g_free(markup);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_label_set_wrap(label, TRUE);
#else
    gtk_label_set_line_wrap(label, TRUE);
#endif
    gtk_label_set_selectable(label, TRUE);
    gtk_label_set_xalign(label, 0.);
    gtk_label_set_yalign(label, 0.5);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(contentBox), GTK_WIDGET(label));
#else
    gtk_box_pack_start(GTK_BOX(contentBox), GTK_WIDGET(label), FALSE, FALSE, 2);
    gtk_widget_show(GTK_WIDGET(label));
#endif

    label = GTK_LABEL(gtk_label_new(text));
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_label_set_wrap(label, TRUE);
#else
    gtk_label_set_line_wrap(label, TRUE);
#endif
    gtk_label_set_selectable(label, TRUE);
    gtk_label_set_xalign(label, 0.);
    gtk_label_set_yalign(label, 0.5);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_append(GTK_BOX(contentBox), GTK_WIDGET(label));
    gtk_info_bar_add_child(GTK_INFO_BAR(dialog), contentBox);
#else
    gtk_box_pack_start(GTK_BOX(contentBox), GTK_WIDGET(label), FALSE, FALSE, 0);
    gtk_widget_show(GTK_WIDGET(label));
#endif

    return dialog;
}

static void tlsErrorsDialogResponse(GtkWidget *dialog, gint response, BrowserPane *pane)
{
    if (response == GTK_RESPONSE_YES) {
        const char *failingURI = (const char *)g_object_get_data(G_OBJECT(dialog), "failingURI");
        GTlsCertificate *certificate = (GTlsCertificate *)g_object_get_data(G_OBJECT(dialog), "certificate");
#if SOUP_CHECK_VERSION(2, 91, 0)
        GUri *uri = g_uri_parse(failingURI, SOUP_HTTP_URI_FLAGS, NULL);
        webkit_web_context_allow_tls_certificate_for_host(webkit_web_view_get_context(pane->webView), certificate, g_uri_get_host(uri));
        g_uri_unref(uri);
#else
        SoupURI *uri = soup_uri_new(failingURI);
        webkit_web_context_allow_tls_certificate_for_host(webkit_web_view_get_context(pane->webView), certificate, uri->host);
        soup_uri_free(uri);
#endif
        webkit_web_view_load_uri(pane->webView, failingURI);
    }
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_widget_unparent(dialog);
#else
    gtk_widget_destroy(dialog);
#endif
}

static gboolean loadFailedWithTLSerrors(WebKitWebView *webView,
        const char *failingURI, GTlsCertificate *certificate,
        GTlsCertificateFlags errors, BrowserPane *pane)
{
    gchar *text = g_strdup_printf("Failed to load %s: Do you want to continue ignoring the TLS errors?", failingURI);
    GtkWidget *dialog = createInfoBarQuestionMessage("Invalid TLS Certificate",
            text);
    g_free(text);
    g_object_set_data_full(G_OBJECT(dialog), "failingURI",
            g_strdup(failingURI), g_free);
    g_object_set_data_full(G_OBJECT(dialog), "certificate",
            g_object_ref(certificate), g_object_unref);

    g_signal_connect(dialog, "response",
            G_CALLBACK(tlsErrorsDialogResponse), pane);

#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_prepend(GTK_BOX(pane), dialog);
#else
    gtk_box_pack_start(GTK_BOX(pane), dialog, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(pane), dialog, 0);
    gtk_widget_show(dialog);
#endif

    return TRUE;
}

static gboolean pointerLockMessageTimeoutCallback(BrowserPane *pane)
{
    gtk_widget_hide(pane->pointerLockMessageLabel);
    pane->pointerLockMessageLabelId = 0;
    return FALSE;
}

static void permissionRequestDialogResponse(GtkWidget *dialog,
        gint response, PermissionRequestData *requestData)
{
    switch (response) {
    case GTK_RESPONSE_YES:
        if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(requestData->request))
            g_hash_table_add(userMediaPermissionGrantedOrigins,
                    g_strdup(requestData->origin));
#if WEBKIT_CHECK_VERSION(2,30,0)
        if (WEBKIT_IS_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST(requestData->request))
            g_hash_table_add(mediaKeySystemPermissionGrantedOrigins,
                    g_strdup(requestData->origin));
#endif

        webkit_permission_request_allow(requestData->request);
        break;
    default:
        if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(requestData->request))
            g_hash_table_remove(userMediaPermissionGrantedOrigins,
                    requestData->origin);
#if WEBKIT_CHECK_VERSION(2,30,0)
        if (WEBKIT_IS_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST(requestData->request))
            g_hash_table_remove(mediaKeySystemPermissionGrantedOrigins,
                    requestData->origin);
#endif

        webkit_permission_request_deny(requestData->request);
        break;
    }

#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_widget_unparent(dialog);
#else
    gtk_widget_destroy(dialog);
#endif
    g_clear_pointer(&requestData, permissionRequestDataFree);
}

static gboolean decidePermissionRequest(WebKitWebView *webView,
        WebKitPermissionRequest *request, BrowserPane *pane)
{
    const gchar *title = NULL;
    gchar *text = NULL;

    if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST(request)) {
        title = "Geolocation request";
        text = g_strdup("Allow geolocation request?");
    } else if (WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST(request)) {
        title = "Notification request";
        text = g_strdup("Allow notifications request?");
    } else if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(request)) {
        title = "UserMedia request";
        gboolean isForAudioDevice = webkit_user_media_permission_is_for_audio_device(WEBKIT_USER_MEDIA_PERMISSION_REQUEST(request));
        gboolean isForVideoDevice = webkit_user_media_permission_is_for_video_device(WEBKIT_USER_MEDIA_PERMISSION_REQUEST(request));
#if WEBKIT_CHECK_VERSION(2,34,0)
        gboolean isForDisplayDevice = webkit_user_media_permission_is_for_display_device(WEBKIT_USER_MEDIA_PERMISSION_REQUEST(request));
#else
        gboolean isForDisplayDevice = false;
#endif

        const char *mediaType = NULL;
        if (isForAudioDevice) {
            if (isForVideoDevice)
                mediaType = "audio/video";
            else
                mediaType = "audio";
        } else if (isForVideoDevice)
            mediaType = "video";
        else if (isForDisplayDevice)
            mediaType = "display";
        text = g_strdup_printf("Allow access to %s device?", mediaType);
    } else if (WEBKIT_IS_INSTALL_MISSING_MEDIA_PLUGINS_PERMISSION_REQUEST(request)) {
        title = "Media plugin missing request";
        text = g_strdup_printf("The media backend was unable to find a plugin to play the requested media:\n%s.\nAllow to search and install the missing plugin?",
            webkit_install_missing_media_plugins_permission_request_get_description(WEBKIT_INSTALL_MISSING_MEDIA_PLUGINS_PERMISSION_REQUEST(request)));
    } else if (WEBKIT_IS_DEVICE_INFO_PERMISSION_REQUEST(request)) {
        char* origin = getWebViewOrigin(webView);
        if (g_hash_table_contains(userMediaPermissionGrantedOrigins, origin)) {
            webkit_permission_request_allow(request);
            g_free(origin);
            return TRUE;
        }
        g_free(origin);
        return FALSE;
    } else if (WEBKIT_IS_POINTER_LOCK_PERMISSION_REQUEST(request)) {
        const gchar *titleOrURI = webkit_web_view_get_title(pane->webView);
        if (!titleOrURI || !titleOrURI[0])
            titleOrURI = webkit_web_view_get_uri(pane->webView);

        char *message = g_strdup_printf("%s wants to lock the pointer. Press ESC to get the pointer back.", titleOrURI);
        gtk_label_set_text(GTK_LABEL(pane->pointerLockMessageLabel), message);
        g_free(message);
        gtk_widget_show(pane->pointerLockMessageLabel);

        pane->pointerLockMessageLabelId = g_timeout_add_seconds(2, (GSourceFunc)pointerLockMessageTimeoutCallback, pane);
        g_source_set_name_by_id(pane->pointerLockMessageLabelId, "[WebKit]pointerLockMessageTimeoutCallback");
        return TRUE;
    }
#if WEBKIT_CHECK_VERSION(2,30,0)
    else if (WEBKIT_IS_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST(request)) {
        title = "WebsiteData access request";
        WebKitWebsiteDataAccessPermissionRequest *websiteDataAccessRequest = WEBKIT_WEBSITE_DATA_ACCESS_PERMISSION_REQUEST(request);
        const gchar *requestingDomain = webkit_website_data_access_permission_request_get_requesting_domain(websiteDataAccessRequest);
        const gchar *currentDomain = webkit_website_data_access_permission_request_get_current_domain(websiteDataAccessRequest);
        text = g_strdup_printf("Do you want to allow \"%s\" to use cookies while browsing \"%s\"? This will allow \"%s\" to track your activity",
            requestingDomain, currentDomain, requestingDomain);
    }
    else if (WEBKIT_IS_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST(request)) {
        char *origin = getWebViewOrigin(webView);
        if (g_hash_table_contains(mediaKeySystemPermissionGrantedOrigins, origin)) {
            webkit_permission_request_allow(request);
            g_free(origin);
            return TRUE;
        }
        g_free(origin);
        title = "DRM system access request";
        text = g_strdup_printf("Allow to use a CDM providing access to %s?", webkit_media_key_system_permission_get_name(WEBKIT_MEDIA_KEY_SYSTEM_PERMISSION_REQUEST(request)));
    }
#endif /* WebKit > 2.30.0 */
    else {
        g_print("%s request not handled\n", G_OBJECT_TYPE_NAME(request));
        return FALSE;
    }

    GtkWidget *dialog = createInfoBarQuestionMessage(title, text);
    g_free(text);
    g_signal_connect(dialog, "response", G_CALLBACK(permissionRequestDialogResponse), permissionRequestDataNew(request,
        getWebViewOrigin(webView)));

#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_box_prepend(GTK_BOX(pane), dialog);
#else
    gtk_box_pack_start(GTK_BOX(pane), dialog, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(pane), dialog, 0);
    gtk_widget_show(dialog);
#endif

    return TRUE;
}

static void colorChooserRGBAChanged(GtkColorChooser *colorChooser,
        GParamSpec *paramSpec, WebKitColorChooserRequest *request)
{
    GdkRGBA rgba;
    gtk_color_chooser_get_rgba(colorChooser, &rgba);
    webkit_color_chooser_request_set_rgba(request, &rgba);
}

static void popoverColorClosed(GtkWidget *popover,
        WebKitColorChooserRequest *request)
{
    webkit_color_chooser_request_finish(request);
}

static void colorChooserRequestFinished(WebKitColorChooserRequest *request,
        GtkWidget *popover)
{
    g_object_unref(request);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_widget_unparent(popover);
#else
    gtk_widget_destroy(popover);
#endif
}

static gboolean runColorChooserCallback(WebKitWebView *webView,
        WebKitColorChooserRequest *request, BrowserPane *pane)
{
#if GTK_CHECK_VERSION(3, 98, 5)
    GtkWidget *popover = gtk_popover_new();
    gtk_widget_set_parent(popover, GTK_WIDGET(webView));
#else
    GtkWidget *popover = gtk_popover_new(GTK_WIDGET(webView));
#endif

    GdkRectangle rectangle;
    webkit_color_chooser_request_get_element_rectangle(request, &rectangle);
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rectangle);

    GtkWidget *colorChooser = gtk_color_chooser_widget_new();
    GdkRGBA rgba;
    webkit_color_chooser_request_get_rgba(request, &rgba);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(colorChooser), &rgba);
    g_signal_connect(colorChooser, "notify::rgba",
            G_CALLBACK(colorChooserRGBAChanged), request);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_popover_set_child(GTK_POPOVER(popover), colorChooser);
#else
    gtk_container_add(GTK_CONTAINER(popover), colorChooser);
    gtk_widget_show(colorChooser);
#endif

    g_signal_connect_object(popover, "hide",
            G_CALLBACK(popoverColorClosed), g_object_ref(request), 0);
    g_signal_connect_object(request, "finished",
            G_CALLBACK(colorChooserRequestFinished), popover, 0);

    gtk_widget_show(popover);

    return TRUE;
}

static void webProcessTerminatedCallback(WebKitWebView *webView,
        WebKitWebProcessTerminationReason reason)
{
    if (reason == WEBKIT_WEB_PROCESS_CRASHED)
        g_warning("WebProcess CRASHED");
}

static void certificateDialogResponse(GtkDialog *dialog,
        int response, WebKitAuthenticationRequest *request)
{
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
        if (file) {
            char *path = g_file_get_path(file);
            GError *error = NULL;
            GTlsCertificate *certificate =
                g_tls_certificate_new_from_file(path, &error);
            if (certificate) {
#if WEBKIT_CHECK_VERSION(2, 34, 0)
                WebKitCredential *credential =
                    webkit_credential_new_for_certificate(certificate,
                            WEBKIT_CREDENTIAL_PERSISTENCE_FOR_SESSION);
                webkit_authentication_request_authenticate(request, credential);
                webkit_credential_free(credential);
#else
                g_warning("WebKit 2.34+ required to use certificate from %s", path);
                webkit_authentication_request_authenticate(request, NULL);
#endif
                g_object_unref(certificate);
            } else {
                g_warning("Failed to create certificate for %s", path);
                g_error_free(error);
            }
            g_free(path);
            g_object_unref(file);
        }
    } else
        webkit_authentication_request_authenticate(request, NULL);

    g_object_unref(request);

#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_window_destroy(GTK_WINDOW(dialog));
#else
    gtk_widget_destroy(GTK_WIDGET(dialog));
#endif
}

static gboolean webViewAuthenticate(WebKitWebView *webView,
        WebKitAuthenticationRequest *request, BrowserPane *pane)
{
    if (webkit_authentication_request_get_scheme(request) !=
            WEBKIT_AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE_REQUESTED)
        return FALSE;

#if GTK_CHECK_VERSION(3, 98, 5)
    GtkWindow *window = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(pane)));
#else
    GtkWindow *window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(pane)));
#endif
    GtkWidget *fileChooser =
        gtk_file_chooser_dialog_new("Certificate required", window,
                GTK_FILE_CHOOSER_ACTION_OPEN, "Cancel",
                GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, NULL);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "PEM Certificate");
    gtk_file_filter_add_mime_type(filter, "application/x-x509-ca-cert");
    gtk_file_filter_add_pattern(filter, "*.pem");
    gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(fileChooser), filter);

    g_signal_connect(fileChooser, "response",
            G_CALLBACK(certificateDialogResponse), g_object_ref(request));
    gtk_widget_show(fileChooser);

    return TRUE;
}

static gboolean inspectorOpenedInWindow(WebKitWebInspector *inspector,
        BrowserPane *pane)
{
    pane->inspectorIsVisible = TRUE;
    return FALSE;
}

static gboolean inspectorClosed(WebKitWebInspector *inspector, BrowserPane *pane)
{
    pane->inspectorIsVisible = FALSE;
    return FALSE;
}

static void browserPaneSetProperty(GObject *object, guint propId,
        const GValue *value, GParamSpec *pspec)
{
    BrowserPane *pane = BROWSER_PANE(object);

    switch (propId) {
    case PROP_VIEW:
        pane->webView = g_value_get_object(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
    }
}

static void browserPaneFinalize(GObject *gObject)
{
    BrowserPane *pane = BROWSER_PANE(gObject);

    if (pane->pointerLockMessageLabelId)
        g_source_remove(pane->pointerLockMessageLabelId);

    if (pane->fullScreenMessageLabelId)
        g_source_remove(pane->fullScreenMessageLabelId);

    G_OBJECT_CLASS(browser_pane_parent_class)->finalize(gObject);
}

static void browser_pane_init(BrowserPane *pane)
{
    gtk_orientable_set_orientation(GTK_ORIENTABLE(pane),
            GTK_ORIENTATION_VERTICAL);
}

static void browserPaneConstructed(GObject *gObject)
{
    BrowserPane *pane = BROWSER_PANE(gObject);

    G_OBJECT_CLASS(browser_pane_parent_class)->constructed(gObject);

    pane->searchBar = gtk_search_bar_new();
    GtkWidget *searchBox = browser_search_box_new(BROWSER_PANE(pane)->webView);
    gtk_search_bar_set_show_close_button(GTK_SEARCH_BAR(pane->searchBar), TRUE);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_search_bar_set_child(GTK_SEARCH_BAR(pane->searchBar), searchBox);
    gtk_search_bar_connect_entry(GTK_SEARCH_BAR(pane->searchBar),
        GTK_EDITABLE(browser_search_box_get_entry(BROWSER_SEARCH_BOX(searchBox))));
    gtk_box_prepend(GTK_BOX(pane), pane->searchBar);
#else
    gtk_container_add(GTK_CONTAINER(pane->searchBar), searchBox);
    gtk_widget_show(searchBox);
    gtk_search_bar_connect_entry(GTK_SEARCH_BAR(pane->searchBar),
        browser_search_box_get_entry(BROWSER_SEARCH_BOX(searchBox)));
    gtk_container_add(GTK_CONTAINER(pane), pane->searchBar);
    gtk_widget_show(pane->searchBar);
#endif

    GtkWidget *overlay = gtk_overlay_new();
#if GTK_CHECK_VERSION(3, 98, 4)
    gtk_box_append(GTK_BOX(pane), overlay);
#else
    gtk_container_add(GTK_CONTAINER(pane), overlay);
    gtk_widget_show(overlay);
#endif

    pane->statusLabel = gtk_label_new(NULL);
    gtk_widget_set_halign(pane->statusLabel, GTK_ALIGN_START);
    gtk_widget_set_valign(pane->statusLabel, GTK_ALIGN_END);
    gtk_widget_set_margin_start(pane->statusLabel, 1);
    gtk_widget_set_margin_end(pane->statusLabel, 1);
    gtk_widget_set_margin_top(pane->statusLabel, 1);
    gtk_widget_set_margin_bottom(pane->statusLabel, 1);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), pane->statusLabel);

    pane->pointerLockMessageLabel = gtk_label_new(NULL);
    gtk_widget_set_halign(pane->pointerLockMessageLabel, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(pane->pointerLockMessageLabel, GTK_ALIGN_START);
#if !GTK_CHECK_VERSION(3, 98, 0)
    gtk_widget_set_no_show_all(pane->pointerLockMessageLabel, TRUE);
#endif
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), pane->pointerLockMessageLabel);

    pane->fullScreenMessageLabel = gtk_label_new(NULL);
    gtk_widget_set_halign(pane->fullScreenMessageLabel, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(pane->fullScreenMessageLabel, GTK_ALIGN_CENTER);
#if !GTK_CHECK_VERSION(3, 98, 0)
    gtk_widget_set_no_show_all(pane->fullScreenMessageLabel, TRUE);
#endif
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), pane->fullScreenMessageLabel);

    gtk_widget_set_vexpand(GTK_WIDGET(pane->webView), TRUE);
#if GTK_CHECK_VERSION(3, 98, 5)
    gtk_overlay_set_child(GTK_OVERLAY(overlay), GTK_WIDGET(pane->webView));
#else
    gtk_container_add(GTK_CONTAINER(overlay), GTK_WIDGET(pane->webView));
    gtk_widget_show(GTK_WIDGET(pane->webView));
#endif

    g_signal_connect(pane->webView, "decide-policy", G_CALLBACK(decidePolicy), pane);
    g_signal_connect(pane->webView, "load-changed", G_CALLBACK(loadChanged), pane);
    g_signal_connect(pane->webView, "load-failed-with-tls-errors", G_CALLBACK(loadFailedWithTLSerrors), pane);
    g_signal_connect(pane->webView, "permission-request", G_CALLBACK(decidePermissionRequest), pane);
    g_signal_connect(pane->webView, "run-color-chooser", G_CALLBACK(runColorChooserCallback), pane);
    g_signal_connect(pane->webView, "web-process-terminated", G_CALLBACK(webProcessTerminatedCallback), NULL);
    g_signal_connect(pane->webView, "authenticate", G_CALLBACK(webViewAuthenticate), pane);

    WebKitWebInspector *inspector = webkit_web_view_get_inspector(pane->webView);
    g_signal_connect(inspector, "open-window", G_CALLBACK(inspectorOpenedInWindow), pane);
    g_signal_connect(inspector, "closed", G_CALLBACK(inspectorClosed), pane);

    if (webkit_web_view_is_editable(pane->webView))
        webkit_web_view_load_html(pane->webView, "<html></html>", "file:///");
}

static void browser_pane_class_init(BrowserPaneClass *klass)
{
    GObjectClass *gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = browserPaneConstructed;
    gobjectClass->set_property = browserPaneSetProperty;
    gobjectClass->finalize = browserPaneFinalize;

    if (!userMediaPermissionGrantedOrigins)
        userMediaPermissionGrantedOrigins =
            g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    if (!mediaKeySystemPermissionGrantedOrigins)
        mediaKeySystemPermissionGrantedOrigins =
            g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    g_object_class_install_property(
        gobjectClass,
        PROP_VIEW,
        g_param_spec_object(
            "view",
            "View",
            "The web view of this pane",
            WEBKIT_TYPE_WEB_VIEW,
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
GtkWidget *browser_pane_new(WebKitWebView *view)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(view), NULL);

    return GTK_WIDGET(g_object_new(BROWSER_TYPE_PANE, "view", view, NULL));
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
    g_return_if_fail(BROWSER_IS_PANE(pane));

    gtk_label_set_text(GTK_LABEL(pane->statusLabel), text);
    gtk_widget_set_visible(pane->statusLabel, !!text);
}

void browser_pane_toggle_inspector(BrowserPane *pane)
{
    g_return_if_fail(BROWSER_IS_PANE(pane));

    WebKitWebInspector *inspector = webkit_web_view_get_inspector(pane->webView);
    if (!pane->inspectorIsVisible) {
        webkit_web_inspector_show(inspector);
        pane->inspectorIsVisible = TRUE;
    } else
        webkit_web_inspector_close(inspector);
}

void browser_pane_set_background_color(BrowserPane *pane, GdkRGBA *rgba)
{
    g_return_if_fail(BROWSER_IS_PANE(pane));
    g_return_if_fail(rgba);

    GdkRGBA viewRGBA;
    webkit_web_view_get_background_color(pane->webView, &viewRGBA);
    if (gdk_rgba_equal(rgba, &viewRGBA))
        return;

    webkit_web_view_set_background_color(pane->webView, rgba);
}

static gboolean browserTabIsSearchBarOpen(BrowserPane *pane)
{
#if GTK_CHECK_VERSION(3, 98, 5)
    GtkWidget *revealer = gtk_widget_get_first_child(pane->searchBar);
#else
    GtkWidget *revealer = gtk_bin_get_child(GTK_BIN(pane->searchBar));
#endif
    return gtk_revealer_get_reveal_child(GTK_REVEALER(revealer));
}

void browser_pane_start_search(BrowserPane *pane)
{
    g_return_if_fail(BROWSER_IS_PANE(pane));
    if (!browserTabIsSearchBarOpen(pane))
        gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(pane->searchBar), TRUE);
}

void browser_pane_stop_search(BrowserPane *pane)
{
    g_return_if_fail(BROWSER_IS_PANE(pane));
    if (browserTabIsSearchBarOpen(pane))
        gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(pane->searchBar), FALSE);
}

static gboolean fullScreenMessageTimeoutCallback(BrowserPane *pane)
{
    gtk_widget_hide(pane->fullScreenMessageLabel);
    pane->fullScreenMessageLabelId = 0;
    return FALSE;
}

void browser_pane_enter_fullscreen(BrowserPane *pane)
{
    g_return_if_fail(BROWSER_IS_PANE(pane));

    const gchar *titleOrURI = webkit_web_view_get_title(BROWSER_PANE(pane)->webView);
    if (!titleOrURI || !titleOrURI[0])
        titleOrURI = webkit_web_view_get_uri(BROWSER_PANE(pane)->webView);

    gchar *message = g_strdup_printf("%s is now full screen. Press ESC or f to exit.", titleOrURI);
    gtk_label_set_text(GTK_LABEL(pane->fullScreenMessageLabel), message);
    g_free(message);

    gtk_widget_show(pane->fullScreenMessageLabel);

    pane->fullScreenMessageLabelId = g_timeout_add_seconds(2, (GSourceFunc)fullScreenMessageTimeoutCallback, pane);
    g_source_set_name_by_id(pane->fullScreenMessageLabelId, "[WebKit] fullScreenMessageTimeoutCallback");

    pane->wasSearchingWhenEnteredFullscreen = browserTabIsSearchBarOpen(pane);
    browser_pane_stop_search(pane);
}

void browser_pane_leave_fullscreen(BrowserPane *pane)
{
    g_return_if_fail(BROWSER_IS_PANE(pane));

    if (pane->fullScreenMessageLabelId) {
        g_source_remove(pane->fullScreenMessageLabelId);
        pane->fullScreenMessageLabelId = 0;
    }

    gtk_widget_hide(pane->fullScreenMessageLabel);

    if (pane->wasSearchingWhenEnteredFullscreen) {
        /* Opening the search bar steals the focus. Usually, we want
         * this but not when coming back from fullscreen.
         */
#if GTK_CHECK_VERSION(3, 98, 5)
        GtkWindow *window = GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(pane)));
#else
        GtkWindow *window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(pane)));
#endif
        GtkWidget *focusWidget = gtk_window_get_focus(window);
        browser_pane_start_search(pane);
        gtk_window_set_focus(window, focusWidget);
    }
}

