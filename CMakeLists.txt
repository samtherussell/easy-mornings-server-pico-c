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
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(easy-mornings-server-pico-c C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(easy-mornings-server-pico-c
        easy-mornings-server-pico-c.c
        src/alarm.c
        src/clock.c
        src/handler.c
        src/http_server.c
        src/json.c
        src/light.c
        src/wifi.c
        )

# WiFi credentials: pass -DWIFI_SSID=... -DWIFI_PASSWORD=... or set them as
# environment variables, otherwise the defaults in config.h are used.
if (NOT DEFINED WIFI_SSID AND DEFINED ENV{WIFI_SSID})
    set(WIFI_SSID $ENV{WIFI_SSID})
endif()
if (NOT DEFINED WIFI_PASSWORD AND DEFINED ENV{WIFI_PASSWORD})
    set(WIFI_PASSWORD $ENV{WIFI_PASSWORD})
endif()
if (DEFINED WIFI_SSID)
    target_compile_definitions(easy-mornings-server-pico-c PRIVATE WIFI_SSID=\"${WIFI_SSID}\")
endif()
if (DEFINED WIFI_PASSWORD)
    target_compile_definitions(easy-mornings-server-pico-c PRIVATE WIFI_PASSWORD=\"${WIFI_PASSWORD}\")
endif()

# hostname advertised over DHCP
target_compile_definitions(easy-mornings-server-pico-c PRIVATE CYW43_HOST_NAME=\"easy-mornings\")

pico_set_program_name(easy-mornings-server-pico-c "easy-mornings-server-pico-c")
pico_set_program_version(easy-mornings-server-pico-c "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(easy-mornings-server-pico-c 0)
pico_enable_stdio_usb(easy-mornings-server-pico-c 1)

# Add the standard library to the build
target_link_libraries(easy-mornings-server-pico-c
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(easy-mornings-server-pico-c PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(easy-mornings-server-pico-c
        pico_cyw43_arch_lwip_poll
        pico_lwip_sntp
        hardware_pwm
        hardware_watchdog
        )

pico_add_extra_outputs(easy-mornings-server-pico-c)

