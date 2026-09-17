qt_add_library(wolfycore SHARED
    scriptengine.cpp
    scriptengine.h
    scriptcontext.cpp
    scriptcontext.h
    windowsync.cpp
    windowsync.h
)

qt_add_qml_module(wolfycore
    URI Wolfy.Core
    VERSION 1.0
    PLUGIN_TARGET wolfycore
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Wolfy/Core
)

target_compile_definitions(wolfycore PRIVATE
    WOLFY_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(wolfycore PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::DBus
)

qt_add_executable(wolfy-sync-relay ../tools/sync-relay.cpp)
target_link_libraries(wolfy-sync-relay PRIVATE Qt6::Core Qt6::DBus)
