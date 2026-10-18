# Link and embed TrustTunnelClient's VpnClientFramework into a network
# extension target (iOS and macOS NE). The framework is dynamic, so linking
# alone is not enough: it has to be copied into the extension's Frameworks
# directory and signed with it, and the extension must search there at run
# time. See recipes/trusttunnel-apple.
function(amnezia_add_trusttunnel_apple target)
    find_package(trusttunnel-apple REQUIRED)
    if(NOT TRUSTTUNNEL_APPLE_XCFRAMEWORK)
        message(FATAL_ERROR "trusttunnel-apple did not export TRUSTTUNNEL_APPLE_XCFRAMEWORK")
    endif()

    # Link the xcframework by path: the generated imported target carries
    # no per-config IMPORTED_LOCATION (conan-io/conan#20034, the same issue
    # the hev-socks5-tunnel block works around), and CMake >= 3.28 resolves
    # an .xcframework path to the right slice itself.
    target_link_libraries(${target} PRIVATE "${TRUSTTUNNEL_APPLE_XCFRAMEWORK}")

    set_target_properties(${target} PROPERTIES
        XCODE_EMBED_FRAMEWORKS "${TRUSTTUNNEL_APPLE_XCFRAMEWORK}"
        XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY YES
        XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY YES
    )

    # iOS appex: <appex>/Frameworks; macOS appex: Contents/Frameworks.
    if(IOS)
        set(_tt_rpath "@executable_path/Frameworks")
    else()
        set(_tt_rpath "@executable_path/../Frameworks")
    endif()
    get_target_property(_existing ${target} XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS)
    if(_existing)
        set(_tt_rpath "${_existing} ${_tt_rpath}")
    endif()
    set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "${_tt_rpath}")

    target_sources(${target} PRIVATE
        ${CLIENT_ROOT_DIR}/platforms/ios/PacketTunnelProvider+TrustTunnel.swift
        ${CLIENT_ROOT_DIR}/platforms/ios/TrustTunnelConfig.swift
        ${CLIENT_ROOT_DIR}/platforms/ios/TrustTunnelClientFactory.h
        ${CLIENT_ROOT_DIR}/platforms/ios/TrustTunnelClientFactory.m
    )
endfunction()
