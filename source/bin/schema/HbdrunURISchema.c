/*
** HbdrunURISchema.c -- implementation of hbdrun URI schema.
**
** Copyright (C) 2023 FMSoft (http://www.fmsoft.cn)
**
** Author: XueShuming
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

#include "xguipro-features.h"
#include "HbdrunURISchema.h"
#include "BuildRevision.h"
//#include "LayouterWidgets.h"

#include "utils/utils.h"
#include "utils/hbdrun-uri.h"
#include "purcmc/server.h"
#include "purcmc/purcmc.h"
#include "sd/sd.h"

#include <webkit2/webkit2.h>
#include <purc/purc-pcrdr.h>
#include <purc/purc-helpers.h>
#include <gio/gunixinputstream.h>

#include <assert.h>
#include <stdio.h>

#define HBDRUN_SCHEMA_TYPE_VERSION          "version"
#define HBDRUN_SCHEMA_TYPE_APPS             "apps"
#define HBDRUN_SCHEMA_TYPE_STORE            "store"
#define HBDRUN_SCHEMA_TYPE_RUNNERS          "runners"
#define HBDRUN_SCHEMA_TYPE_CONFIRM          "confirm"
#define HBDRUN_SCHEMA_TYPE_ACTION           "action"

#define HBDRUN_SCHEMA_TYPE_WINDOWS          "windows" /* used to switch window */

#define CONFIRM_BTN_TEXT_ACCEPT_ONCE        "接受一次"
#define CONFIRM_BTN_TEXT_ACCEPT_ALWAYS      "始终接受"
#define CONFIRM_BTN_TEXT_DECLINE            "拒绝连接"

typedef void (*hbdrun_handler)(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri);

#if PLATFORM(MINIGUI)
extern HWND g_xgui_main_window;
extern HWND g_xgui_floating_window;
#endif


static const char *error_page =
    "<html><body><h1>%d : %s</h1></body></html>";

static const char *confirm_success = "{'code':200}";

/* has_rdr, title */
static const char *runners_page_tmpl_prefix = ""
"<!DOCTYPE html>"
"<html lang='zh-CN'>"
"    <head>"
"        <meta http-equiv='Content-Type' content='text/html; charset=UTF-8'>"
"        <meta name='viewport' content='width=device-width, initial-scale=1'>"
"        <!-- Bootstrap core CSS -->"
"        <link rel='stylesheet' href='hvml://localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/css/bootstrap.min.css' />"
"        <script type='text/javascript' src='hvml://localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/js/bootstrap.bundle.min.js'></script>"
""
"        <script>"
"            var has_rdr = %s;"
"            var rdr_dialog;"
"            function on_change(e) {"
"                var result = e.target.getAttribute('data-link-btn');"
"                const button = document.getElementById(result);"
"                var select_count = button.getAttribute('data-select-count');"
"                if (e.target.checked) {"
"                    select_count++;"
"                }"
"                else {"
"                    select_count--;"
"                }"
"                if (select_count > 0 && has_rdr) {"
"                    button.removeAttribute('disabled');"
"                }"
"                else {"
"                    button.setAttribute('disabled', '');"
"                }"
"                button.setAttribute('data-select-count', select_count)"
"            }"
"            window.addEventListener('load', (event) => {"
"                const inpputElems = document.querySelectorAll('input');"
"                inpputElems.forEach((item) => {"
"                    item.addEventListener('change', on_change);"
"                });"
""
"                rdr_dialog = new bootstrap.Modal('#id_rdr_dialog');"
"            });"
""
"            function on_card_switch_click(e) {"
"                var link_id = e.getAttribute('data-link-id');"
"                var switch_now_btn = document.getElementById('id_switch_now_btn');"
"                switch_now_btn.setAttribute('data-link-id', link_id);"
"                rdr_dialog.show();"
"            }"
""
"            function on_rdr_item_click(e) {"
"                if (e.target && e.target.nodeName == 'LI') {"
"                    let current = document.getElementsByClassName('active');"
"                    if (current.length > 0) {"
"                        current[0].classList.remove('active');"
"                    }"
"                    e.target.classList.add('active');"
"                }"
"            }"
""
"            function on_switch_now_click(e) {"
"                var link_id = e.getAttribute('data-link-id');"
"                var parentElem = document.getElementById(link_id);"
"                const inpputElems = parentElem.querySelectorAll('input');"
"                var param='';"
"                inpputElems.forEach((item) => {"
"                    if (item.checked) {"
"                        var endpoint = item.getAttribute('data-endpoint');"
"                        param += endpoint + ';'"
"                    }"
"                });"
"                var rdrListElem = document.getElementById('id_rdr_list');"
"                const rdrInputElems = rdrListElem.querySelectorAll('input');"
"                var activeElem = document.getElementsByClassName('active');"
"                var rdr;"
"                rdrInputElems.forEach((item) => {"
"                    if (item.checked) {"
"                        rdr = item.getAttribute('data-rdr-name');"
"                    }"
"                });"
"                do_switch_rdr(param.substr(0, param.length-1), rdr);"
"                rdr_dialog.hide();"
"            }"
""
"            function on_close_page_click() {"
"                window.close();"
"            }"
""
"            function on_action_result() {"
"                if (httpRequest.readyState === XMLHttpRequest.DONE) {"
"                    window.close();"
"                }"
"            }"
""
"            function do_switch_rdr(endpoints, rdr) {"
"                var uri = 'hbdrun://action?type=switchRdr&endpoints=' + endpoints+ '&rdr=' + rdr;"
"                httpRequest = new XMLHttpRequest();"
"                httpRequest.onreadystatechange = on_action_result;"
"                httpRequest.open('POST', uri);"
"                httpRequest.send();"
"            }"
"        </script>"
"        <style>"
"            .w-95 {"
"                width: 95%!important;"
"            }"
"        </style>"
"    </head>"
"    <body>"
"        <main>"
"            <header class='py-3 mb-4 border-bottom'>"
"                <div class='container d-flex flex-wrap justify-content-between'>"
"                  <div class='d-flex align-items-center mb-3 mb-lg-0 me-lg-auto link-body-emphasis text-decoration-none'>"
"                    <img class='me-2' width='32' height='32' src='hvml://localhost/_renderer/_builtin/-/assets/favicon.ico' />"
"                    <span class='fs-4'>所有应用</span>"
"                  </div>"
"                  <button type='button' class='btn-close mb-3 mb-lg-0 me-lg-auto ' onclick='on_close_page_click()'></button>"
"                </div>"
"            </header>"
"            <div class='container px-4' id='custom-cards'>"
""
"                <div class='row row-cols-1 row-cols-lg-2 row-cols-xl-3 align-items-stretch g-4 py-2'>"
"";

