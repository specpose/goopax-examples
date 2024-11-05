function(set_apple_properties P)
    STRING(REGEX REPLACE "_" "-" bundle_id_name ${P})
    message("P=${P}, bundle_id_name=${bundle_id_name}")

    set_target_properties(${P} PROPERTIES
      MACOSX_BUNDLE 1
      MACOSX_BUNDLE_BUNDLE_NAME "${bundle_id_name}"
      MACOSX_BUNDLE_LONG_VERSION_STRING "3.3.0"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "3.3.0"
      MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
      MACOSX_BUNDLE_GUI_IDENTIFIER "com.goopax.${bundle_id_name}")
      
#      MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/examples/ios/Info.plist"

       if (IOS)
          set_target_properties(${P} PROPERTIES
        LINK_FLAGS "-Wl,-F${PROJECT_BINARY_DIR}/lib/$<CONFIG>${OS_SUFFIX}/"
            XCODE_ATTRIBUTE_INSTALL_PATH "${PROJECT_SOURCE_DIR}/${DEST_BIN}"
            XCODE_ATTRIBUTE_SKIP_INSTALL "No")
    install(FILES ${PROJECT_SOURCE_DIR}/examples/ios/logo_120x120.png DESTINATION "${DEST_BIN}/${P}.app")
       endif()

    # This little macro lets you set any XCode specific property
    macro (set_xcode_property2 TARGET XCODE_PROPERTY XCODE_VALUE)
        set_property (TARGET ${TARGET} PROPERTY XCODE_ATTRIBUTE_${XCODE_PROPERTY} "${XCODE_VALUE}")
    endmacro (set_xcode_property2)

    set(DEVELOPMENT_TEAM_ID "45V82BN3M6")
    set(DEVELOPMENT_CERTIFICATE_ID "94L856K4U5")
    
    SET_XCODE_PROPERTY2(${P} DEVELOPMENT_TEAM "${DEVELOPMENT_TEAM_ID}")
    if (IOS)
        SET_XCODE_PROPERTY2(${P} CODE_SIGN_IDENTITY "iPhone Developer")
    else()
        SET_XCODE_PROPERTY2(${P} CODE_SIGN_IDENTITY "Mac Developer")
    endif()
    SET_XCODE_PROPERTY2(${P} PRODUCT_BUNDLE_IDENTIFIER "com.goopax.${bundle_id_name}")
    SET_XCODE_PROPERTY2(${P} PRODUCT_NAME "${bundle_id_name}")
    SET_XCODE_PROPERTY2(${P} IPHONEOS_DEPLOYMENT_TARGET "${IOS_DEPLOYMENT_TARGET}")
    
    if (IOS)
      add_custom_command(TARGET ${P} PRE_LINK
                COMMAND mkdir -vp ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>${OS_SUFFIX}/${bundle_id_name}.app/Frameworks/
#                  COMMAND cp -avH ${PROJECT_BINARY_DIR}/lib/$<CONFIG>${OS_SUFFIX}/goopax.framework.dSYM ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>${OS_SUFFIX}/${bundle_id_name}.app/Frameworks/
                  COMMAND cp -avH ${goopax_DIR}/../../../../goopax.framework ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>${OS_SUFFIX}/${bundle_id_name}.app/Frameworks/
                  COMMAND codesign --force --verbose ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>${OS_SUFFIX}/${bundle_id_name}.app/Frameworks/goopax.framework --sign "${DEVELOPMENT_CERTIFICATE_ID}"
#                  COMMAND codesign --force --verbose ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>${OS_SUFFIX}/${bundle_id_name}.app/Frameworks/goopax.framework.dSYM --sign "${DEVELOPMENT_CERTIFICATE_ID}"
                  )
    endif()
    set_target_properties(${P} PROPERTIES XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks")
    message("P=${P}")
    add_dependencies(${P} goopax::goopax)
endfunction()

