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


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME STM32F407_RTT)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Set toolchain file for ARM Cortex-M4
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

set(RTTHREAD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/RT-Thread)
set(LVGL_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/LVGL)
file(GLOB_RECURSE LVGL_SOURCES ${LVGL_ROOT}/src/*.c)
# Graphical UI tools can export here; run CMake configure again after adding files.
# CONFIGURE_DEPENDS is intentionally avoided because some Windows Ninja/PTY
# combinations can wait indefinitely in CMake's always-run VerifyGlobs step.
file(GLOB_RECURSE LVGL_UI_GENERATED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Applications/lvgl_ui/generated/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Applications/lvgl_ui/custom/*.c
)
set(RTTHREAD_KERNEL_SOURCES
    ${RTTHREAD_ROOT}/src/clock.c
    ${RTTHREAD_ROOT}/src/cpu_up.c
    ${RTTHREAD_ROOT}/src/defunct.c
    ${RTTHREAD_ROOT}/src/idle.c
    ${RTTHREAD_ROOT}/src/ipc.c
    ${RTTHREAD_ROOT}/src/irq.c
    ${RTTHREAD_ROOT}/src/kservice.c
    ${RTTHREAD_ROOT}/src/mem.c
    ${RTTHREAD_ROOT}/src/mempool.c
    ${RTTHREAD_ROOT}/src/object.c
    ${RTTHREAD_ROOT}/src/scheduler_comm.c
    ${RTTHREAD_ROOT}/src/scheduler_up.c
    ${RTTHREAD_ROOT}/src/thread.c
    ${RTTHREAD_ROOT}/src/timer.c
    ${RTTHREAD_ROOT}/src/klibc/kerrno.c
    ${RTTHREAD_ROOT}/src/klibc/kstdio.c
    ${RTTHREAD_ROOT}/src/klibc/kstring.c
    ${RTTHREAD_ROOT}/src/klibc/rt_vsnprintf_tiny.c
    ${RTTHREAD_ROOT}/src/klibc/rt_vsscanf.c
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Applications/app_init.c
    Applications/runtime/app_tasks.c
    Applications/serial_console_ui.c
    Applications/shell_commands.c
    Applications/lvgl_port/lv_port_disp.c
    Applications/lvgl_port/lv_port_indev.c
    Applications/lvgl_ui/ui_main_menu.c
    Applications/lvgl_ui/ui_navigation.c
    Applications/lvgl_ui/ui_boot_screen.c
    Applications/lvgl_ui/ui_feature_pages.c
    Applications/lvgl_ui/ui_font_chinese_14.c
    ${LVGL_UI_GENERATED_SOURCES}
    BSP/lcd/st7789_fsmc.c
    BSP/services/board_service.c
    BSP/algorithms/sensor_fusion.c
    BSP/drivers/bus/soft_i2c.c
    BSP/drivers/bus/spi_driver.c
    BSP/drivers/bus/spi2_adapter.c
    BSP/drivers/sensors/aht21.c
    BSP/drivers/sensors/ap3216c.c
    BSP/drivers/sensors/icm20608.c
    BSP/drivers/devices/w25q_device.c
    BSP/drivers/devices/rw007_device.c
    BSP/drivers/storage/board_storage.c
    BSP/drivers/wireless/rw007_hw.c
    BSP/drivers/led/led_ring.c
    BSP/drivers/diagnostics/gpio_monitor.c
    BSP/rt_hw_console.c
    BSP/rtthread_startup.c
    Core/Src/usart.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_sram.c
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_fsmc.c
    ${LVGL_SOURCES}
    ${RTTHREAD_KERNEL_SOURCES}
    ${RTTHREAD_ROOT}/components/finsh/shell.c
    ${RTTHREAD_ROOT}/components/finsh/msh.c
    ${RTTHREAD_ROOT}/components/finsh/msh_parse.c
    ${RTTHREAD_ROOT}/components/finsh/cmd.c
    ${RTTHREAD_ROOT}/libcpu/arm/cortex-m4/cpuport.c
    ${RTTHREAD_ROOT}/libcpu/arm/cortex-m4/context_gcc.S
    ${RTTHREAD_ROOT}/libcpu/arm/common/atomic_arm.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Applications
    Applications/runtime
    Applications/lvgl_port
    Applications/lvgl_ui
    Applications/lvgl_ui/generated
    Applications/lvgl_ui/custom
    BSP
    BSP/lcd
    BSP/services
    BSP/algorithms
    BSP/drivers/bus
    BSP/drivers/devices
    BSP/drivers/sensors
    BSP/drivers/storage
    BSP/drivers/wireless
    BSP/drivers/led
    BSP/drivers/diagnostics
    ${LVGL_ROOT}
    ${LVGL_ROOT}/src
    ${RTTHREAD_ROOT}/include
    ${RTTHREAD_ROOT}/components/finsh
    ${RTTHREAD_ROOT}/libcpu/arm/cortex-m4
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    RT_USING_RTTHREAD
    __RT_KERNEL_SOURCE__
    LV_CONF_INCLUDE_SIMPLE
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    m

    # Add user defined libraries
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary -S
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
)
