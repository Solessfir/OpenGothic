cmake_minimum_required(VERSION 3.16)
project(RenderingTests LANGUAGES CXX)
enable_testing()
add_executable(lanczos-tests lanczos.cpp)
target_compile_features(lanczos-tests PRIVATE cxx_std_20)
add_test(NAME lanczos-paired-filter COMMAND lanczos-tests)
add_executable(camera-obstruction-tests cameraobstruction.cpp)
target_compile_features(camera-obstruction-tests PRIVATE cxx_std_20)
add_test(NAME camera-obstruction-materials COMMAND camera-obstruction-tests)

set(RENDERING_TEST_TEMPEST_LIBRARY "" CACHE FILEPATH "Optional built shared Tempest import library for GPU comparisons")
if(RENDERING_TEST_TEMPEST_LIBRARY)
  find_program(GLSLANG_VALIDATOR glslangValidator HINTS "$ENV{VULKAN_SDK}/Bin" REQUIRED)
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/camera-obstruction.spv"
    COMMAND "${GLSLANG_VALIDATOR}" -V -I${CMAKE_CURRENT_SOURCE_DIR}/../../shader
            "${CMAKE_CURRENT_SOURCE_DIR}/camera_obstruction.comp" -o "${CMAKE_CURRENT_BINARY_DIR}/camera-obstruction.spv"
    DEPENDS camera_obstruction.comp ../../shader/scene.glsl ../../shader/materials/camera_obstruction.glsl
    VERBATIM)
  add_executable(camera-obstruction-gpu-tests camera_obstruction_gpu.cpp "${CMAKE_CURRENT_BINARY_DIR}/camera-obstruction.spv")
  target_compile_features(camera-obstruction-gpu-tests PRIVATE cxx_std_20)
  target_include_directories(camera-obstruction-gpu-tests PRIVATE ../../lib/Tempest/Engine/include)
  target_link_libraries(camera-obstruction-gpu-tests PRIVATE "${RENDERING_TEST_TEMPEST_LIBRARY}")
  add_test(NAME camera-obstruction-vulkan COMMAND camera-obstruction-gpu-tests)
  set_tests_properties(camera-obstruction-vulkan PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" TIMEOUT 60)
  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/hdr-output.spv"
    COMMAND "${GLSLANG_VALIDATOR}" -V -I${CMAKE_CURRENT_SOURCE_DIR}/../../shader
            "${CMAKE_CURRENT_SOURCE_DIR}/hdr_output.comp" -o "${CMAKE_CURRENT_BINARY_DIR}/hdr-output.spv"
    DEPENDS hdr_output.comp ../../shader/lighting/tonemapping.glsl ../../shader/lighting/hdr_output.glsl
    VERBATIM)
  add_executable(hdr-output-tests hdr_output_gpu.cpp "${CMAKE_CURRENT_BINARY_DIR}/hdr-output.spv")
  target_compile_features(hdr-output-tests PRIVATE cxx_std_20)
  target_include_directories(hdr-output-tests PRIVATE ../../lib/Tempest/Engine/include)
  target_link_libraries(hdr-output-tests PRIVATE "${RENDERING_TEST_TEMPEST_LIBRARY}")
  add_test(NAME hdr-vulkan-output COMMAND hdr-output-tests)
  set_tests_properties(hdr-vulkan-output PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" TIMEOUT 60)
  foreach(VARIANT reference paired)
    set(DEFINES "")
    if(VARIANT STREQUAL "reference")
      set(DEFINES -DREFERENCE)
    endif()
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${VARIANT}.spv"
      COMMAND "${GLSLANG_VALIDATOR}" -V ${DEFINES} -I${CMAKE_CURRENT_SOURCE_DIR}/../../shader
              "${CMAKE_CURRENT_SOURCE_DIR}/lanczos.comp" -o "${CMAKE_CURRENT_BINARY_DIR}/${VARIANT}.spv"
      DEPENDS lanczos.comp ../../shader/upscale/lanczos.glsl
      VERBATIM)
  endforeach()
  add_executable(lanczos-gpu-tests lanczos_gpu.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/reference.spv" "${CMAKE_CURRENT_BINARY_DIR}/paired.spv")
  target_compile_features(lanczos-gpu-tests PRIVATE cxx_std_20)
  target_include_directories(lanczos-gpu-tests PRIVATE ../../lib/Tempest/Engine/include)
  target_link_libraries(lanczos-gpu-tests PRIVATE "${RENDERING_TEST_TEMPEST_LIBRARY}")
  add_test(NAME lanczos-vulkan-filter COMMAND lanczos-gpu-tests)
  set_tests_properties(lanczos-vulkan-filter PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

  foreach(VARIANT reference optimized)
    set(DEFINES "")
    if(VARIANT STREQUAL "reference")
      set(DEFINES -DREFERENCE)
    endif()
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ssao-${VARIANT}.spv"
      COMMAND "${GLSLANG_VALIDATOR}" -V ${DEFINES} -I${CMAKE_CURRENT_SOURCE_DIR}/../../shader
              "${CMAKE_CURRENT_SOURCE_DIR}/ssao_convergence.comp" -o "${CMAKE_CURRENT_BINARY_DIR}/ssao-${VARIANT}.spv"
      DEPENDS ssao_convergence.comp ../../shader/ssao/convergence.glsl
      VERBATIM)
  endforeach()
  add_executable(ssao-convergence-tests ssao_convergence_gpu.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/ssao-reference.spv" "${CMAKE_CURRENT_BINARY_DIR}/ssao-optimized.spv")
  target_compile_features(ssao-convergence-tests PRIVATE cxx_std_20)
  target_include_directories(ssao-convergence-tests PRIVATE ../../lib/Tempest/Engine/include)
  target_link_libraries(ssao-convergence-tests PRIVATE "${RENDERING_TEST_TEMPEST_LIBRARY}")
  add_test(NAME ssao-vulkan-convergence COMMAND ssao-convergence-tests)
  set_tests_properties(ssao-vulkan-convergence PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" TIMEOUT 60)

  add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ssao-half.spv"
    COMMAND "${GLSLANG_VALIDATOR}" -V -I${CMAKE_CURRENT_SOURCE_DIR}/../../shader
            "${CMAKE_CURRENT_SOURCE_DIR}/ssao_half.comp" -o "${CMAKE_CURRENT_BINARY_DIR}/ssao-half.spv"
    DEPENDS ssao_half.comp ../../shader/ssao/half_resolution.glsl
    VERBATIM)
  add_executable(ssao-half-tests ssao_half_gpu.cpp "${CMAKE_CURRENT_BINARY_DIR}/ssao-half.spv")
  target_compile_features(ssao-half-tests PRIVATE cxx_std_20)
  target_include_directories(ssao-half-tests PRIVATE ../../lib/Tempest/Engine/include)
  target_link_libraries(ssao-half-tests PRIVATE "${RENDERING_TEST_TEMPEST_LIBRARY}")
  add_test(NAME ssao-vulkan-half-resolution COMMAND ssao-half-tests)
  set_tests_properties(ssao-vulkan-half-resolution PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" TIMEOUT 60)
endif()
