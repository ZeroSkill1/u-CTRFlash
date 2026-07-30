cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(mcu_flasher C CXX ASM)


set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (PICO_SDK_VERSION_STRING VERSION_LESS "2.2.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.2.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# If you want debug output from USB (pass -DPICO_STDIO_USB=1) this ensures you don't lose any debug output while USB is set up
if (NOT DEFINED PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS)
    set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)
endif()

# Initialize the SDK
pico_sdk_init()


add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        )
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-maybe-uninitialized)
endif()




include_directories(mcu_flasher)

set(HEADER_FILES
	tool78/tool78_cmds.h
	tool78/tool78_hw.h
	tool78/tool78_defs.h
	tool78/tool78_hw_helpers.h)

set(SOURCES
	mcu_flasher.c
	tool78/
	tool78/tool78_cmds.c
	tool78/tool78_hw_78k0r_uart1.c
	tool78/tool78_hw_rl78_uart1.c
	tool78/tool78_hw_helpers.c
	)

add_executable(mcu_flasher
    ${HEADER_FILES} ${SOURCES}
    )

pico_enable_stdio_usb(mcu_flasher 1)

pico_generate_pio_header(mcu_flasher ${PROJECT_SOURCE_DIR}/tool78/tool78.pio)

target_include_directories(mcu_flasher PRIVATE ${PROJECT_SOURCE_DIR}/tool78)

# pull in common dependencies
target_link_libraries(mcu_flasher hardware_pio hardware_exception pico_stdlib pico_multicore)


# create map/bin/hex file etc.
pico_add_extra_outputs(mcu_flasher)
