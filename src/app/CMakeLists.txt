qt_add_executable(loofi-pulse
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/pulsetelemetrycontroller.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/pulsetelemetrycontroller.h
    qml.qrc
)

set_target_properties(loofi-pulse PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    QT_QML_SKIP_QML_IMPORT_SCANNER TRUE
)

add_dependencies(loofi-pulse hardwarepulseplugin)

target_compile_definitions(loofi-pulse PRIVATE LOOFI_PULSE_VERSION="${PROJECT_VERSION}")

target_include_directories(loofi-pulse PRIVATE
    ${CMAKE_SOURCE_DIR}/src/plugin
)

target_link_libraries(loofi-pulse
    PRIVATE
    loofipulsecore
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::Kirigami
    KF6::CoreAddons
    KF6::Notifications
    KF6::I18n
)

install(TARGETS loofi-pulse RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})
