# JUCE 8.0.15 juceaide reads narrow argv as UTF-8, but Windows supplies it in
# the system ANSI code page. Absolute Vietnamese workspace paths consequently
# fail with "Unhandled exception". Keep resource arguments relative to the
# build directory; do not change the user's directory or patch the JUCE SDK.
# This overrides only the JUCE 8 Windows resource hook, preserving RC options.
if(WIN32)
  function(_juce_add_resources_rc source_target dest_target)
    if(NOT TARGET ${source_target}_rc_lib)
      get_target_property(code_dir ${source_target} JUCE_GENERATED_SOURCES_DIRECTORY)
      get_target_property(info_file ${source_target} JUCE_INFO_FILE)
      get_target_property(icon_file ${source_target} JUCE_ICON_FILE)
      set(resource_file "${code_dir}/${source_target}_resources.rc")
      file(RELATIVE_PATH relative_info "${CMAKE_BINARY_DIR}" "${info_file}")
      file(RELATIVE_PATH relative_resource "${CMAKE_BINARY_DIR}" "${resource_file}")
      set(resource_dependencies "${info_file}")
      if(icon_file)
        list(APPEND resource_dependencies "${icon_file}")
      endif()
      add_custom_command(OUTPUT "${resource_file}"
        COMMAND juce::juceaide rcfile "${relative_info}" "${relative_resource}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        DEPENDS ${resource_dependencies} VERBATIM)
      add_library(${source_target}_rc_lib OBJECT "${resource_file}")
      set(defs $<TARGET_GENEX_EVAL:${source_target},$<TARGET_PROPERTY:${source_target},COMPILE_DEFINITIONS>>)
      set(dirs $<TARGET_GENEX_EVAL:${source_target},$<TARGET_PROPERTY:${source_target},INCLUDE_DIRECTORIES>>)
      set(custom_rc $<BOOL:$<FILTER:${defs},INCLUDE,JUCE_USER_DEFINED_RC_FILE=>>)
      target_include_directories(${source_target}_rc_lib PRIVATE $<${custom_rc}:${dirs}>)
      set_source_files_properties("${resource_file}" PROPERTIES COMPILE_DEFINITIONS $<${custom_rc}:${defs}>)
    endif()
    target_link_libraries(${dest_target} PRIVATE ${source_target}_rc_lib)
  endfunction()
endif()
