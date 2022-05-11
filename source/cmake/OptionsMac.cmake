# FIXME: These should line up with versions in Version.xcconfig
set(XGUIPRO_MAC_VERSION 0.0.1)
set(MACOSX_FRAMEWORK_BUNDLE_VERSION 0.0.1)

find_package(GLIB 2.44.0 REQUIRED COMPONENTS gio gio-unix gmodule)
find_package(Ncurses 5.0 REQUIRED)
find_package(PurC 0.0.1 REQUIRED)
find_package(LibXml2 2.8.0)

XGUIPRO_OPTION_BEGIN()
# Private options shared with other xGUIPro ports. Add options here only if
# we need a value different from the default defined in GlobalFeatures.cmake.

XGUIPRO_OPTION_DEFAULT_PORT_VALUE(ENABLE_XML PUBLIC OFF)

XGUIPRO_OPTION_END()

set(xGUIPRo_PKGCONFIG_FILE ${CMAKE_BINARY_DIR}/source/xguipro.pc)

set(xGUIProTestSupport_LIBRARY_TYPE STATIC)

