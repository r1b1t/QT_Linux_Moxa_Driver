cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" MOXA_VERSION_FILE_LINES)
foreach(MOXA_VERSION_FILE_LINE IN LISTS MOXA_VERSION_FILE_LINES)
    if(MOXA_VERSION_FILE_LINE MATCHES "^APP_NAME=(.*)$")
        set(MOXA_APP_NAME_STRING "${CMAKE_MATCH_1}")
    elseif(MOXA_VERSION_FILE_LINE MATCHES "^APP_VERSION=(.*)$")
        set(MOXA_APP_VERSION_STRING "${CMAKE_MATCH_1}")
    endif()
endforeach()
if(NOT DEFINED MOXA_APP_NAME_STRING OR NOT DEFINED MOXA_APP_VERSION_STRING)
    message(FATAL_ERROR "version.txt must define APP_NAME and APP_VERSION")
endif()

string(TOLOWER "${MOXA_APP_NAME_STRING}" MOXA_APP_ID)
string(REGEX REPLACE "[^a-z0-9]" "" MOXA_APP_ID "${MOXA_APP_ID}")
if(MOXA_APP_ID STREQUAL "")
    message(FATAL_ERROR "APP_NAME in version.txt must contain at least one letter or digit")
endif()

project(QT_Linux_Moxa_Driver VERSION ${MOXA_APP_VERSION_STRING} LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
    app/main.cpp
    app/mainwindow.h
    app/mainwindow.cpp
    app/mainwindow.ui
    core/systeminfo.h
    core/systeminfo.cpp
    core/processrunner.h
    core/processrunner.cpp
    core/moxadrivermanager.h
    core/moxadrivermanager.cpp
    core/moxaportmanager.h
    core/moxaportmanager.cpp
)

set(MOXA_TARGET_NAME "MoxaUportManager")

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${MOXA_TARGET_NAME}
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(${MOXA_TARGET_NAME}
        ${PROJECT_SOURCES}
    )
endif()

target_include_directories(${MOXA_TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

target_link_libraries(${MOXA_TARGET_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

target_compile_definitions(${MOXA_TARGET_NAME} PRIVATE
    MOXA_APP_ID="${MOXA_APP_ID}"
    MOXA_APP_NAME="${MOXA_APP_NAME_STRING}"
    MOXA_APP_VERSION="${MOXA_APP_VERSION_STRING}"
    MOXA_DEV_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
)

include(GNUInstallDirs)
install(TARGETS ${MOXA_TARGET_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${MOXA_TARGET_NAME})
endif()

if(UNIX AND NOT APPLE)
    set(MOXA_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package" CACHE PATH
        "Directory used by the Linux deployment target")

    add_custom_target(deploy_linux
        COMMAND ${CMAKE_COMMAND}
            -DAPP_EXECUTABLE=$<TARGET_FILE:${MOXA_TARGET_NAME}>
            -DSTAGING_DIR=${MOXA_PACKAGE_DIR}/${MOXA_APP_ID}
            -DAPP_ID=${MOXA_APP_ID}
            -DPROJECT_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P ${CMAKE_SOURCE_DIR}/packaging/linux/DeployLinux.cmake
        DEPENDS ${MOXA_TARGET_NAME}
        COMMENT "Staging a /opt/${MOXA_APP_ID} application directory (binary + driver + helper script)"
        VERBATIM
    )
endif()
