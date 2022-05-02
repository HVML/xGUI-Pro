# xGUI Pro

xGUI Pro is a modern, cross-platform, and advanced [HVML] renderer which is
based on tailored [WebKit].

- [Introduction](#introduction)
- [Dependencies](#dependencies)
- [Building xGUI](#building-xgui)
- [Current Status](#current-status)
- [TODO List](#todo-list)
- [Copying](#copying)
   + [Commercial License](#commercial-license)
   + [Special Statement](#special-statement)

## Introduction

During the development of [HybridOS], [Vincent Wei] proposed a new-style,
general-purpose, and easy-to-learn programming language called `HVML`.

HVML provides a totally different framework other than Java, C#, or Swift.
In a complete HVML-based application framework, a standalone GUI renderer
is usually included.

xGUI Pro is an open source implementation of the standalone GUI renderer
for HVML programs. It licensed under GPLv3, and it is free for commercial
use if you follow the terms of GPLv3.

xGUI Pro is based on the mature browser engine - [WebKit]. By using xGUI Pro,
the developers can use HTML/SVG and CSS to describe the GUI contents in their
HVML programs. We reserve the JavaScript engine of WebKit, so it is possible
to use some popular front-end framwork such as Bootstrap to help xGUI Pro to
render contents in your user interfaces.

For other open source tools of HVML, please refer to the following repositories:

- HVML Specifications: <https://github.com/HVML/hvml-docs>.
- PurC (the Prime hVml inteRpreter for C language): <https://github.com/HVML/purc>.
- PurC Fetcher (the remote data fetcher for PurC): <https://github.com/HVML/purc-fetcher>.
- PurCMC (an HVML renderer in text-mode): <https://github.com/HVML/purc-midnight-commander>.
- xGUI Pro (an advanced HVML renderer based on WebKit): <https://github.com/HVML/xgui-pro>.

## Dependencies


## Building xGUI Pro


## Current Status

- May 2022: Version 0.9.

## TODO List


## Copying

Copyright (C) 2022 FMSoft <https://www.fmsoft.cn>

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
