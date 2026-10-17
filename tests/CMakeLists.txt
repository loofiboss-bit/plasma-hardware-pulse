qt_add_executable(test_pulsecore
    test_pulsecore.cpp
)

target_link_libraries(test_pulsecore
    PRIVATE
    loofipulsecore
    Qt6::Core
    Qt6::Test
)

option(BUILD_LIVE_HARDWARE_TESTS "Run tests that require live host telemetry" OFF)

# Run deterministic tests individually so live host telemetry remains opt-in.
set(PULSECORE_PORTABLE_TESTS
    testHwmonDiscoveryAndVelocity
    testPsiAndZramCompression
    testCausalDiagnosisEngineVerdicts
    testBatteryEtaAndHealthCalculation
    testBatteryOnlyDischargingAndZeroPower
    testReadableEmptyZramIsAvailableWithoutRatio
    testThrottleSensorDisappearanceDoesNotCreateEvent
    testFlightRecorderSpikeLookup
    testMissingPsiIsUnknownAndRecovers
    testMalformedPsiIsUnavailable
    testCpuCountersResetAfterMissingDataAndRollback
    testGpuMetricsStayWithTheirDeviceAndUnknownBusy
    testHotStorageTemperatureDoesNotClaimIoBottleneck
    testThermalRiskIsNotReportedAsActiveThrottling
    testRaplCounterWrapUsesHardwareRange
    testRaplUnavailableReadingResetsInterval
    testFlightRecorderUsesRetentionWindowAndStableIds
    testFlightRecorderFiveMinuteWindowAcrossIntervals
    testHistoryAndReportsSkipUnavailableValues
    testJsonReportHasVersionQualityAndRecordedChannels
    testVerdictStabilityAndImmediateUnknown
    testCpuHwmonPowerAndDiskStatsThroughput
)
foreach(test_name IN LISTS PULSECORE_PORTABLE_TESTS)
    add_test(NAME pulsecore_${test_name} COMMAND test_pulsecore ${test_name})
endforeach()

if(BUILD_LIVE_HARDWARE_TESTS)
    add_test(NAME pulsecore_live_host_telemetry COMMAND test_pulsecore testLiveHostTelemetrySanity)
endif()

add_test(
    NAME app_qml_smoke_test
    COMMAND "${CMAKE_BINARY_DIR}/bin/loofi-pulse" --smoke-test
)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test-state/config" "${CMAKE_BINARY_DIR}/test-state/data" "${CMAKE_BINARY_DIR}/test-state/cache")
set(PULSE_TEST_XDG_ENV
    "XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/test-state/config"
    "XDG_DATA_HOME=${CMAKE_BINARY_DIR}/test-state/data"
    "XDG_CACHE_HOME=${CMAKE_BINARY_DIR}/test-state/cache"
)
set_tests_properties(app_qml_smoke_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${CMAKE_BINARY_DIR}/bin;${PULSE_TEST_XDG_ENV}"
    TIMEOUT 30
)

qt_add_executable(test_controller_actions
    test_controller_actions.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/pulsetelemetrycontroller.cpp
)
target_include_directories(test_controller_actions PRIVATE ${PROJECT_SOURCE_DIR}/src/plugin)
target_compile_definitions(test_controller_actions PRIVATE LOOFI_PULSE_VERSION="${PROJECT_VERSION}")
target_link_libraries(test_controller_actions PRIVATE
    loofipulsecore Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Widgets Qt6::Test
    KF6::Notifications KF6::I18n
)
foreach(test_name IN ITEMS testMissingSystemMonitorReportsFailure testUnsupportedProfileNeverStartsChange testUnwritableReportLocationReportsFailure)
    add_test(NAME controller_${test_name} COMMAND test_controller_actions ${test_name})
    set_tests_properties(controller_${test_name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;${PULSE_TEST_XDG_ENV}")
endforeach()
find_program(DBUS_RUN_SESSION dbus-run-session)
if(DBUS_RUN_SESSION)
    add_test(NAME controller_async_profile_failure
        COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:test_controller_actions> testAsyncProfileSetFailure)
    set_tests_properties(controller_async_profile_failure PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;${PULSE_TEST_XDG_ENV}" TIMEOUT 15)
endif()

qt_add_executable(test_qml_ui test_qml_ui.cpp)
target_compile_definitions(test_qml_ui PRIVATE
    PULSE_BUILD_BIN="${CMAKE_BINARY_DIR}/bin"
    PULSE_APP_QML="${PROJECT_SOURCE_DIR}/src/app/qml"
    PULSE_WIDGET_QML="${PROJECT_SOURCE_DIR}/package/contents/ui"
)
target_link_libraries(test_qml_ui PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
add_dependencies(test_qml_ui hardwarepulseplugin)
foreach(test_name IN ITEMS testAppNavigationKeyboardAndResponsiveRendering testWidgetComponentsLoad)
    add_test(NAME qml_${test_name} COMMAND test_qml_ui ${test_name})
    set_tests_properties(qml_${test_name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${CMAKE_BINARY_DIR}/bin;PULSE_TEST_ARTIFACT_DIR=${CMAKE_BINARY_DIR}/test-artifacts;${PULSE_TEST_XDG_ENV}"
        TIMEOUT 30
    )
endforeach()