static const char *runners_page_tmpl_suffix = ""
"                </div>"
""
"            </div>"
"            <div class='modal fade' id='id_rdr_dialog' tabindex='-1' aria-labelledby='id_rdr_dialog_title' aria-hidden='true'>"
"                <div class='modal-dialog modal-dialog-centered  modal-dialog-scrollable'>"
"                    <div class='modal-content'>"
"                        <div class='modal-header'>"
"                            <h1 class='modal-title fs-5' id='id_rdr_dialog_title'>%s</h1>"
"                            <button type='button' class='btn-close' data-bs-dismiss='modal' aria-label='Close'></button>"
"                        </div>"
"                        <div class='modal-body'>"
"                            <div class='list-group' id='id_rdr_list'>"
"%s"
"                            </div>"
"                        </div>"
"                        <button type='button' class='btn btn-primary m-3' id='id_switch_now_btn' data-link-id='' onclick='on_switch_now_click(this)' %s>立即切换</button>"
"                    </div>"
"                </div>"
"            </div>"
"        </main>"
"    </body>"
"</html>"
"";

static const char *runners_rdr_tmpl = ""
"                                <div class='list-group-item'>"
"                                    <input class='form-check-input' type='radio' name='runner_rdrs' data-rdr-name='%s' id='id-radio-%d'/>"
"                                    <label for='id-radio-%d'>%s</label>"
"                                </div>"
"";

static const char *runners_rdr_active_tmpl = ""
"                                <div class='list-group-item'>"
"                                    <input class='form-check-input' type='radio' name='runner_rdrs' data-rdr-name='%s' id='id-radio-%d' checked/>"
"                                    <label for='id-radio-%d'>%s</label>"
"                                </div>"
"";

/* disabled, runner_id, app_name, endpoint, runner_id, runner label, runner endpoint */
static const char *runner_template = ""
"                                <div class='list-group-item list-group-item-action d-flex %s' >"
"                                    <div>"
"                                        <input class='form-check-input me-1 h5' type='checkbox' value='' id='id_runner_%d' data-link-btn='id_btn_%s' data-endpoint='%s'>"
"                                    </div>"
"                                    <div class='w-95'>"
"                                        <label class='form-check-label w-95' for='id_runner_%d'>"
"                                            <span class='h5'>%s</span>"
"                                            <p class='mb-1 text-truncate'>%s</p>"
"                                        </label>"
"                                    </div>"
"                                </div>"
"";

/* icon, app label, desc, app name */
static const char *runners_card_tmpl_prefix = ""
"                    <div class='col mx-auto'>"
"                        <div class='card card-cover h-100 overflow-hidden text-bg-light rounded-4 shadow-lg p-3'>"
"                            <div class='d-inline-flex  align-items-center'>"
"                                <div class='rounded d-inline-flex align-items-center justify-content-center fs-2 mb-3'>"
"                                    <img class='d-block mx-auto rounded' src='%s' alt='' width='72' height='72' />"
"                                </div>"
"                                <h3 class='fs-2 ms-2'>%s</h3>"
"                            </div>"
"                            <p>%s</p>"
"                            <div class='list-group' id='id_rdrlist_%s'>"
"";

/* app_name, app_name, button */
static const char *runners_card_tmpl_suffix = ""
"                            </div>"
"                            <button type='button' class='btn btn-primary m-3' id='id_btn_%s' data-link-id='id_rdrlist_%s' onclick='on_card_switch_click(this)' data-select-count=0 disabled>%s</button>"
"                        </div>"
"                    </div>"
"";

