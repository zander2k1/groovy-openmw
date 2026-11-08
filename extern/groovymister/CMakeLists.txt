# GroovyMiSTer: sends frames over the network to a MiSTer FPGA running the Groovy_MiSTer core,
# which outputs them to a CRT. Library by psakhis (https://github.com/psakhis/Groovy_MiSTer),
# taken from Groovy Dolphin. LZ4 by Yann Collet (BSD 2-clause, see LICENSE.lz4).

add_library(groovymister STATIC
    groovymister.cpp
    groovymister_wrapper.cpp
    lz4.c
    lz4hc.c
)

target_include_directories(groovymister PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (WIN32)
    target_compile_definitions(groovymister PUBLIC GROOVYMISTER_WIN32_STATIC _WINSOCK_DEPRECATED_NO_WARNINGS
        _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(groovymister PUBLIC ws2_32 mswsock)
endif()

# Third-party code: don't fail the build on its warnings.
if (MSVC)
    target_compile_options(groovymister PRIVATE /W0 /WX-)
else()
    target_compile_options(groovymister PRIVATE -w)
endif()
