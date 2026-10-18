# Keep D-Bus activations in a private process group and capture their output.
# Portal services can otherwise outlive the test, retain its pipes, and race RPM cleanup.
find_program(PULSE_SETSID setsid REQUIRED)
find_program(PULSE_BASH bash REQUIRED)
find_program(PULSE_TIMEOUT timeout REQUIRED)
get_filename_component(log_directory "${TEST_LOG}" DIRECTORY)
file(MAKE_DIRECTORY "${log_directory}")
math(EXPR wrapper_timeout "${TEST_TIMEOUT} + 10")
execute_process(COMMAND "${PULSE_SETSID}" --wait "${PULSE_BASH}"
    "${CMAKE_CURRENT_LIST_DIR}/run_dbus_test.sh" "${TEST_TIMEOUT}"
    "${DBUS_RUN_SESSION}" "${TEST_EXECUTABLE}" ${TEST_ARGUMENTS}
    RESULT_VARIABLE result TIMEOUT "${wrapper_timeout}"
    OUTPUT_FILE "${TEST_LOG}" ERROR_FILE "${TEST_LOG}")
file(READ "${TEST_LOG}" output)
message("${output}")
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Isolated D-Bus test failed: ${result}")
endif()