/* timeout, decline, icon url, app label, desc, accept once, accept once, accept once, accept always, accept always, Decline  */
static const char *confirm_page_template = ""
"<!DOCTYPE html>"
"<html lang='zh-CN'>"
"    <head>"
"        <meta http-equiv='Content-Type' content='text/html; charset=UTF-8'>"
"        <meta name='viewport' content='width=device-width, initial-scale=1'>"
"        <!-- Bootstrap core CSS -->"
"        <link rel='stylesheet' href='hvml://localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/css/bootstrap.min.css' />"
"        <script type='text/javascript' src='hvml://localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/js/bootstrap.bundle.min.js'></script>"
""
"        <script>"
"            let httpRequest;"
"            let timerId;"
"            let timeout = %s;"
"            let decline = '%s';"
""
"            window.addEventListener('load', (event) => {"
"                timerId = setInterval(function(){"
"                    const btn = document.getElementById('id_decline');"
"                    btn.textContent = decline + '(' + timeout + ')';"
"                    timeout = timeout - 1;"
"                    if (timeout == 0) {"
"                        clearInterval(timerId);"
"                        on_result(btn);"
"                    }"
"                }, 1000);"
"            });"
"            function on_radio_change(elem) {"
"                const btn = document.getElementById('id_accept');"
"                btn.textContent = elem.value;"
"                var result = elem.getAttribute('data-result');"
"                btn.setAttribute('data-result', result);"
"            }"
""
"            function on_action_result() {"
"                if (httpRequest.readyState === XMLHttpRequest.DONE) {"
"                    window.close();"
"                }"
"            }"
""
"            function on_result(elem)"
"            {"
"                var result = elem.getAttribute('data-result');"
"                var uri = 'hbdrun://action?type=confirm&result=' + result;"
"                httpRequest = new XMLHttpRequest();"
"                httpRequest.onreadystatechange = on_action_result;"
"                httpRequest.open('POST', uri);"
"                httpRequest.send();"
"            }"
"        </script>"
""
"        <style>"
"            html,body{"
"                height:100%;"
"                padding:0;"
"                margin:0;"
"            }"
"        </style>"
"    </head>"
"    <body>"
"        <header class='py-3 mb-4 border-bottom'>"
"            <div class='container d-flex flex-wrap justify-content-start'>"
"              <div class='d-flex align-items-center mb-3 mb-lg-0 me-lg-auto link-body-emphasis text-decoration-none'>"
"                <img class='me-2' width='32' height='32' src='hvml://localhost/_renderer/_builtin/-/assets/favicon.ico' />"
"                <span class='fs-4'>收到应用展现请求，是否允许展现远程应用？</span>"
"              </div>"
"            </div>"
"        </header>"
"        <div class='px-4 text-center w-100 d-flex flex-column align-items-center justify-content-center'>"
"            <img class='d-block mx-auto mb-4' src='%s' alt='' width='72' height='57'>"
"            <h1 class='display-5 fw-bold'>%s</h1>"
"            <div class='col-lg-6 mx-auto'>"
"                <p class='lead mb-4'>%s</p>"
"                <div class='d-grid gap-2 d-flex justify-content-around'>"
"                    <div class='btn-group'>"
"                        <button type='button' class='btn btn-primary' id='id_accept' data-result='AcceptOnce' onclick='on_result(this)'>%s</button>"
"                        <button type='button' class='btn btn-primary dropdown-toggle dropdown-toggle-split' data-bs-toggle='dropdown' aria-expanded='false'>"
"                        </button>"
"                        <ul class='dropdown-menu'>"
"                            <li>"
"                                <div class='form-check mx-1'>"
"                                    <input class='form-check-input' type='radio' name='acceptRadio' id='id_accept_once' data-result='AcceptOnce' value='%s' onchange='on_radio_change(this)' checked>"
"                                    <label class='form-check-label' for='id_accept_once'>"
"                                        %s"
"                                    </label>"
"                                </div>"
"                            </li>"
"                            <li>"
"                                <div class='form-check mx-1'>"
"                                    <input class='form-check-input' type='radio' name='acceptRadio' id='id_accept_always' data-result='AcceptAlways' value='%s' onchange='on_radio_change(this)' >"
"                                    <label class='form-check-label' for='id_accept_always'>"
"                                        %s"
"                                    </label>"
"                                </div>"
"                            </li>"
"                        </ul>"
"                    </div>"
"                    <button type='button' class='btn btn-outline-secondary' data-result='Decline' id='id_decline' onclick='on_result(this)'>%s</button>"
"                </div>"
"            </div>"
"        </div>"
"    </body>"
"</html>"
"";

static const char *windows_page_tmpl_prefix = ""
"<!DOCTYPE html>"
"<html lang='zh-CN'>"
"    <head>"
"        <meta http-equiv='Content-Type' content='text/html; charset=UTF-8'>"
"        <meta name='viewport' content='width=device-width, initial-scale=1'>"
"        <!-- Bootstrap core CSS -->"
"        <link rel='stylesheet' href='hvml://localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/css/bootstrap.min.css' />"
"        <script type='text/javascript' src='hvml://localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/js/bootstrap.bundle.min.js'></script>"
""
"        <script>"
"            function on_action_result() {"
"                if (httpRequest.readyState === XMLHttpRequest.DONE) {"
"                    window.close();"
"                }"
"            }"
""
"            function on_item_click(e) {"
"                var handle = e.getAttribute('data-handle');"
"                var uri = 'hbdrun://action?type=switchWindow&handle=' + handle;"
"                httpRequest = new XMLHttpRequest();"
"                httpRequest.onreadystatechange = on_action_result;"
"                httpRequest.open('POST', uri);"
"                httpRequest.send();"
"            }"
""
"            function on_close_page_click() {"
"                window.close();"
"            }"
"        </script>"
""
"    </head>"
"    <body>"
"        <main>"
"            <header class='py-3 mb-4 border-bottom'>"
"                <div class='container d-flex flex-wrap justify-content-between'>"
"                  <div class='d-flex align-items-center mb-3 mb-lg-0 me-lg-auto link-body-emphasis text-decoration-none'>"
"                    <img class='me-2' width='32' height='32' src='hvml://localhost/_renderer/_builtin/-/assets/favicon.ico' />"
"                    <span class='fs-4'>所有窗口</span>"
"                  </div>"
"                  <button type='button' class='btn-close mb-3 mb-lg-0 me-lg-auto ' onclick='on_close_page_click()'></button>"
"                </div>"
"            </header>"
"            <div class='container px-4' id='custom-cards'>"
"                <div class='row row-cols-1 row-cols-lg-2 row-cols-xl-3 align-items-stretch g-4 py-2'>"
"";

