﻿#
# MegaMol™ vtkm Plugin
# Copyright 2017, by MegaMol Team
# Alle Rechte vorbehalten. All rights reserved.
#
string(TOUPPER mmvtkm EXPORT_NAME)
option(BUILD_${EXPORT_NAME}_PLUGIN "Option to build mmvtkm" OFF)

if(BUILD_${EXPORT_NAME}_PLUGIN)
  project(mmvtkm LANGUAGES CXX CUDA)
  
  # Check option dependencies: First list item is always the current build option,
  # afterwards list the dependencies.
  set(DEP_LIST "${DEP_LIST};BUILD_${EXPORT_NAME}_PLUGIN BUILD_CORE" CACHE INTERNAL "")
  
  # Add externals.
  require_external(vtkm)

  file(GLOB_RECURSE public_header_files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "include/*.h")
  file(GLOB_RECURSE source_files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "src/*.cpp")
  file(GLOB_RECURSE header_files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "src/*.h")
  #file(GLOB_RECURSE library_files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "lib/*") # comment this line if implemented as external library
  #file(GLOB_RECURSE shader_files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "Shaders/*")

  set_source_files_properties(source_files PROPERTIES LANGUAGE CUDA)

  # external_get_property(vtkm INSTALL_MAIN_DIR)
  external_get_property(vtkm SOURCE_DIR)
  # message(${SOURCE_DIR})
  string(REPLACE "-src" "-build" BUILD_DIR "${SOURCE_DIR}")

  # Target definition
  #add_library(${PROJECT_NAME} STATIC ${public_header_files} ${header_files} ${shader_files} ${source_files})
  add_library(${PROJECT_NAME} STATIC ${public_header_files} ${header_files} ${source_files})
  target_compile_definitions(${PROJECT_NAME} PRIVATE ${EXPORT_NAME}_EXPORTS)
  target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    "include" "src" "lib"
    "${SOURCE_DIR}"
    "${BUILD_DIR}/Release/include"
    "${SOURCE_DIR}/vtkm/thirdparty/taotuple"
    "${SOURCE_DIR}/vtkm/thirdparty/diy"
    )
  target_link_libraries(${PROJECT_NAME} PRIVATE core vtkm_cont vtkm_renderer vtkm_worklet)

  # Installation rules for generated files
  #install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION "include")
  #install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/ DESTINATION "share/shaders")

  if(WIN32)
    #install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "bin")
    #install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION "lib")
  else()
    #install(TARGETS ${PROJECT_NAME} DESTINATION "lib" EXPORT ${PROJECT_NAME}-target)
  endif()

  # Grouping in Visual Studio
  set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER plugins)
  source_group("Public Header Files" FILES ${public_header_files})
  source_group("Header Files" FILES ${header_files})
  source_group("Source Files" FILES ${source_files})
  #source_group("Shaders" FILES ${shader_files})

  # Register plugin
  megamol_register_plugin(${PROJECT_NAME})
endif()
