# userland/cli-application/CMakeLists.txt

add_executable(cliApplication
    Application.cpp
    Helpers/Metrics.cpp
    Helpers/SBPio.cpp
    Helpers/DHT11.cpp
    Helpers/SBRp1IO.cpp
)

target_include_directories(cliApplication
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Helpers
)

# Link against tracing library (from Tracer subdir)
target_link_libraries(cliApplication
    PRIVATE
        tracing
        gpiod
)

set_target_properties(cliApplication PROPERTIES
    BUILD_RPATH "\$ORIGIN"
    INSTALL_RPATH "\$ORIGIN"
)