static const char *windows_page_tmpl_suffix = ""
"                </div>"
"            </div>"
"        </main>"
"    </body>"
"</html>"
"";

static const char *dup_confirm_page_template = ""
"<!DOCTYPE html>"
"<html lang='zh-CN'>"
"    <head>"
"        <meta http-equiv='Content-Type' content='text/html; charset=UTF-8'>"
"        <meta name='viewport' content='width=device-width, initial-scale=1'>"
"        <!-- Bootstrap core CSS -->"
"        <link rel='stylesheet' href='hvml://localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/css/bootstrap.min.css' />"
"        <script type='text/javascript' src='hvml://localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/js/bootstrap.bundle.min.js'></script>"
"        <script>"
"            let httpRequest;"
"            function on_radio_change(elem) {"
"                const btn = document.getElementById('id_accept');"
"                btn.textContent = elem.value;"
"                var result = elem.getAttribute('data-result');"
"                btn.setAttribute('data-result', result);"
"            }"
"            function on_action_result() {"
"                if (httpRequest.readyState === XMLHttpRequest.DONE) {"
"                    window.close();"
"                }"
"            }"
"            function on_result(elem) {"
"                var result = elem.getAttribute('data-result');"
"                var uri = 'hbdrun://action?type=dupConfirm&result=' + result;"
"                httpRequest = new XMLHttpRequest();"
"                httpRequest.onreadystatechange = on_action_result;"
"                httpRequest.open('POST', uri);"
"                httpRequest.send();"
"            }"
"        </script>"
"        <style>"
"            html,body{ height:100%; padding:0; margin:0; background-color:#B3B3B3; color:white; }"
"            .btn-be {"
"              --bs-btn-color: var(--bs-white);"
"              --bs-btn-bg: #B3B3B3;"
"              --bs-btn-border-color: var(--bs-white);"
"              --bs-btn-hover-color: var(--bs-white);"
"              --bs-btn-hover-bg: #B3B3B3;"
"              --bs-btn-hover-border-color: var(--bs-white);"
"              --bs-btn-focus-shadow-rgb: var(--bs-white);"
"              --bs-btn-active-color: var(--bs-white);"
"              --bs-btn-active-bg: #C0C0C0;"
"              --bs-btn-active-border-color: var(--bs-white);"
"            }"
"        </style>"
"    </head>"
"    <body>"
"        <header class='py-3 mb-1'>"
"        </header>"
"        <div class='px-4 text-center w-100 d-flex flex-column align-items-center justify-content-center'>"
"            <h1 class='fs-4'>"
"                是否获取"
"            </h1>"
"            <div class='col-lg-6 mx-auto'>"
"                <p class='mb-4 fs-4'>"
"                    %s 投屏 ？"
"                </p>"
"                <div class='d-grid gap-2 d-flex justify-content-between'>"
"                    <button type='button' class='btn btn-sm btn-be px-4 fs-4' id='id_accept' data-result='Accept' onclick='on_result(this)'>"
"                        是"
"                    </button>"
"                    <button type='button' class='btn btn-sm btn-be px-4 fs-4' data-result='Decline' id='id_decline' onclick='on_result(this)'>"
"                        否"
"                    </button>"
"                </div>"
"            </div>"
"        </div>"
"    </body>"
"</html>"
"";

static const char *dup_close_confirm_page_template = ""
"<!DOCTYPE html>"
"<html lang='zh-CN'>"
"    <head>"
"        <meta http-equiv='Content-Type' content='text/html; charset=UTF-8'>"
"        <meta name='viewport' content='width=device-width, initial-scale=1'>"
"        <!-- Bootstrap core CSS -->"
"        <link rel='stylesheet' href='hvml://localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/css/bootstrap.min.css' />"
"        <script type='text/javascript' src='hvml://localhost/_renderer/_builtin/-/assets/bootstrap-5.3.1-dist/js/bootstrap.bundle.min.js'></script>"
"        <script>"
"            let httpRequest;"
"            function on_radio_change(elem) {"
"                const btn = document.getElementById('id_accept');"
"                btn.textContent = elem.value;"
"                var result = elem.getAttribute('data-result');"
"                btn.setAttribute('data-result', result);"
"            }"
"            function on_action_result() {"
"                if (httpRequest.readyState === XMLHttpRequest.DONE) {"
"                    window.close();"
"                }"
"            }"
"            function on_result(elem) {"
"                var result = elem.getAttribute('data-result');"
"                var uri = 'hbdrun://action?type=dupConfirm&result=' + result;"
"                httpRequest = new XMLHttpRequest();"
"                httpRequest.onreadystatechange = on_action_result;"
"                httpRequest.open('POST', uri);"
"                httpRequest.send();"
"            }"
"        </script>"
"        <style>"
"            html,body{ height:100%; padding:0; margin:0; background-color:#B3B3B3; color:white; }"
"            .btn-be {"
"              --bs-btn-color: var(--bs-white);"
"              --bs-btn-bg: #B3B3B3;"
"              --bs-btn-border-color: var(--bs-white);"
"              --bs-btn-hover-color: var(--bs-white);"
"              --bs-btn-hover-bg: #B3B3B3;"
"              --bs-btn-hover-border-color: var(--bs-white);"
"              --bs-btn-focus-shadow-rgb: var(--bs-white);"
"              --bs-btn-active-color: var(--bs-white);"
"              --bs-btn-active-bg: #C0C0C0;"
"              --bs-btn-active-border-color: var(--bs-white);"
"            }"
"        </style>"
"    </head>"
"    <body>"
"        <header class='py-3 mb-1'>"
"        </header>"
"        <div class='px-4 text-center w-100 d-flex flex-column align-items-center justify-content-center'>"
"            <h1 class='fs-4'>"
"            </h1>"
"            <div class='col-lg-6 mx-auto'>"
"                <p class='mb-4 fs-4'>"
"                    是否中止投屏 ？"
"                </p>"
"                <div class='d-grid gap-2 d-flex justify-content-between'>"
"                    <button type='button' class='btn btn-sm btn-be px-4 fs-4' id='id_accept' data-result='Accept' onclick='on_result(this)'>"
"                        是"
"                    </button>"
"                    <button type='button' class='btn btn-sm btn-be px-4 fs-4' data-result='Decline' id='id_decline' onclick='on_result(this)'>"
"                        否"
"                    </button>"
"                </div>"
"            </div>"
"        </div>"
"    </body>"
"</html>"
"";

