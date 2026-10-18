find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
add_test(NAME service_install_paths COMMAND ${CMAKE_COMMAND}
    "-DSERVICE_SOURCE=${PROJECT_SOURCE_DIR}/src/service"
    "-DTEST_ROOT=${CMAKE_BINARY_DIR}/test-state/service-install"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_service_install.cmake")
function(pulse_add_dbus_test name target timeout)
    add_test(NAME ${name} COMMAND ${CMAKE_COMMAND}
        "-DDBUS_RUN_SESSION=${DBUS_RUN_SESSION}" "-DTEST_EXECUTABLE=$<TARGET_FILE:${target}>"
        "-DTEST_ARGUMENTS=${ARGN}" "-DTEST_LOG=${CMAKE_BINARY_DIR}/test-logs/${name}.log"
        "-DTEST_TIMEOUT=${timeout}" -P "${CMAKE_CURRENT_SOURCE_DIR}/run_dbus_test.cmake")
endfunction()
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
    testIncidentEpisodesRespectRecordingGaps
    testHighActivityWithoutRiskIsNeutral
    testRetentionMatrixAndCapacity
    testDeviceBaselinesHotplugResetAndVirtualSelection
    testProcessIdentityAndObservedTimestamp
    testRuleIdentityStabilizationAndEvidence
    testExportV2PowerDomainsAndProcessRedaction
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
    testNetworkThroughputAndProcessRadarDiskWait
    testMultiDomainContributingSignalsAndStabilizerOscillation
)
foreach(test_name IN LISTS PULSECORE_PORTABLE_TESTS)
    add_test(NAME pulsecore_${test_name} COMMAND test_pulsecore ${test_name})
endforeach()

if(BUILD_LIVE_HARDWARE_TESTS)
    add_test(NAME pulsecore_live_host_telemetry COMMAND test_pulsecore testLiveHostTelemetrySanity)
endif()

