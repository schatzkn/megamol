file(GLOB plugins RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/" *)
foreach(plugin ${plugins})
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${plugin}/CMakeLists.txt" AND NOT ${plugin} STREQUAL "template")
    add_subdirectory(${plugin})
  endif()
endforeach()
