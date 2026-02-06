# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD seeed_xiao_rp2040 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Dshot_EM32 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(Dshot_EM32)

#                      ${PROJECT_NAME}
pico_generate_pio_header(Dshot_EM32 ${CMAKE_CURRENT_LIST_DIR}/dshot_rx.pio )

# by default the header is generated into the build dir
#//pico_generate_pio_header(Dshot_EM32 ${CMAKE_CURRENT_LIST_DIR}/blink.pio)
# however, alternatively you can choose to generate it somewhere else (in this case in the source tree for check in)
#pico_generate_pio_header(Dshot_EM32 ${CMAKE_CURRENT_LIST_DIR}/blink.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR})

target_sources(Dshot_EM32 PRIVATE 
    Dshot_EM32.c 
    Dshot.c 
    KissTelemetry.c
#    AD.c
    )


target_link_libraries(Dshot_EM32 PRIVATE 
    pico_stdlib 
    hardware_pio 
    hardware_uart 
    hardware_gpio 
    hardware_dma
#    hardware_adc
    )

# enable usb output, disable uart output
pico_enable_stdio_usb(Dshot_EM32 1)
pico_enable_stdio_uart(Dshot_EM32 0)

pico_add_extra_outputs(Dshot_EM32)

# add url via pico_set_program_url
