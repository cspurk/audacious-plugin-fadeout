## build file for the Audacious FadeOut plugin
## © 2009–2018 by Christian Spurk

## only tested with this version – will probably work with older versions, too
cmake_minimum_required(VERSION 3.3.2)

include(FindPkgConfig)
include(FindGettext)

## include non-standard cmake modules
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
set(GettextTranslate_ALL "TRUE")
include(GettextTranslate)
include(GNUInstallDirs)
include(GenerateExportHeader)


###############################
## MAIN CONFIGURATION VARIABLES
###############################

## set the package name and version
set(_pkg_name "audacious-plugin-fadeout")
set(_pkg_version "3.9.0-1.0")

## find the directories to which to install the plugin and its translations
string(REGEX REPLACE "/$" "" _home_dir "$ENV{HOME}")
string(REGEX REPLACE "/$" "" _install_base "${CMAKE_INSTALL_PREFIX}")
IF(${_home_dir} STREQUAL ${_install_base})
  set(_install_dir "${_install_base}/.local/share/audacious/Plugins")
  set(LOCALEDIR "${_install_dir}/locale")
ELSE(${_home_dir} STREQUAL ${_install_base})
  set(_install_dir "${_install_base}/${CMAKE_INSTALL_LIBDIR}/audacious/Effect")
  set(LOCALEDIR "${_install_base}/${CMAKE_INSTALL_LOCALEDIR}")
ENDIF(${_home_dir} STREQUAL ${_install_base})

## translation template file
set(_pot_file ${_pkg_name}.pot)


#####################
## MAIN BUILD TARGETS
#####################

## create targets for compiling and linking the plugin
PKG_SEARCH_MODULE(AUDACIOUS REQUIRED audacious>=3.9)
PKG_SEARCH_MODULE(GLIB REQUIRED glib-2.0>=2.32)
add_definitions(${AUDACIOUS_CFLAGS} ${GLIB_CFLAGS})
add_definitions("'-DPACKAGE=\"${_pkg_name}\"'")
add_library("${_pkg_name}" SHARED "${_pkg_name}.cc")
set_target_properties("${_pkg_name}" PROPERTIES PREFIX "")

## only export symbols which are marked accordingly
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
GENERATE_EXPORT_HEADER("${_pkg_name}" BASE_NAME plugin EXPORT_MACRO_NAME EXPORT)

## create targets for i18n
add_subdirectory(po)

## create targets for the installation of the plugin
install(TARGETS "${_pkg_name}" LIBRARY DESTINATION ${_install_dir})

