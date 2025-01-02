function(set_apple_properties P)
  STRING(REGEX REPLACE "_" "-" bundle_id_name ${P})

  set_target_properties(${P} PROPERTIES
    MACOSX_BUNDLE 1
    MACOSX_BUNDLE_BUNDLE_NAME "${bundle_id_name}"
    MACOSX_BUNDLE_LONG_VERSION_STRING "1.0.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
    MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.goopax.${bundle_id_name}")
  
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

  SET_XCODE_PROPERTY2(${P} DEVELOPMENT_TEAM "$ENV{APPLE_DEVELOPER_TEAM}")

  if (IOS)
    SET_XCODE_PROPERTY2(${P} CODE_SIGN_IDENTITY "iPhone Developer")
  else()
    SET_XCODE_PROPERTY2(${P} CODE_SIGN_IDENTITY "Mac Developer")
  endif()
  SET_XCODE_PROPERTY2(${P} PRODUCT_BUNDLE_IDENTIFIER "com.goopax.${bundle_id_name}")
  SET_XCODE_PROPERTY2(${P} PRODUCT_NAME "${bundle_id_name}")
  set_property(TARGET ${P} PROPERTY XCODE_EMBED_FRAMEWORKS "${goopax_DIR}/../../../../goopax.framework")

  set_target_properties(${P} PROPERTIES XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks")
  add_dependencies(${P} goopax::goopax)
endfunction()