pulse_add_dbus_test(app_qml_smoke_test loofi-pulse 25 --smoke-test)

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
    ${PROJECT_SOURCE_DIR}/src/plugin/collectorclient.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/telemetryselection.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/reportexport.cpp
)
target_include_directories(test_controller_actions PRIVATE ${PROJECT_SOURCE_DIR}/src/plugin)
target_compile_definitions(test_controller_actions PRIVATE LOOFI_PULSE_VERSION="${PROJECT_VERSION}")
target_link_libraries(test_controller_actions PRIVATE
    loofipulsecore Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Widgets Qt6::Test
    KF6::Notifications KF6::I18n
)
foreach(test_name IN ITEMS testNavigationValidationAndPrivacyDefaults testNetworkSelectionAndHistoryMetadata testMissingSystemMonitorReportsFailure testUnsupportedProfileNeverStartsChange testUnwritableReportLocationReportsFailure testHistoryRetentionAndClearHistory testCollectorConnectedAndReconnect testSystemResumeResetsBaselinesAndSetsGap testTemperatureUnitPreference testCollectorPreferencesUpdate testTrendsPropertiesAndRanges testProcessTerminationSafetyAndExecution testProcessIoStats)
    add_test(NAME controller_${test_name} COMMAND test_controller_actions ${test_name})
    set_tests_properties(controller_${test_name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;${PULSE_TEST_XDG_ENV}")
endforeach()
find_program(DBUS_RUN_SESSION dbus-run-session)
if(DBUS_RUN_SESSION)
    add_test(NAME controller_async_profile_failure
        COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:test_controller_actions> testAsyncProfileSetFailure)
    set_tests_properties(controller_async_profile_failure PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;${PULSE_TEST_XDG_ENV}" TIMEOUT 15)
endif()

qt_add_executable(test_single_instance
    test_single_instance.cpp
    ${PROJECT_SOURCE_DIR}/src/app/appinstance.cpp
)
target_include_directories(test_single_instance PRIVATE ${PROJECT_SOURCE_DIR}/src/app)
target_link_libraries(test_single_instance PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
pulse_add_dbus_test(app_single_instance test_single_instance 15)

qt_add_executable(test_qml_ui test_qml_ui.cpp)
target_compile_definitions(test_qml_ui PRIVATE
    PULSE_BUILD_BIN="${CMAKE_BINARY_DIR}/bin"
    PULSE_APP_QML="${PROJECT_SOURCE_DIR}/src/app/qml"
    PULSE_WIDGET_QML="${PROJECT_SOURCE_DIR}/package/contents/ui"
)
target_link_libraries(test_qml_ui PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
add_dependencies(test_qml_ui hardwarepulseplugin)
foreach(test_name IN ITEMS testAppNavigationKeyboardAndResponsiveRendering testWidgetComponentsLoad testPanelPresetsPreserveCustomMetrics testProcessInspectionIdentityAndFiltering testSessionDialogsAndUnsavedClose)
    pulse_add_dbus_test(qml_${test_name} test_qml_ui 25 ${test_name})
    set_tests_properties(qml_${test_name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${CMAKE_BINARY_DIR}/bin;XDG_DATA_DIRS=${CMAKE_BINARY_DIR}/share:/usr/local/share:/usr/share;PULSE_TEST_ARTIFACT_DIR=${CMAKE_BINARY_DIR}/test-artifacts;${PULSE_TEST_XDG_ENV}"
        TIMEOUT 30
    )
endforeach()

foreach(name IN ITEMS hardware_v09 sessions_v09 performance_v09)
    qt_add_executable(test_${name} test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE loofipulsecore Qt6::Core Qt6::Test)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()

qt_add_executable(test_collector_v09 test_collector_v09.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/pulsetelemetrycontroller.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/collectorclient.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/telemetryselection.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/reportexport.cpp)
target_include_directories(test_collector_v09 PRIVATE ${PROJECT_SOURCE_DIR}/src/plugin)
target_compile_definitions(test_collector_v09 PRIVATE LOOFI_PULSE_VERSION="${PROJECT_VERSION}" PULSE_SERVICE_BIN="$<TARGET_FILE:loofi-pulse-service>" PULSE_BUILD_BIN="${CMAKE_BINARY_DIR}/bin" PULSE_APP_QML="${PROJECT_SOURCE_DIR}/src/app/qml")
target_link_libraries(test_collector_v09 PRIVATE loofipulsecore Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Widgets Qt6::Test KF6::Notifications KF6::I18n)
add_dependencies(test_collector_v09 loofi-pulse-service hardwarepulseplugin)
pulse_add_dbus_test(collector_v09 test_collector_v09 80)
set_tests_properties(collector_v09 PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/test-state/collector/config;XDG_DATA_HOME=${CMAKE_BINARY_DIR}/test-state/collector/data;XDG_CACHE_HOME=${CMAKE_BINARY_DIR}/test-state/collector/cache")
pulse_add_dbus_test(collector_full_history_v09 test_collector_v09 80 actualQmlProcessPageResponsiveness)
set_tests_properties(collector_full_history_v09 PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PULSE_PREFILL_HISTORY=1;PULSE_SOAK_MS=10000;XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/test-state/full-history/config;XDG_DATA_HOME=${CMAKE_BINARY_DIR}/test-state/full-history/data;XDG_CACHE_HOME=${CMAKE_BINARY_DIR}/test-state/full-history/cache")
set_tests_properties(performance_v09 PROPERTIES TIMEOUT 60)

qt_add_executable(test_notifications_v09 test_notifications_v09.cpp)
target_include_directories(test_notifications_v09 PRIVATE ${PROJECT_SOURCE_DIR}/src/plugin)
target_compile_definitions(test_notifications_v09 PRIVATE PULSE_SERVICE_BIN="$<TARGET_FILE:loofi-pulse-service>")
target_link_libraries(test_notifications_v09 PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
add_dependencies(test_notifications_v09 loofi-pulse-service)
pulse_add_dbus_test(notifications_v09 test_notifications_v09 80)
set_tests_properties(notifications_v09 PROPERTIES TIMEOUT 90 ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_DATA_DIRS=${CMAKE_BINARY_DIR}/share:/usr/share;XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/test-state/notifications/config;XDG_DATA_HOME=${CMAKE_BINARY_DIR}/test-state/notifications/data;XDG_CACHE_HOME=${CMAKE_BINARY_DIR}/test-state/notifications/cache")

qt_add_executable(test_historystore test_historystore.cpp)
target_link_libraries(test_historystore PRIVATE loofipulsecore Qt6::Core Qt6::Sql Qt6::Test)
add_test(NAME historystore COMMAND test_historystore)
set_tests_properties(historystore PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;${PULSE_TEST_XDG_ENV}")

qt_add_executable(test_i18n test_i18n.cpp)
target_compile_definitions(test_i18n PRIVATE
    PULSE_BUILD_LOCALE_DIR="${CMAKE_BINARY_DIR}/locale"
)
target_link_libraries(test_i18n PRIVATE Qt6::Core Qt6::Test KF6::I18n)
add_dependencies(test_i18n pofiles)
add_test(NAME i18n COMMAND test_i18n)
set_tests_properties(i18n PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;${PULSE_TEST_XDG_ENV}")

find_program(MSGFMT_EXECUTABLE msgfmt)
if(MSGFMT_EXECUTABLE)
    add_test(NAME i18n_catalogs_valid COMMAND ${MSGFMT_EXECUTABLE} --check -o /dev/null "${PROJECT_SOURCE_DIR}/po/sv/loofi-pulse.po")
    add_test(NAME i18n_applet_catalogs_valid COMMAND ${MSGFMT_EXECUTABLE} --check -o /dev/null "${PROJECT_SOURCE_DIR}/po/sv/plasma_applet_com.github.loofi.hardwarepulse.po")
endif()