#if PLATFORM(MINIGUI)
/* handle, title */
static const char *windows_card_tmpl = ""
"                    <div class='col mx-auto'>"
"                        <div class='card card-cover h-100 overflow-hidden text-bg-light rounded-4 shadow-lg p-3' data-handle='%lx' onclick='on_item_click(this)'>"
"                            <h3 class='fs-2 ms-2'>%s</h3>"
"                        </div>"
"                    </div>"
"";
#endif

static void send_response(WebKitURISchemeRequest *request, guint status_code,
        const char *content_type, char *contents, size_t nr_contents,
        GDestroyNotify notify)
{
    GInputStream *stream = NULL;
    WebKitURISchemeResponse *response = NULL;;

    stream = g_memory_input_stream_new_from_data(contents, nr_contents, notify);
    response = webkit_uri_scheme_response_new(stream, nr_contents);

    webkit_uri_scheme_response_set_status(response, status_code, NULL);
    webkit_uri_scheme_response_set_content_type(response, content_type);
    webkit_uri_scheme_request_finish_with_response(request, response);

    g_object_unref(response);
    g_object_unref(stream);
}

static void send_error_response(WebKitURISchemeRequest *request, guint status_code,
        const char *content_type, char *err_info, size_t nr_err_info,
        GDestroyNotify notify)
{
    char *contents = g_strdup_printf(error_page, status_code, err_info);
    send_response(request, status_code, content_type, contents, strlen(contents),
            g_free);
    if (notify) {
        notify(err_info);
    }
}

static void on_hbdrun_versions(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;
}

static void on_hbdrun_apps(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;
}

static void on_hbdrun_store(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;
}

static void on_hbdrun_runners(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;
    const char *icon = "hvml://localhost/_renderer/_builtin/-/assets/hvml.png";
    const char *title = "所有应用";
    const char *switch_btn = "切换";
    const char *rdr_title = "可展现设备";
    const char *rdr_empty = "未发现可用设备";

    struct kvlist app_list;
    kvlist_init(&app_list, NULL);

    struct purcmc_server *server = xguitls_get_purcmc_server();

    bool has_rdr = !kvlist_is_empty(&server->dnssd_rdr_list);
    GOutputStream *page_stream = NULL;
    page_stream = g_memory_output_stream_new(NULL, 0, g_realloc, g_free);
    g_output_stream_printf(page_stream, NULL, NULL, NULL,
            runners_page_tmpl_prefix, has_rdr?"true":"true", title);

    char *err_info = NULL;
    const char *name;
    void *next, *data;
    int idx = 0;
    purcmc_endpoint *endpoint;
    kvlist_for_each_safe(&server->endpoint_list, name, next, data) {
        endpoint = *(purcmc_endpoint **)data;
        void *data = kvlist_get(&app_list, endpoint->app_name);
        if (strcmp(endpoint->runner_name, "unknown") == 0) {
            continue;
        }
        GOutputStream *stream = data ?  *(GOutputStream **) data : NULL;
        if (!stream) {
            stream = g_memory_output_stream_new(NULL, 0, g_realloc, g_free);
            if (!stream) {
                err_info = g_strdup_printf("Can not allocate memory for runner page");
                goto error;
            }
            kvlist_set(&app_list, endpoint->app_name, &stream);
            const char *img = endpoint->app_icon ? endpoint->app_icon : icon;
            g_output_stream_printf(stream, NULL, NULL, NULL,
                runners_card_tmpl_prefix,
                img,
                endpoint->app_label, endpoint->app_desc,
                endpoint->app_name);
        }
        g_output_stream_printf(stream, NULL, NULL, NULL, runner_template,
                endpoint->allow_switching_rdr ? "" : "disabled",
                idx, endpoint->app_name, name, idx, endpoint->runner_label, name);
        idx++;
    }

    kvlist_for_each_safe(&app_list, name, next, data) {
        GOutputStream *stream = *(GOutputStream **)data;
        g_output_stream_printf(stream, NULL, NULL, NULL,
                runners_card_tmpl_suffix, name, name, switch_btn);
        gpointer *cards = g_memory_output_stream_get_data(
                G_MEMORY_OUTPUT_STREAM(stream));
        gsize size = g_memory_output_stream_get_size(
                G_MEMORY_OUTPUT_STREAM(stream));
        g_output_stream_write(page_stream, cards, size, NULL, NULL);
        g_object_unref(stream);
        kvlist_delete(&app_list, name);
    }
    kvlist_free(&app_list);


    if (has_rdr) {
        GOutputStream *stream;
        stream = g_memory_output_stream_new(NULL, 0, g_realloc, g_free);
        struct dnssd_rdr *rdr;
        int idx = 0;
        kvlist_for_each_safe(&server->dnssd_rdr_list, name, next, data) {
            rdr = *(struct dnssd_rdr **)data;
            if (idx == 0) {
                g_output_stream_printf(stream, NULL, NULL, NULL,
                    runners_rdr_active_tmpl, name, idx, idx, rdr->hostname);
            }
            else {
                g_output_stream_printf(stream, NULL, NULL, NULL,
                    runners_rdr_tmpl, name, idx, idx, rdr->hostname);
            }
            idx++;
        }
        gpointer *data = g_memory_output_stream_get_data(
                G_MEMORY_OUTPUT_STREAM(stream));
        g_output_stream_printf(page_stream, NULL, NULL, NULL,
                    runners_page_tmpl_suffix, rdr_title, (char*)data, "");
        g_object_unref(stream);
    }
    else {
        g_output_stream_printf(page_stream, NULL, NULL, NULL,
                    runners_page_tmpl_suffix, rdr_title, rdr_empty, "disabled");
    }

    g_output_stream_close(page_stream, NULL, NULL);
    gsize size = g_memory_output_stream_get_size(
            G_MEMORY_OUTPUT_STREAM(page_stream));
    data = g_memory_output_stream_steal_data(
            G_MEMORY_OUTPUT_STREAM(page_stream));
    send_response(request, 200, "text/html", (char*)data, size, g_free);
    if (page_stream) {
        g_object_unref(page_stream);
    }
    return;

error:
    if (err_info) {
        send_error_response(request, 500, "text/html", err_info, strlen(err_info), g_free);
    }
    if (page_stream) {
        g_object_unref(page_stream);
    }
}

