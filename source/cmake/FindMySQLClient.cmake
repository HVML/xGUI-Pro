# - Try to find MySQLClient
# 
# Copyright (C) 2020 Beijing FMSoft Technologies Co., Ltd.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
# 
# Or,
# 
# As this component is a program released under LGPLv3, which claims
# explicitly that the program could be modified by any end user
# even if the program is conveyed in non-source form on the system it runs.
# Generally, if you distribute this program in embedded devices,
# you might not satisfy this condition. Under this situation or you can
# not accept any condition of LGPLv3, you need to get a commercial license
# from FMSoft, along with a patent license for the patents owned by FMSoft.
# 
# If you have got a commercial/patent license of this program, please use it
# under the terms and conditions of the commercial license.
# 
# For more information about the commercial license and patent license,
# please refer to
# <https://hybridos.fmsoft.cn/blog/hybridos-licensing-policy/>.
# 
# Also note that the LGPLv3 license does not apply to any entity in the
# Exception List published by Beijing FMSoft Technologies Co., Ltd.
# 
# If you are or the entity you represent is listed in the Exception List,
# the above open source or free software license does not apply to you
# or the entity you represent. Regardless of the purpose, you should not
# use the software in any way whatsoever, including but not limited to
# downloading, viewing, copying, distributing, compiling, and running.
# If you have already downloaded it, you MUST destroy all of its copies.
# 
# The Exception List is published by FMSoft and may be updated
# from time to time. For more information, please see
# <https://www.fmsoft.cn/exception-list>.

# use pkg-config to get the directories and then use these values
# in the find_path() and find_library() calls
if (NOT WIN32)
    find_package(PkgConfig)

    pkg_check_modules(PC_MYSQLCLIENT mysqlclient)

    set(MYSQLCLIENT_DEFINITIONS ${PC_MYSQLCLIENT_CFLAGS_OTHER})
    set(MYSQLCLIENT_VERSION "${PC_MYSQLCLIENT_VERSION}")
endif (NOT WIN32)

find_path(MYSQLCLIENT_INCLUDE_DIR NAMES mysql.h
    PATHS
    ${PC_MYSQLCLIENT_INCLUDEDIR}
    ${PC_MYSQLCLIENT_INCLUDE_DIRS}
)

find_library(MYSQLCLIENT_LIBRARIES NAMES mysqlclient
    PATHS
    ${PC_MYSQLCLIENT_LIBDIR}
    ${PC_MYSQLCLIENT_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MySQLClient
        REQUIRED_VARS MYSQLCLIENT_INCLUDE_DIR MYSQLCLIENT_LIBRARIES
        VERSION_VAR   MYSQLCLIENT_VERSION)

# show the MYSQLCLIENT_INCLUDE_DIR and MYSQLCLIENT_LIBRARIES variables only in the advanced view
mark_as_advanced(
    MYSQLCLIENT_INCLUDE_DIR
    MYSQLCLIENT_LIBRARIES
)
