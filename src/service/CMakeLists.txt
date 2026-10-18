qt_add_executable(loofi-pulse-service main.cpp collectorservice.cpp collectorservice.h
    ${PROJECT_SOURCE_DIR}/src/plugin/pulsetelemetrycontroller.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/collectorclient.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/telemetryselection.cpp
    ${PROJECT_SOURCE_DIR}/src/plugin/reportexport.cpp)
target_include_directories(loofi-pulse-service PRIVATE ${PROJECT_SOURCE_DIR}/src/plugin)
target_compile_definitions(loofi-pulse-service PRIVATE LOOFI_PULSE_VERSION="${PROJECT_VERSION}")
target_link_libraries(loofi-pulse-service PRIVATE loofipulsecore Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Widgets KF6::Notifications KF6::I18n)
set_target_properties(loofi-pulse-service PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" QT_QML_SKIP_QML_IMPORT_SCANNER TRUE)
install(TARGETS loofi-pulse-service RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})
set(PULSE_SERVICE_EXEC "${CMAKE_INSTALL_FULL_BINDIR}/loofi-pulse-service")
configure_file(com.github.loofi.hardwarepulse.Collector1.service.in ${CMAKE_CURRENT_BINARY_DIR}/com.github.loofi.hardwarepulse.Collector1.service @ONLY)
configure_file(install-service.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/install-service.cmake @ONLY)
install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/install-service.cmake)
set(PULSE_SERVICE_EXEC "\"${CMAKE_BINARY_DIR}/bin/loofi-pulse-service\"")
configure_file(com.github.loofi.hardwarepulse.Collector1.service.in ${CMAKE_BINARY_DIR}/share/dbus-1/services/com.github.loofi.hardwarepulse.Collector1.service @ONLY)
