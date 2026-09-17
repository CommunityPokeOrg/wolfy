cmake_minimum_required(VERSION 3.20)
project(wolfy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick Test DBus)

if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
else()
    # Qt < 6.3 fallback (e.g. Ubuntu 22.04's Qt 6.2).
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

add_subdirectory(core)

enable_testing()
add_subdirectory(tests)
