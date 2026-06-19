set(CLIENT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/..)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/Modules;${CMAKE_MODULE_PATH}")

add_subdirectory(${CLIENT_ROOT_DIR}/3rd/SortFilterProxyModel)
set(LIBS ${LIBS} SortFilterProxyModel)
include(${CLIENT_ROOT_DIR}/cmake/QSimpleCrypto.cmake)

include(${CLIENT_ROOT_DIR}/3rd/qrcodegen/qrcodegen.cmake)

add_compile_definitions(_WINSOCKAPI_)

# Stop <windows.h> from defining the min()/max() macros, which mangle
# std::min/std::max call sites (e.g. client/masterdnsvpn/*) into syntax
# errors under MSVC. Defined on the command line, so it lands before any
# windows.h include across every client target.
if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_WITH_QT6 ON)
add_subdirectory(${CLIENT_ROOT_DIR}/3rd/qtkeychain EXCLUDE_FROM_ALL)

if(ANDROID)
    # Use qtgamepad from amnezia-vpn/qtgamepad repository
    # Only if Qt6CorePrivate is available (required by qtgamepad)
    find_package(Qt6CorePrivate CONFIG QUIET)
    if(Qt6CorePrivate_FOUND)
        add_subdirectory(${CLIENT_ROOT_DIR}/3rd/qtgamepad)
        # Link both the C++ module and QML plugin
        if(TARGET GamepadLegacy)
            target_link_libraries(${PROJECT} PRIVATE GamepadLegacy)
        endif()
        if(TARGET GamepadLegacyQuickPrivate)
            target_link_libraries(${PROJECT} PRIVATE GamepadLegacyQuickPrivate)
        endif()
        message(STATUS "Gamepad support enabled for Android")
    else()
        message(STATUS "Qt6CorePrivate not found. Gamepad support disabled for Android.")
    endif()
endif()

set(LIBS ${LIBS} qt6keychain)

include_directories(
    ${CLIENT_ROOT_DIR}/3rd/QSimpleCrypto/src/include
    ${CLIENT_ROOT_DIR}/3rd/qtkeychain/qtkeychain
    ${CMAKE_CURRENT_BINARY_DIR}/3rd/qtkeychain
)

find_package(OpenSSL REQUIRED)
list(APPEND LIBS OpenSSL::SSL OpenSSL::Crypto)

find_package(libssh REQUIRED)
list(APPEND LIBS ssh::ssh)
