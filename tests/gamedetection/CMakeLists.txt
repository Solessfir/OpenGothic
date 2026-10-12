cmake_minimum_required(VERSION 3.16)
project(GameDetectionTests LANGUAGES CXX)

enable_testing()
add_executable(game-detection-tests main.cpp)
target_compile_features(game-detection-tests PRIVATE cxx_std_20)
add_test(NAME game-detection COMMAND game-detection-tests)
