![GitHub Repo stars](https://img.shields.io/github/stars/hvml/xgui-pro?style=for-the-badge&labelColor=%2300B0F0)
![GitHub](https://img.shields.io/github/license/hvml/xgui-pro?style=for-the-badge&labelColor=%2300B0F0)
![GitHub top language](https://img.shields.io/github/languages/top/hvml/xgui-pro?style=for-the-badge&labelColor=%2300B0F0)
![GitHub tag (with filter)](https://img.shields.io/github/v/tag/hvml/xgui-pro?style=for-the-badge&labelColor=%2300B0F0)
![GitHub repo size](https://img.shields.io/github/repo-size/hvml/xgui-pro?style=for-the-badge&labelColor=%2300B0F0)

# xGUI Pro

xGUI Pro is a modern, cross-platform, and advanced [HVML] renderer which is based on tailored [WebKit].

**Table of Contents**

[//]:# (START OF TOC)

- [Introduction](#introduction)
- [Release Status](#release-status)
- [Dependencies](#dependencies)
- [Building xGUI Pro](#building-xgui-pro)
- [Current Status](#current-status)
- [Authors and Contributors](#authors-and-contributors)
- [Copying](#copying)
- [Tradmarks](#tradmarks)

[//]:# (END OF TOC)

## Introduction

This software is a part of HVML project.

During the development of [HybridOS], [Vincent Wei] proposed a new-style,
       general-purpose, and easy-to-learn programming language called `HVML`.

HVML provides a totally different framework for GUI applications other than C/C++, Java, C#, or Swift.
In a complete HVML-based application framework, a standalone GUI renderer is usually included.

xGUI Pro is an open source implementation of the standalone GUI renderer for HVML programs.
It licensed under GPLv3, and it is free for commercial use if you follow the conditions and terms of GPLv3.

xGUI Pro is based on the mature browser engine - [WebKit].
By using xGUI Pro, the developers can use HTML/SVG/MathML and CSS to describe the GUI contents in their HVML programs.
We reserve the JavaScript engine of WebKit,
   so it is possible to use some popular front-end framwork such as Bootstrap to help xGUI Pro to render contents in your user interfaces.

For documents and other open source software around HVML, you can visit the following URLs:

- <https://github.com/HVML>, or
- <https://www.hvml.org>, or
- <https://hvml.fmsoft.cn>

## Release Status

[![Packaging status xguipro](https://repology.org/badge/vertical-allrepos/xguipro.svg)](https://repology.org/project/xguipro/versions)

[![Packaging status hvml-xguipro](https://repology.org/badge/vertical-allrepos/hvml-xgui-pro.svg)](https://repology.org/project/hvml-xgui-pro/versions)

## Dependencies

xGUI Pro depends on the following software:

- [WebKit2GTK 2.28.0+](https://webkitgtk.org): Use the package `libwebkit2gtk-4.0-dev` on popular Linux distributions; Use the package [webkit2-gtk-devel](https://ports.macports.org/port/webkit2-gtk-devel/) on macOS.
- [PurC 0.9.22+](https://github.com/HVML/PurC): the HVML interpreter for C language. This repository also includes two libraries to maintain a DOM tree, lay out and stylize the DOM elements by using CSS. xGUI Pro needs these libraries.

Currently, xGUI Pro can run on Linux and macOS.

Please download or fetch the source code of the above software, build and install them by following the instructions included in the software.

### Tailored WebKit

You can also try our tailored WebKit engine instead of the package shipped with the distribution. The tailored WebKit engine provide a new JavaScript API to support two HVML-specific attributes `hvml-handle` and `hvml-events`. You may obtain better performance by using the tailored WebKit engine.

The tailored WebKit engine also provides support for MiniGUI, which is a mature window system for embedded system. Note that the graphics stack and window system of HybridOS is based on MiniGUI.

- [Tailored WebKit Engine](https://files.fmsoft.cn/sources/WebKitHBD-2.34.1-20231120-040500-Source.tar.xz): to support two HVML-specific attributes `hvml-handle` and `hvml-events`. This tailored WebKit engine provides support for two ports: GTK+ and HybridOS (MiniGUI).

If you are using Ubuntu Linux, you can use the following commands to build and install the tailored WebKit to your system:

1) When using Gtk3 and Soup2 (Ubuntu 20.04):

```bash
$ mkdir -p WebKitBuild/Release && cd WebKitBuild/Release
$ cmake ../.. -DPORT=GTK -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_GAMEPAD=OFF -DENABLE_INTROSPECTION=OFF -DUSE_SOUP2=ON -DUSE_WPE_RENDERER=OFF -DUSE_LCMS=OFF -GNinja
$ ninja -j2
$ sudo ninja install
```

2) When using Gtk4 and Soup3 (Ubuntu 22.04):

```bash
$ mkdir -p WebKitBuild/Release && cd WebKitBuild/Release
$ cmake ../.. -DPORT=GTK -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_GAMEPAD=OFF -DENABLE_INTROSPECTION=OFF -DUSE_SOUP3=ON -DUSE_WPE_RENDERER=OFF -DUSE_LCMS=OFF -GNinja
$ ninja -j2
$ sudo ninja install
```

Note that you may need to install the following packages on your Ubuntu Linux:

- libenchant
- libsoup2/libsoup3
- gperf
- git-svn

For other distributions, you can visit the following URL for detailed instructions:

<https://trac.webkit.org/wiki/BuildingGtk>

Please make sure that the cmake configuration option `ENABLE_HVML_ATTRS` is enabled when you configuring the tailored WebKit.

We encourage everyone to port xGUI Pro to other platforms, such as Windows, Android, iOS, etc.

## Building and Running xGUI Pro

To build xGUI Pro from source code, please make sure that the following tools or libraries are available on your Linux or macOS system:

1. The cross-platform build system generator: CMake 3.18 or later
1. A C11 and CXX17 compliant compiler: GCC 8+ or Clang 6+
1. Glib 2.44.0 or later (the package `libglib2.0-dev` on most Linux distributions)
1. WebKit2GTK 2.28.0+ (the package `libwebkit2gtk-4.0-dev` on popular Linux distributions; the package `webkit2-gtk-devel` of macPorts on macOS).
1. WebKit2HBD 2.34.1+ as an alternative WebKit engine instead of WebKit2GTK.
1. The HVML interpterter PurC 0.9.22+.

After building and installing the dependency libraries,
      you can change to the root directory of the source tree of xGUI Pro and run the following commands:

```
$ rm -rf build && cmake -DCMAKE_BUILD_TYPE=Debug -DPORT=GTK -B build && cmake --build build
```

__NOTE__  
You may need to use the following additional options for `cmake` to use Gtk3 and Libsoup3:

```
-DUSE_GTK4=OFF -DUSE_SOUP2=OFF
```

You can run the following commands to install xGUI Pro to your system in the `build/` directory:

```
$ cd build/
$ sudo make install
```

You can also run xGUI Pro in `build/` directory without installing it to the system.
However, you need to specify the environment variable `WEBKIT_WEBEXT_DIR` as follow:

```
$ WEBKIT_WEBEXT_DIR=</path/to/your/xgui-pro/build/lib/webext> bin/xguipro
```

In the above command lines, the environment variable `WEBKIT_WEBEXT_DIR` is used to specify the directory in which the HVML extension module locates.
The building scripts will copy the HVML extension module for WebKit to the `build/` directory,
    so that you can start xGUI Pro without installing it to your system.

By default, if you do not define this environment variable,
   xGUI Pro will try to find the extension module in the sub directory called `xguipro/` in the library installation directory.
Generally, it is `/usr/local/lib/xguipro` on Linux by default.
Therefore, if you have installed xGUI Pro into your system, you can run xGUI Pro without defining this environment variable.

You can also pass the following options to your command line when running xGUI Pro:

```
  --pcmc-nowebsocket       Without support for WebSocket
  --pcmc-accesslog         Logging the verbose socket access information
  --pcmc-unixsocket=PATH   The path of the Unix-domain socket to listen on
  --pcmc-addr              The IPv4 address to bind to for WebSocket
  --pcmc-port              The port to bind to for WebSocket
  --pcmc-origin=FQDN       The origin to ensure clients send the specified origin header upon the WebSocket handshake
  --pcmc-sslcert=FILE      The path to SSL certificate
  --pcmc-sslkey=FILE       The path to SSL private key
  --pcmc-maxfrmsize=BYTES  The maximum size of a socket frame
  --pcmc-backlog=NUMBER    The maximum length to which the queue of pending connections.
```

After you start xGUI Pro, run `purc` from another terminal to execute an HVML program.
For example, in the `build/` directory of `PurC`, run the following command to start the arbitrary precision calculator:

```bash
$ cd <path/to/directory/to/build/purc>
$ bin/purc -c socket hvml/calculator-bc.hvml
```

## Debugging xGUI Pro

For security reasons, the core dump is disabled by default on some
Linux systems. The above command lines specify the core pattern, which will
be used when dumping the core of an aborted process by the kernel.

```
$ sudo su
# echo "/tmp/core-pid_%p.dump" > /proc/sys/kernel/core_pattern
# exit
$ ulimit -c unlimited
```

If encounter core dumps, use `gdb`:

```
$ gdb bin/xguipro -c /tmp/core-pid_xxxx.dump
```

Note that, `gdb` might take several minutes to load the symbols of xGUI Pro
and WebKit. So please wait patiently.

## Current Status

- Apr. 2025: Version 0.9.22. (PurC 0.9.22+)
- Otc. 2023: Version 0.8.4. (PurC 0.9.16+)
- Sept. 2023: Version 0.8.3. (PurC 0.9.15+)
- July 2023: Version 0.8.2. (PurC 0.9.14+)
- June 2023: Version 0.8.1. (PurC 0.9.13+)
- May 2023: Version 0.8.0. (PurC 0.9.12+)
- Apr. 2023: Version 0.7.0. (PurC 0.9.10+)
- Feb. 2023: Version 0.6.4. (PurC 0.9.7+)
- Jan. 2023: Version 0.6.3. (PurC 0.9.5+)
- Dec. 2022: Version 0.6.2.

## Authors and Contributors

- Vincent Wei (<https://github.com/VincentWei>)
- @ninexue
- @taotieren

<a href="https://github.com/hvml/xgui-pro/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=hvml/xgui-pro" />
</a>

## Copying

Copyright (C) 2022, 2025 [FMSoft Technologies]

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

## Tradmarks

1) `HVML` is a registered tradmark of [FMSoft Technologies] in China and other contries or regions.

![HVML](https://www.fmsoft.cn/application/files/8116/1931/8777/HVML256132.jpg)

2) `呼噜猫` is a registered tradmark of [FMSoft Technologies] in China and other contries or regions.

![呼噜猫](https://www.fmsoft.cn/application/files/8416/1931/8781/256132.jpg)

3) `Purring Cat` is a tradmark of [FMSoft Technologies] in China and other contries or regions.

![Purring Cat](https://www.fmsoft.cn/application/files/2816/1931/9258/PurringCat256132.jpg)

4) `PurC` is a tradmark of [FMSoft Technologies] in China and other contries or regions.

![PurC](https://www.fmsoft.cn/application/files/5716/2813/0470/PurC256132.jpg)

5) `xGUI` is a tradmark of [FMSoft Technologies] in China and other contries or regions.

![xGUI](https://www.fmsoft.cn/application/files/cache/thumbnails/7fbcb150d7d0747e702fd2d63f20017e.jpg)

[Beijing FMSoft Technologies Co., Ltd.]: https://www.fmsoft.cn
[FMSoft Technologies]: https://www.fmsoft.cn
[FMSoft]: https://www.fmsoft.cn
[HybridOS Official Site]: https://hybridos.fmsoft.cn
[HybridOS]: https://hybridos.fmsoft.cn

[HVML]: https://github.com/HVML
[MiniGUI]: http:/www.minigui.com
[WebKit]: https://webkit.org
[HTML 5.3]: https://www.w3.org/TR/html53/
[DOM Specification]: https://dom.spec.whatwg.org/
[WebIDL Specification]: https://heycam.github.io/webidl/
[CSS 2.2]: https://www.w3.org/TR/CSS22/
[CSS Box Model Module Level 3]: https://www.w3.org/TR/css-box-3/

[Vincent Wei]: https://github.com/VincentWei
