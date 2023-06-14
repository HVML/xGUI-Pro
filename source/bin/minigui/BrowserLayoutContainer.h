/*
** BrowserLayoutContainer.h -- The declaration of BrowserLayoutContainer.
**
** Copyright (C) 2022, 2023 FMSoft <http://www.fmsoft.cn>
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

#ifndef BrowserLayoutContainer_h
#define BrowserLayoutContainer_h

#include <webkit2/webkit2.h>
#include "Common.h"

G_BEGIN_DECLS

#define BROWSER_TYPE_LAYOUT_CONTAINER            (browser_layout_container_get_type())
#define BROWSER_LAYOUT_CONTAINER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), BROWSER_TYPE_LAYOUT_CONTAINER, BrowserLayoutContainer))
#define BROWSER_LAYOUT_CONTAINER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  BROWSER_TYPE_LAYOUT_CONTAINER, BrowserLayoutContainerClass))
#define BROWSER_IS_LAYOUT_CONTAINER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), BROWSER_TYPE_LAYOUT_CONTAINER))
#define BROWSER_IS_LAYOUT_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  BROWSER_TYPE_LAYOUT_CONTAINER))
#define BROWSER_LAYOUT_CONTAINER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  BROWSER_TYPE_LAYOUT_CONTAINER, BrowserLayoutContainerClass))

typedef struct _BrowserLayoutContainer        BrowserLayoutContainer;
typedef struct _BrowserLayoutContainerClass   BrowserLayoutContainerClass;

typedef struct _BrowserTabbedWindow           BrowserTabbedWindow;

GType browser_layout_container_get_type(void);

BrowserLayoutContainer *browser_layout_container_new(BrowserTabbedWindow *window,
        GObject *container, const char *klass, const RECT *geometry);
HWND browser_layout_container_get_hwnd(BrowserLayoutContainer*);


G_END_DECLS

#endif
