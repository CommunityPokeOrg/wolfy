qt_add_library(wolfycore SHARED
    scriptengine.cpp
    scriptengine.h
    scriptcontext.cpp
    scriptcontext.h
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
)
