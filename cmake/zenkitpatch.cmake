# The changes of OpenGothic in ZenKit (an upstream submodule) are kept as a patch: patches/zenkit-opengothic.patch.
# A clean clone gets them here, before ZenKit is built. A tree that has them already (the patch can be reversed) is left alone.
find_package(Git QUIET)
set(ZENKIT_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/lib/ZenKit")
set(ZENKIT_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/patches/zenkit-opengothic.patch")

if(GIT_FOUND AND EXISTS "${ZENKIT_PATCH}" AND EXISTS "${ZENKIT_DIR}/CMakeLists.txt")
  # The submodule is a repository of its own (.git is a file or a folder): the paths of the patch start there.
  # Without it (a copy of the sources) git would take the repository around it, then the directory is given.
  if(EXISTS "${ZENKIT_DIR}/.git")
    set(ZENKIT_APPLY_DIR  "${ZENKIT_DIR}")
    set(ZENKIT_APPLY_ARGS apply --ignore-whitespace)
  else()
    set(ZENKIT_APPLY_DIR  "${CMAKE_CURRENT_SOURCE_DIR}")
    set(ZENKIT_APPLY_ARGS apply --ignore-whitespace --directory=lib/ZenKit)
  endif()

  execute_process(
    COMMAND ${GIT_EXECUTABLE} ${ZENKIT_APPLY_ARGS} --check --reverse "${ZENKIT_PATCH}"
    WORKING_DIRECTORY "${ZENKIT_APPLY_DIR}"
    RESULT_VARIABLE ZENKIT_PATCHED
    OUTPUT_QUIET ERROR_QUIET)
  if(NOT ZENKIT_PATCHED EQUAL 0)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} ${ZENKIT_APPLY_ARGS} "${ZENKIT_PATCH}"
      WORKING_DIRECTORY "${ZENKIT_APPLY_DIR}"
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
