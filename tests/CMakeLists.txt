qt_add_executable(test_scriptengine
    test_scriptengine.cpp
)

target_link_libraries(test_scriptengine PRIVATE
    wolfycore
    Qt6::Core
    Qt6::Qml
    Qt6::Test
)

target_include_directories(test_scriptengine PRIVATE ${CMAKE_SOURCE_DIR}/core)

add_test(NAME scriptengine COMMAND test_scriptengine)

qt_add_executable(test_winsync
    test_winsync.cpp
)

target_link_libraries(test_winsync PRIVATE
    wolfycore
    Qt6::Core
    Qt6::Qml
    Qt6::Test
)

target_include_directories(test_winsync PRIVATE ${CMAKE_SOURCE_DIR}/core)

add_test(NAME winsync COMMAND test_winsync)
