set(loofipulsecore_SRCS
    sysfsreader.cpp
    psimemorysampler.cpp
    cpusocsampler.cpp
    powerflowsampler.cpp
    causaldiagnosisengine.cpp
    flightrecorder.cpp
    sessionstore.cpp
    historystore.cpp
    kernellogreader.cpp
)

set(loofipulsecore_HDRS
    telemetrytypes.h
    sysfsreader.h
    psimemorysampler.h
    cpusocsampler.h
    powerflowsampler.h
    causaldiagnosisengine.h
    flightrecorder.h
    sessionstore.h
    historystore.h
    kernellogreader.h
)

add_library(loofipulsecore STATIC
    ${loofipulsecore_SRCS}
    ${loofipulsecore_HDRS}
)

set_target_properties(loofipulsecore PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(loofipulsecore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(loofipulsecore PUBLIC
    Qt6::Core
    Qt6::DBus
    Qt6::Sql
)

target_compile_definitions(loofipulsecore PUBLIC LOOFI_PULSE_VERSION="${PROJECT_VERSION}")
