# xGUI Pro

xGUI Pro is a modern, cross-platform, and advanced [HVML] renderer which is
based on tailored [WebKit].

**Table of Contents**

[//]:# (START OF TOC)

- [Introduction](#introduction)
- [Dependencies](#dependencies)
- [Building xGUI Pro](#building-xgui-pro)
- [Current Status](#current-status)
- [TODO List](#todo-list)
- [Copying](#copying)
   + [Commercial License](#commercial-license)
   + [Special Statement](#special-statement)
- [Tradmarks](#tradmarks)

[//]:# (END OF TOC)

## Introduction

During the development of [HybridOS], [Vincent Wei] proposed a new-style,
general-purpose, and easy-to-learn programming language called `HVML`.

HVML provides a totally different framework for GUI applications other than
C/C++, Java, C#, or Swift. In a complete HVML-based application framework,
a standalone GUI renderer is usually included.

xGUI Pro is an open source implementation of the standalone GUI renderer
for HVML programs. It licensed under GPLv3, and it is free for commercial
use if you follow the terms of GPLv3.

xGUI Pro is based on the mature browser engine - [WebKit]. By using xGUI Pro,
the developers can use HTML/SVG/MathML and CSS to describe the GUI contents
in their HVML programs. We reserve the JavaScript engine of WebKit, so it is
possible to use some popular front-end framwork such as Bootstrap to help
xGUI Pro to render contents in your user interfaces.

For documents and other open source tools of HVML, please refer to the
following repositories:

- HVML Specifications: <https://github.com/HVML/hvml-docs>.
- PurC (the Prime hVml inteRpreter for C language): <https://github.com/HVML/purc>.
- PurC Fetcher (the remote data fetcher for PurC): <https://github.com/HVML/purc-fetcher>.
- PurCMC (an HVML renderer in text-mode): <https://github.com/HVML/purc-midnight-commander>.
- xGUI Pro (an advanced HVML renderer based on WebKit): <https://github.com/HVML/xgui-pro>.

## Dependencies


## Building xGUI Pro

```
rm -rf build && cmake -DCMAKE_BUILD_TYPE=Debug -DPORT=GTK -B build && cmake --build build
```

## Debugging xGUI Pro

```
$ sudo su
# echo "/tmp/core-pid_%p.dump" > /proc/sys/kernel/core_pattern
# exit
$ ulimit -c unlimited
```

For security reasons, the core dump is disabled by default on some
Linux systems. The above command lines specify the core pattern, which will
be used when dumping the core of an aborted process by the kernel.

```
$ WEBKIT_WEBEXT_DIR=/path/to/your/xgui-pro/build/lib/webext bin/xguipro hvml://localhost/_renderer/_builtin/-/assets/about.html
```

In the above command lines, the enviornment variable `WEBKIT_WEBEXT_DIR` can be
used to specify the directory in which the HVML extension module locates.

By default, if you do not define the enviornment variable, xGUI Pro will try to
find the extension module in the sub directory called `xguipro/` in the library
installation directory. Generally, it is `/usr/local/lib/xguipro` on Linux.

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

After this, run `purc` to execute an HVML program:

```
```

Or run `purcsex` in PurC Midnigth Commander in another terminal:

```
$ cd /path/to/purc-midnight-commander/build/
$ source/bin/purcsex/purcsex --sample=calculator
```

If encounter core dumps, use `gdb`:

```
$ gdb bin/xguipro -c /tmp/core-pid_xxxx.dump
```

Note that, `gdb` might take several minutes to load the symbols of xGUI Pro
and WebKit. So please wait patiently.

## Current Status

- July 2022: Version 0.6.0.

## Authors and Contributors

- Vincent Wei

## Copying

Copyright (C) 2022 [FMSoft Technologies]

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

### Commercial License

If you cannot accept GPLv3, you need to be licensed from FMSoft.

For more information about the commercial license, please refer to
<https://hybridos.fmsoft.cn/blog/hybridos-licensing-policy>.

### Special Statement

The above open source or free software license(s) does
not apply to any entity in the Exception List published by
Beijing FMSoft Technologies Co., Ltd.

If you are or the entity you represent is listed in the Exception List,
the above open source or free software license does not apply to you
or the entity you represent. Regardless of the purpose, you should not
use the software in any way whatsoever, including but not limited to
downloading, viewing, copying, distributing, compiling, and running.
If you have already downloaded it, you MUST destroy all of its copies.

The Exception List is published by FMSoft and may be updated
from time to time. For more information, please see
<https://www.fmsoft.cn/exception-list>.

## Tradmarks

1) `HVML` is a registered tradmark of [FMSoft Technologies] in China and other contries or regions.

![HVML](https://www.fmsoft.cn/application/files/8116/1931/8777/HVML256132.jpg)

2) `呼噜猫` is a registered tradmark of [FMSoft Technologies] in China and other contries or regions.

![呼噜猫](https://www.fmsoft.cn/application/files/8416/1931/8781/256132.jpg)

3) `Purring Cat` is a tradmark of [FMSoft Technologies] in China and other contries or regions.

![Purring Cat](https://www.fmsoft.cn/application/files/2816/1931/9258/PurringCat256132.jpg)

4) `PurC` is a tradmark of [FMSoft Technologies] in China and other contries or regions.

![PurC](https://www.fmsoft.cn/application/files/5716/2813/0470/PurC256132.jpg)

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
