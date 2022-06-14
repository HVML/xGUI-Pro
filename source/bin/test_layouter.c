/*
** layouter.c -- The implementation of page/window layouter.
**
** Copyright (C) 2022 FMSoft (http://www.fmsoft.cn)
**
** Author: Vincent Wei (https://github.com/VincentWei)
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

#undef NDEBUG

#include "xguipro-version.h"
#include "xguipro-features.h"

#include "utils/load-asset.h"
#include "layouter/layouter.h"

#include <string.h>
#include <errno.h>
#include <assert.h>

struct test_ctxt {
};

void *my_create_widget(void *ws_ctxt, ws_widget_type_t type,
        void *parent, const struct ws_widget_style *style)
{
    return NULL;
}

void my_destroy_widget(void *ws_ctxt, void *widget)
{
}

void my_update_widget(void *ws_ctxt,
        void *widget, const struct ws_widget_style *style)
{
}

int main(int argc, char *argv[])
{
    int retv;
    struct test_ctxt ctxt;
    struct ws_metrics metrics = { 1024, 768, 96, 1 };

    char *html;
    size_t len_html;
    html = load_asset_content(NULL, NULL,
                "test_layouter.html", &len_html);
    assert(html);

    struct ws_layouter *layouter;
    layouter = ws_layouter_new(&metrics, html, len_html, &ctxt,
        my_create_widget, my_destroy_widget, my_update_widget,
        &retv);

    if (layouter == NULL) {
        return retv;
    }

    ws_layouter_delete(layouter);

    return 0;
}
