set(hardwarepulseplugin_SRCS
    pulsetelemetrycontroller.cpp
)

set(hardwarepulseplugin_HDRS
    pulsetelemetrycontroller.h
)

qt_add_qml_module(hardwarepulseplugin
    URI com.github.loofi.hardwarepulse
    VERSION 1.0
    PLUGIN_TARGET hardwarepulseplugin
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/com/github/loofi/hardwarepulse
    SOURCES ${hardwarepulseplugin_SRCS} ${hardwarepulseplugin_HDRS}
)

set_target_properties(hardwarepulseplugin PROPERTIES PREFIX "lib")

target_compile_definitions(hardwarepulseplugin
    PRIVATE LOOFI_PULSE_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(hardwarepulseplugin
    PUBLIC
    loofipulsecore
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    KF6::Notifications
    KF6::I18n
)

install(TARGETS hardwarepulseplugin
        DESTINATION ${KDE_INSTALL_QMLDIR}/com/github/loofi/hardwarepulse)
install(FILES ${CMAKE_BINARY_DIR}/bin/com/github/loofi/hardwarepulse/qmldir
              ${CMAKE_BINARY_DIR}/bin/com/github/loofi/hardwarepulse/hardwarepulseplugin.qmltypes
        DESTINATION ${KDE_INSTALL_QMLDIR}/com/github/loofi/hardwarepulse)
