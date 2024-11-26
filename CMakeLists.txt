# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(asquaredos C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Set additional compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O0")

# Add executable. Default name is the project name, version 0.1
add_executable(asquaredos 
    kern/boot.c
    kern/palloc.c
    kern/zalloc.c
    kern/resources.c

    kern/scheduler.c

    kern/context_switch.s
)

pico_set_program_name(asquaredos "asquaredos")
pico_set_program_version(asquaredos "0.1")

# Generate PIO header
pico_generate_pio_header(asquaredos ${CMAKE_CURRENT_LIST_DIR}/blink.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(asquaredos 1)
pico_enable_stdio_usb(asquaredos 1)

# Add the standard library to the build
target_link_libraries(asquaredos
        pico_stdlib
        hardware_exception
        hardware_ticks
        hardware_structs
        )

# Add the standard include files to the build
target_include_directories(asquaredos PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# Add any user requested libraries
target_link_libraries(asquaredos 
        )

pico_add_extra_outputs(asquaredos)
