macro(make_persistent A)
foreach(argument ${A})
  string(FIND "${argument}" ":" typepos)
  string(FIND "${argument}" "=" valuepos)
  if (typepos GREATER_EQUAL 1 AND valuepos GREATER_EQUAL 1)
  math(EXPR typepos "${typepos}-2")
  string(SUBSTRING "${argument}" 2 "${typepos}" argname)
  math(EXPR typepos "${typepos}+3")
  math(EXPR valueoffset "${valuepos}-${typepos}")
  string(SUBSTRING "${argument}" "${typepos}" "${valueoffset}" argtype)
  math(EXPR valuepos "${valuepos}+1")
  string(SUBSTRING "${argument}" "${valuepos}" -1 argvalue)
  set("${argname}" "${argvalue}" CACHE "${argtype}" "")
  message("${argname} is ${argvalue} of type ${argtype}")
  endif()
endforeach()
endmacro()

macro(make_goopax_exec P)
  add_executable(${P} ${P}.cpp)
  target_link_libraries(${P} PRIVATE goopax::goopax)
  if (UNIX AND NOT APPLE AND NOT CYGWIN AND NOT ANDROID)
    target_link_libraries(${P} PRIVATE -ltbb)
  endif()
  if (IOS)
    set_apple_properties(${P})
  endif()
  install(TARGETS ${P} DESTINATION bin)
endmacro()