static void on_hbdrun_dup_confirm(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    char *err_info = NULL;

    size_t nr_uri = strlen(uri) + 1;
    char label[nr_uri];
    hbdrun_uri_get_query_value(uri, CONFIRM_PARAM_LABEL, label);
    char *app_label = g_uri_unescape_string(label, NULL);

    char *contents = g_strdup_printf(dup_confirm_page_template, app_label);

    if (!contents) {
        err_info = g_strdup_printf("Can not allocate memory for confirm page (%s)", uri);
        LOG_WARN("Can not allocate memory for confirm page (%s)", uri);
        goto error;
    }
    send_response(request, 200, "text/html", contents, strlen(contents), g_free);
    return;

error:
    if (err_info) {
        send_error_response(request, 500, "text/html", err_info, strlen(err_info), g_free);
    }
    if (contents) {
        g_free(contents);
    }
}

static void on_hbdrun_dup_close_confirm(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    char *err_info = NULL;
    char *contents = g_strdup(dup_close_confirm_page_template);

    if (!contents) {
        err_info = g_strdup_printf("Can not allocate memory for confirm page (%s)", uri);
        LOG_WARN("Can not allocate memory for confirm page (%s)", uri);
        goto error;
    }
    send_response(request, 200, "text/html", contents, strlen(contents), g_free);
    return;

error:
    if (err_info) {
        send_error_response(request, 500, "text/html", err_info, strlen(err_info), g_free);
    }
    if (contents) {
        g_free(contents);
    }
}

static void on_hbdrun_confirm(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;
    size_t nr_uri = strlen(uri) + 1;

    char *type = NULL;
    if (hbdrun_uri_get_query_value_alloc(uri, "type", &type)) {
        if (strcasecmp(type, CONFIRM_TYPE_DUP) == 0) {
            on_hbdrun_dup_confirm(request, webContext, uri);
            free(type);
            return;
        }
        else if (strcasecmp(type, CONFIRM_TYPE_DUP_CLOSE) == 0) {
            on_hbdrun_dup_close_confirm(request, webContext, uri);
            free(type);
            return;
        }
        free(type);
    }

    char label[nr_uri];
    hbdrun_uri_get_query_value(uri, CONFIRM_PARAM_LABEL, label);
    char *app_label = g_uri_unescape_string(label, NULL);

    char desc[nr_uri];
    hbdrun_uri_get_query_value(uri, CONFIRM_PARAM_DESC, desc);
    char *app_desc = g_uri_unescape_string(desc, NULL);

    char icon[nr_uri];
    hbdrun_uri_get_query_value(uri, CONFIRM_PARAM_ICON, icon);
    char *app_icon = g_uri_unescape_string(icon, NULL);

    char timeout[nr_uri];
    hbdrun_uri_get_query_value(uri, CONFIRM_PARAM_TIMEOUT, timeout);
    char *s_timeout = g_uri_unescape_string(timeout, NULL);

    const char *accept_once = CONFIRM_BTN_TEXT_ACCEPT_ONCE;
    const char *accept_always = CONFIRM_BTN_TEXT_ACCEPT_ALWAYS;
    const char *decline = CONFIRM_BTN_TEXT_DECLINE;

    /* timeout, decline, icon url, app label, desc, accept once,
     * accept once, accept once, accept always, accept always, Decline  */
    char *err_info = NULL;
    char *contents = g_strdup_printf(confirm_page_template, timeout, decline,
            app_icon, app_label, app_desc, accept_once, accept_once, accept_once,
            accept_always, accept_always, decline);

    g_free(app_label);
    g_free(app_desc);
    g_free(app_icon);
    g_free(s_timeout);

    if (!contents) {
        err_info = g_strdup_printf("Can not allocate memory for confirm page (%s)", uri);
        LOG_WARN("Can not allocate memory for confirm page (%s)", uri);
        goto error;
    }
    send_response(request, 200, "text/html", contents, strlen(contents), g_free);
    return;

error:
    if (err_info) {
        send_error_response(request, 500, "text/html", err_info, strlen(err_info), g_free);
    }
    if (contents) {
        g_free(contents);
    }
}

