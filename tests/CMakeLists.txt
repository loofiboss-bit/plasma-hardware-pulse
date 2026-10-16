qt_add_executable(test_pulsecore
    test_pulsecore.cpp
)

target_link_libraries(test_pulsecore
    PRIVATE
    loofipulsecore
    Qt6::Core
    Qt6::Test
)

add_test(NAME pulsecore_unit_tests COMMAND test_pulsecore)

add_test(
    NAME app_qml_smoke_test
    COMMAND "${CMAKE_BINARY_DIR}/bin/loofi-pulse" --smoke-test
)
set_tests_properties(app_qml_smoke_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${CMAKE_BINARY_DIR}/bin"
    TIMEOUT 30
)
