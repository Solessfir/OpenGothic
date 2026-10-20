# The changes of OpenGothic in ZenKit (an upstream submodule) are kept as a patch: patches/zenkit-opengothic.patch.
# A clean clone gets them here, before ZenKit is built. A tree that has them already (the patch can be reversed) is left alone.
find_package(Git QUIET)
set(ZENKIT_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/lib/ZenKit")
set(ZENKIT_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/patches/zenkit-opengothic.patch")

if(GIT_FOUND AND EXISTS "${ZENKIT_PATCH}" AND EXISTS "${ZENKIT_DIR}/CMakeLists.txt")
  execute_process(
    COMMAND ${GIT_EXECUTABLE} apply --check --reverse --ignore-whitespace "${ZENKIT_PATCH}"
    WORKING_DIRECTORY "${ZENKIT_DIR}"
    RESULT_VARIABLE ZENKIT_PATCHED
    OUTPUT_QUIET ERROR_QUIET)
  if(NOT ZENKIT_PATCHED EQUAL 0)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} apply --ignore-whitespace "${ZENKIT_PATCH}"
      WORKING_DIRECTORY "${ZENKIT_DIR}"
      RESULT_VARIABLE ZENKIT_APPLIED
      ERROR_VARIABLE  ZENKIT_APPLY_ERROR)
    if(ZENKIT_APPLIED EQUAL 0)
      message(STATUS "ZenKit: patches/zenkit-opengothic.patch applied")
    else()
      message(FATAL_ERROR "ZenKit: patches/zenkit-opengothic.patch does not apply to lib/ZenKit:\n${ZENKIT_APPLY_ERROR}\n"
                          "Use the commit of ZenKit that the submodule points to, see patches/README.md")
    endif()
  endif()
endif()
