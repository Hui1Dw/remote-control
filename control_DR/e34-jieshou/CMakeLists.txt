cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME e34-jieshou)

# Processor and link settings
set(TARGET_MCU STM32G431xx)
set(TARGET_FPU "fpv4-sp-d16")
set(TARGET_FLOAT_ABI "hard")
set(TARGET_CORE "cm4")

set(CPU_PARAMETERS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=${TARGET_FPU}
    -mfloat-abi=${TARGET_FLOAT_ABI}
)
set(COMMON_DEFINES -DSTM32G431xx -DUSE_HAL_DRIVER -DUSE_FULL_LL_DRIVER -DHSE_VALUE=8000000 -DHSE_STARTUP_TIMEOUT=100 -DLSE_VALUE=32768 -DLSE_STARTUP_TIMEOUT=5000 -DLSI_VALUE=32000 -DHSI_VALUE=16000000 -DHSI48_VALUE=48000000)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C CXX ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# CPU / FPU compile flags for the executable
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    ${CPU_PARAMETERS}
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++14 -fno-rtti -fno-exceptions -fno-threadsafe-statics>
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ${COMMON_DEFINES})

# Linker flags for the executable
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    ${CPU_PARAMETERS}
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -T${PROJECT_SOURCE_DIR}/STM32G431XX_FLASH.ld
    -specs=nano.specs
    -specs=nosys.specs
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Startup file must be in the executable target, not the library
    ${PROJECT_SOURCE_DIR}/startup_stm32g431xx.s

    # Core sources — moved from static library to main target so
    # their strong symbols override the startup file's weak aliases
    # (SysTick_Handler, etc.)
    ${PROJECT_SOURCE_DIR}/Core/Src/main.c
    ${PROJECT_SOURCE_DIR}/Core/Src/stm32g4xx_hal_msp.c
    ${PROJECT_SOURCE_DIR}/Core/Src/stm32g4xx_it.c
    ${PROJECT_SOURCE_DIR}/Core/Src/uart_rx.c
    ${PROJECT_SOURCE_DIR}/Core/Src/e34_rx.c
    ${PROJECT_SOURCE_DIR}/Core/Src/uart2_rx.c
    ${PROJECT_SOURCE_DIR}/Core/Src/uart3_rx.c
    ${PROJECT_SOURCE_DIR}/Core/Src/rx_dispatcher.c
    ${PROJECT_SOURCE_DIR}/Core/Src/control_protocol.c
    ${PROJECT_SOURCE_DIR}/Core/Src/receiver_control.c
    ${PROJECT_SOURCE_DIR}/Core/Src/debug.c
    ${PROJECT_SOURCE_DIR}/Bsp/Src/bsp_time.cpp
    ${PROJECT_SOURCE_DIR}/Bsp/Src/bsp_gpio.cpp
    ${PROJECT_SOURCE_DIR}/Board/Src/board.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/Bsp/Inc
    ${PROJECT_SOURCE_DIR}/Board/Inc
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    m
    c
    gcc
)

# Single firmware image: build/<preset>/e34-jieshou.elf
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES SUFFIX ".elf")

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "Firmware size"
)
