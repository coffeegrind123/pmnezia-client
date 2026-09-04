message("Client android ${CMAKE_ANDROID_ARCH_ABI} build")

if(NOT DEFINED APP_ANDROID_MIN_SDK)
    set(APP_ANDROID_MIN_SDK 28)
endif()

set(ANDROID_PLATFORM "android-${APP_ANDROID_MIN_SDK}" CACHE STRING
    "The minimum API level supported by the application or library" FORCE)

# set QTP0002 policy: target properties that specify Android-specific paths may contain generator expressions
qt_policy(SET QTP0002 NEW)

set_target_properties(${PROJECT} PROPERTIES
    QT_ANDROID_VERSION_NAME ${CMAKE_PROJECT_VERSION}
    QT_ANDROID_VERSION_CODE ${APP_ANDROID_VERSION_CODE}
    QT_ANDROID_MIN_SDK_VERSION ${APP_ANDROID_MIN_SDK}
    QT_ANDROID_TARGET_SDK_VERSION 36
    QT_ANDROID_SDK_BUILD_TOOLS_REVISION 36.0.0
)

set(QT_ANDROID_MULTI_ABI_FORWARD_VARS "QT_NO_GLOBAL_APK_TARGET_PART_OF_ALL;CMAKE_BUILD_TYPE")

# We need to include qtprivate api's
# As QAndroidBinder is not yet implemented with a public api
# Check if Qt6::CorePrivate is available (may not be in all Qt versions/configurations)
if(TARGET Qt6::CorePrivate)
    set(LIBS ${LIBS} Qt6::CorePrivate)
endif()
set(LIBS ${LIBS} -ljnigraphics)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/platforms/android)

set(HEADERS ${HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/platforms/android/android_controller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/platforms/android/android_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/protocols/androidVpnProtocol.h
    ${CMAKE_CURRENT_SOURCE_DIR}/core/utils/installedAppsImageProvider.h
)

set(SOURCES ${SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/platforms/android/android_controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platforms/android/android_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/protocols/androidVpnProtocol.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/utils/installedAppsImageProvider.cpp
    # JNI bridge into the native MasterDnsVPN engine. Compiled into the main
    # Qt-for-Android shared library so the Java loader resolves
    # Java_org_amnezia_vpn_protocol_masterdnsvpn_MasterDnsVpnNative_*
    # via the same .so the activity already pulls in.
    ${CMAKE_CURRENT_SOURCE_DIR}/masterdnsvpn/android_jni.cpp
    # JNI bridge into the native QQ-DNS engine (same .so). Exposes the loopback
    # UDP port the Android VpnService points AmneziaWG at.
    ${CMAKE_CURRENT_SOURCE_DIR}/qqdns/android_jni.cpp
)


find_package(awg-android REQUIRED)
set(LIBS ${LIBS} amnezia::awg-android)
set_property(TARGET ${PROJECT} APPEND PROPERTY QT_ANDROID_EXTRA_LIBS ${AMNEZIA_ANDROID_LIBWG_PATH} ${AMNEZIA_ANDROID_LIBWG_QUICK_PATH})

find_package(amnezia-libxray REQUIRED)
file(COPY ${AMNEZIA_LIBXRAY_PATH} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/android/xray/libXray)


set(APP_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android)

if(APP_ANDROID_MAX_SDK)
    set(APP_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/android-package-source)
    file(REMOVE_RECURSE ${APP_ANDROID_PACKAGE_SOURCE_DIR})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/android/ DESTINATION ${APP_ANDROID_PACKAGE_SOURCE_DIR})

    set(manifest_path ${APP_ANDROID_PACKAGE_SOURCE_DIR}/AndroidManifest.xml)
    set(manifest_anchor "android:installLocation=\"auto\">")
    file(READ ${manifest_path} manifest_contents)
    string(REPLACE
        "${manifest_anchor}"
        "${manifest_anchor}\n\n    <uses-sdk android:maxSdkVersion=\"${APP_ANDROID_MAX_SDK}\" />"
        patched_contents "${manifest_contents}")
    if(patched_contents STREQUAL manifest_contents)
        message(FATAL_ERROR
            "Failed to set maxSdkVersion=${APP_ANDROID_MAX_SDK}: anchor '${manifest_anchor}' "
            "not found in ${CMAKE_CURRENT_SOURCE_DIR}/android/AndroidManifest.xml")
    endif()
    file(WRITE ${manifest_path} "${patched_contents}")
endif()

set_property(TARGET ${PROJECT} PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR ${APP_ANDROID_PACKAGE_SOURCE_DIR})

if(QT_USE_TARGET_ANDROID_BUILD_DIR)
    set(_android_build_dir "${CMAKE_CURRENT_BINARY_DIR}/android-build-${PROJECT}")
else()
    set(_android_build_dir "${CMAKE_CURRENT_BINARY_DIR}/android-build")
endif()

add_custom_target(android_gradle_clean
    COMMAND ./gradlew clean
    WORKING_DIRECTORY "${_android_build_dir}"
    COMMENT "Cleaning Android Gradle build cache"
)

# Always-available debug target: build the debug APK and copy it to the standard output path
# so Qt Creator's deploy step picks it up automatically
add_custom_target(android_debug_install
    COMMAND ./gradlew assembleOssDebug
    COMMAND sh -c "cp build/outputs/apk/oss/debug/*.apk build/outputs/apk/android-build-${PROJECT}-debug.apk"
    WORKING_DIRECTORY "${_android_build_dir}"
    COMMENT "Building Android Debug APK and copying to deploy path"
    DEPENDS ${PROJECT}
)