static void on_hbdrun_action_confirm(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    char *err_info = NULL;
    char *result = NULL;
    if (!hbdrun_uri_get_query_value_alloc(uri,
                BROWSER_HBDRUN_ACTION_PARAM_RESULT, &result)) {
        err_info = g_strdup_printf("invalid result param (%s)", uri);
        goto error;
    }

    WebKitWebView *webview = webkit_uri_scheme_request_get_web_view(request);
    /* result will free at: xgutils_show_confirm_window */
    g_object_set_data(G_OBJECT(webview), BROWSER_HBDRUN_ACTION_PARAM_RESULT,
            result);

    send_response(request, 200, "application/json", (char *)confirm_success,
            strlen(confirm_success), NULL);
    return;

error:
    if (err_info) {
        send_error_response(request, 500, "text/html", err_info, strlen(err_info), g_free);
    }

    if (result) {
        g_free(result);
    }
}

static void on_hbdrun_action_switch_rdr(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    char *err_info = NULL;
    char *endpoints = NULL;
    char *rdr = NULL;
    if (!hbdrun_uri_get_query_value_alloc(uri,
                BROWSER_HBDRUN_ACTION_PARAM_ENDPOINTS, &endpoints)) {
        err_info = g_strdup_printf("invalid endpoints param (%s)", uri);
        goto error;
    }

    if (!hbdrun_uri_get_query_value_alloc(uri,
                BROWSER_HBDRUN_ACTION_PARAM_RDR, &rdr)) {
        err_info = g_strdup_printf("invalid rdr param (%s)", uri);
        goto error;
    }

    struct purcmc_server *server = xguitls_get_purcmc_server();
    void *data = kvlist_get(&server->dnssd_rdr_list, rdr);
    struct dnssd_rdr *dnssd_rdr = *(struct dnssd_rdr **)data;

    const char *line = endpoints;
    const char *next = NULL;
    int len = 0;
    while (line) {
        if ((next = strstr (line, ";")) != NULL) {
            len = (next - line);
        }
        else {
            len = strlen (line);
        }

        if (len <= 0) {
            break;
        }

        char tmp[len + 1];
        memcpy(tmp, line, len);
        tmp[len] = '\0';

        purcmc_endpoint *endpoint = purcmc_endpoint_from_name(server, tmp);
        if (endpoint) {
            switch_new_renderer(server, endpoint, dnssd_rdr->hostname,
                    dnssd_rdr->port);
        }

        line = next ? (next + 1) : NULL;
    }

    send_response(request, 200, "application/json", (char *)confirm_success,
            strlen(confirm_success), NULL);
    if (rdr) {
        g_free(rdr);
    }

    if (endpoints) {
        g_free(endpoints);
    }
    return;

error:
    if (err_info) {
        send_error_response(request, 500, "text/html", err_info, strlen(err_info), g_free);
    }

    if (rdr) {
        g_free(rdr);
    }

    if (endpoints) {
        g_free(endpoints);
    }
}

static void on_hbdrun_action_switch_window(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    char *err_info = NULL;
    char *handle = NULL;
    if (!hbdrun_uri_get_query_value_alloc(uri,
                BROWSER_HBDRUN_ACTION_PARAM_HANDLE, &handle)) {
        err_info = g_strdup_printf("invalid endpoints param (%s)", uri);
        goto error;
    }

#if PLATFORM(MINIGUI)
    uint64_t u64v = strtoull(handle, NULL, 16);
    HWND hWnd = (HWND) (uintptr_t)u64v;
    ShowWindow(hWnd, SW_SHOWNORMAL);
#endif

    send_response(request, 200, "application/json", (char *)confirm_success,
            strlen(confirm_success), NULL);

    if (handle) {
        g_free(handle);
    }
    return;
error:
    if (err_info) {
        send_error_response(request, 500, "text/html", err_info, strlen(err_info), g_free);
    }

    if (handle) {
        g_free(handle);
    }
}

static void on_hbdrun_action_dup_confirm(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    char *err_info = NULL;
    char *result = NULL;
    if (!hbdrun_uri_get_query_value_alloc(uri,
                BROWSER_HBDRUN_ACTION_PARAM_RESULT, &result)) {
        err_info = g_strdup_printf("invalid result param (%s)", uri);
        goto error;
    }

    WebKitWebView *webview = webkit_uri_scheme_request_get_web_view(request);
    /* result will free at: xgutils_show_confirm_window */
    g_object_set_data(G_OBJECT(webview), BROWSER_HBDRUN_ACTION_PARAM_RESULT,
            result);

    send_response(request, 200, "application/json", (char *)confirm_success,
            strlen(confirm_success), NULL);
    return;

error:
    if (err_info) {
        send_error_response(request, 500, "text/html", err_info, strlen(err_info), g_free);
    }

    if (result) {
        g_free(result);
    }
}

