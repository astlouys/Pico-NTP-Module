# ==========================================================================================================================================
# CMakeLists.txt for project Pico-NTP-Example
# St-Louys Andre - May 2025
# astlouys@gmail.com
# Revision 17-MAY-2025
# Version 1.00
#
# REVISION HISTORY:
# =================
# 17-MAY-2025 1.00 - Initial release.
# ==========================================================================================================================================
#
#
cmake_minimum_required(VERSION 3.16)
#
#
# Set board type.
set(PICO_BOARD pico_w CACHE STRING "Board type")
#
#
# <pico_sdk_import.cmake> is a symbolic link pointing to the SDK. This way, the symbolic link always point to the latest SDK version.
# A rebuild will automatically use the latest version without any other required change.
include(pico_sdk_import.cmake)
#
#
# Specify project name.
project(Pico-NTP-Example)
#
#
set (C_STANDARD 11)
set (CXX_STANDARD 17)
#
#
# Initialize the SDK
pico_sdk_init()
#
#
if (PICO_CYW43_SUPPORTED) # set by BOARD=pico-w
  if (NOT TARGET pico_cyw43_arch)
    message("Skipping build as support is not available")
  else()
    set(WIFI_SSID      "$ENV{WIFI_SSID}"      CACHE INTERNAL "WIFI_SSID")
    set(WIFI_PASSWORD  "$ENV{WIFI_PASSWORD}"  CACHE INTERNAL "WIFI_PASSWORD")
    # set(MQTT_BROKER_IP "$ENV{MQTT_BROKER_IP}" CACHE INTERNAL "MQTT_BROKER_IP")
    # set(MQTT_PASSWORD  "$ENV{MQTT_PASSWORD}"  CACHE INTERNAL "MQTT_PASSWORD")
    message("========================================================================================================")
    message("Setting WiFi SSID:                <${WIFI_SSID}>")
    message("Setting WiFi password:            <${WIFI_PASSWORD}>")
    # message("Setting MQTT broker IP address to <${MQTT_BROKER_IP}>")
    # message("Setting MQTT broker password to   <${MQTT_PASSWORD}>")
    message("========================================================================================================")
    if ("${WIFI_SSID}" STREQUAL "")
      message("Environment variable WIFI_SSID (network name) is not defined... aborting build process.")
      message("(edit your .bashrc file to add the WIFI_SSID environment variable or modify the CMakeLists.txt file to define it there.")
      return()
    elseif ("${WIFI_PASSWORD}" STREQUAL "")
      message("Environment variable WIFI_PASSWORD (network password) is not defined... aborting build process.")
      message("(edit your .bashrc file to add the WIFI_PASSWORD environment variable or modify the CMakeLists.txt file to define it there.")
      return()
    # elseif ("${MQTT_BROKER_IP}" STREQUAL "")
    #   message("Environment variable MQTT_BROKER_IP is not defined... aborting build process.")
    #   message("(edit your .bashrc file to add the MQTT_BROKER_IP environment variable or modify the CMakeLists.txt file to define it there.")
    #   return()
    # elseif ("${MQTT_PASSWORD}" STREQUAL "")
    #   message("Environment variable MQTT_PASSWORD is not defined... aborting build process.")
    #   message("(edit your .bashrc file to add the MQTT_PASSWORD environment variable or modify the CMakeLists.txt file to define it there.")
    #   return()
    else()
      #
      add_executable(
        Pico-NTP-Example
        Pico-NTP-Example.c
        Pico-NTP-Module.c
        Pico-WiFi-Module.c
        )
      #
      #
      #
      #
      # add_compile_definitions(WIFI_SSID="${WIFI_SSID}" WIFI_PASSWORD="${WIFI_PASSWORD}")
      target_compile_definitions(
        Pico-NTP-Example PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        # MQTT_BROKER_IP=\"${MQTT_BROKER_IP}\"
        # MQTT_PASSWORD=\"${MQTT_PASSWORD}\"
        NO_SYS=1
      )
      #
      # Add the standard include files / directories to the build
      target_include_directories(
        Pico-NTP-Example PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR} /.. # for our common lwipopts
        ${PICO_LWIP_CONTRIB_PATH}/apps/ping
        )
      #
      #
      # include_directories(/home/pi/pico/pico-sdk/lib/lwip/contrib/ports/unix/port/include)
      # Add libraries requested to the build
      target_link_libraries(
        Pico-NTP-Example
        hardware_clocks
        hardware_rtc
        pico_cyw43_arch_lwip_threadsafe_background
        pico_stdlib
      )
      #
      #
      # Enable usb output, disable uart output
      pico_enable_stdio_usb(Pico-NTP-Example  1)
      pico_enable_stdio_uart(Pico-NTP-Example 0)
      #
      #
      pico_add_extra_outputs(Pico-NTP-Example)
      #
      #
      pico_add_library(pico_httpd_content NOFLAG)
    endif()
  endif()
endif()
#
