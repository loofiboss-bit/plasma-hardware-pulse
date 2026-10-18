cmake_minimum_required(VERSION 3.24)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PROJECT_VERSION_VALUE
     LIMIT_COUNT 1 REGEX "^[0-9]+\\.[0-9]+\\.[0-9]+$")
if(NOT PROJECT_VERSION_VALUE)
    message(FATAL_ERROR "VERSION must contain one MAJOR.MINOR.PATCH value")
endif()

project(plasma-hardware-pulse VERSION "${PROJECT_VERSION_VALUE}" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

set(QT_MAJOR_VERSION 6)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 DBus Widgets Sql)
qt_policy(SET QTP0001 NEW)

find_package(Plasma REQUIRED)
find_package(KF6Kirigami REQUIRED)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6Notifications REQUIRED)
find_package(KF6I18n REQUIRED)
find_package(KF6Crash REQUIRED)
find_package(KF6WindowSystem REQUIRED)

configure_file(packaging/plasma_applet_com.github.loofi.hardwarepulse.notifyrc
    ${CMAKE_BINARY_DIR}/share/knotifications6/plasma_applet_com.github.loofi.hardwarepulse.notifyrc COPYONLY)

include(CTest)

add_subdirectory(src/core)
add_subdirectory(src/plugin)
add_subdirectory(src/app)
add_subdirectory(src/service)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

# Install the Plasma 6 Applet package
plasma_install_package(package com.github.loofi.hardwarepulse)

# Install KNotification event definitions
install(FILES packaging/plasma_applet_com.github.loofi.hardwarepulse.notifyrc
        DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})

# Install application icon
install(FILES assets/logo.svg
        RENAME com.github.loofi.hardwarepulse.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)

# Install Desktop Entry and AppStream metainfo
install(FILES packaging/com.github.loofi.hardwarepulse.desktop
        DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES packaging/com.github.loofi.hardwarepulse.metainfo.xml
        DESTINATION ${KDE_INSTALL_METAINFODIR})