static void on_hbdrun_action(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;
    char *err_info = NULL;
    char *type = NULL;
    if (!hbdrun_uri_get_query_value_alloc(uri,
                BROWSER_HBDRUN_ACTION_PARAM_TYPE, &type)) {
        err_info = g_strdup_printf("invalid rquest type (%s)", uri);
        goto error;
    }

    /* confirm */
    if (strcasecmp(type, BROWSER_HBDRUN_ACTION_TYPE_CONFIRM) == 0) {
        on_hbdrun_action_confirm(request, webContext, uri);
    }
    else if (strcasecmp(type, BROWSER_HBDRUN_ACTION_TYPE_SWITCH_RDR) == 0) {
        on_hbdrun_action_switch_rdr(request, webContext, uri);
    }
    else if (strcasecmp(type, BROWSER_HBDRUN_ACTION_TYPE_SWITCH_WINDOW) == 0) {
        on_hbdrun_action_switch_window(request, webContext, uri);
    }
    else if (strcasecmp(type, BROWSER_HBDRUN_ACTION_TYPE_DUP_CONFIRM) == 0) {
        on_hbdrun_action_dup_confirm(request, webContext, uri);
    }

    if (type) {
        g_free(type);
    }
    return;

error:
    if (err_info) {
        send_error_response(request, 500, "text/html", err_info, strlen(err_info), g_free);
    }

    if (type) {
        g_free(type);
    }
}

static void on_hbdrun_windows(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext, const char *uri)
{
    (void) request;
    (void) webContext;
    (void) uri;

    GOutputStream *stream;
    stream = g_memory_output_stream_new(NULL, 0, g_realloc, g_free);
    g_output_stream_write(stream, windows_page_tmpl_prefix,
            strlen(windows_page_tmpl_prefix), NULL, NULL);

#if PLATFORM(MINIGUI)
    HWND hWnd = HWND_NULL;
    while ((hWnd = GetNextMainWindow(hWnd)) != HWND_NULL) {
        if (hWnd != g_xgui_main_window && hWnd != g_xgui_floating_window) {
            DWORD exStyle = GetWindowExStyle(hWnd);
            if (exStyle & WS_EX_WINTYPE_NORMAL) {
                g_output_stream_printf(stream, NULL, NULL, NULL, windows_card_tmpl,
                        (uint64_t)(uintptr_t)hWnd, GetWindowCaption(hWnd));
            }
        }
    }
#endif

    g_output_stream_write(stream, windows_page_tmpl_suffix,
            strlen(windows_page_tmpl_suffix), NULL, NULL);

    g_output_stream_close(stream, NULL, NULL);
    gsize size = g_memory_output_stream_get_size(
            G_MEMORY_OUTPUT_STREAM(stream));
    void *data = g_memory_output_stream_steal_data(
            G_MEMORY_OUTPUT_STREAM(stream));
    send_response(request, 200, "text/html", (char*)data, size, g_free);
    if (stream) {
        g_object_unref(stream);
    }
}

static struct hbdrun_handler {
    const char *operation;
    hbdrun_handler handler;
} handlers[] = {
    { HBDRUN_SCHEMA_TYPE_ACTION,            on_hbdrun_action },
    { HBDRUN_SCHEMA_TYPE_APPS,              on_hbdrun_apps },
    { HBDRUN_SCHEMA_TYPE_CONFIRM,           on_hbdrun_confirm },
    { HBDRUN_SCHEMA_TYPE_RUNNERS,           on_hbdrun_runners },
    { HBDRUN_SCHEMA_TYPE_STORE,             on_hbdrun_store },
    { HBDRUN_SCHEMA_TYPE_VERSION,           on_hbdrun_versions },
    { HBDRUN_SCHEMA_TYPE_WINDOWS,           on_hbdrun_windows },
};

#define NOT_FOUND_HANDLER   ((hbdrun_handler)-1)

static hbdrun_handler find_hbdrun_handler(const char* operation)
{
    static ssize_t max = sizeof(handlers)/sizeof(handlers[0]) - 1;

    ssize_t low = 0, high = max, mid;
    while (low <= high) {
        int cmp;

        mid = (low + high) / 2;
        cmp = strcasecmp(operation, handlers[mid].operation);
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

    return NOT_FOUND_HANDLER;

found:
    return handlers[mid].handler;
}

void hbdrunURISchemeRequestCallback(WebKitURISchemeRequest *request,
        WebKitWebContext *webContext)
{
    const char *uri = webkit_uri_scheme_request_get_uri(request);
    char host[PURC_LEN_HOST_NAME + 1];

    if (!hbdrun_uri_split(uri,
            host, NULL, NULL, NULL, NULL) ||
            !purc_is_valid_host_name(host)) {
        LOG_WARN("Invalid hbdrun URI (%s): bad host (%s)", uri, host);
        goto error;
    }

    WebKitWebView *webview = webkit_uri_scheme_request_get_web_view(request);
    xgutils_set_webview_density(webview);
    hbdrun_handler handler = find_hbdrun_handler(host);
    if (handler == NOT_FOUND_HANDLER) {
        LOG_WARN("Invalid hbdrun URI (%s): not found handle for '%s'", uri, host);
        goto error;
    }

    handler(request, webContext, uri);
    return;

error:
    send_error_response(request, 404, "text/html", (char*)uri, strlen(uri), NULL);
}

