# HUB75 RGB LED matrix library utilizing ESP32 DMA Engine
# https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA
# MIT License

cmake_minimum_required(VERSION 3.5)
idf_build_get_property(target IDF_TARGET)

# ESP32-C3, H2, C2 etc. do not support I2S parallel output
set(DMA_CAPABLE_TARGETS "esp32" "esp32s2" "esp32s3")

if(NOT target IN_LIST DMA_CAPABLE_TARGETS)
  # This device only supports the MatrixPanel_GPIO implementation
  set(COMPONENT_SRCS "src/ESP32-HUB75-MatrixPanel-GPIO.cpp" "src/ESP32-HUB75-MatrixPanel-leddrivers.cpp")
else()
  set(COMPONENT_SRCS "src/ESP32-HUB75-MatrixPanel-I2S-DMA.cpp"
                     "src/ESP32-HUB75-MatrixPanel-leddrivers.cpp"
                     "src/platforms/esp32/esp32_i2s_parallel_dma.cpp")

  if(target STREQUAL "esp32s3")
    list(APPEND COMPONENT_SRCS "src/platforms/esp32s3/gdma_lcd_parallel16.cpp")
    list(APPEND REQUIRES esp_lcd)
  endif()
endif()

# Common dependencies
list(APPEND REQUIRES driver esp_timer)

if(ARDUINO_ARCH_ESP32)
  list(APPEND REQUIRES arduino)
endif()

# GFX Library dependencies
if(CONFIG_ESP32_HUB75_USE_GFX)
  list(APPEND build_dependencies Adafruit-GFX-Library)
elseif(CONFIG_ESP32_HUB75_USE_GFX_LITE)
  list(APPEND build_dependencies GFX_Lite)
endif()

idf_component_register(SRCS ${COMPONENT_SRCS}
                       INCLUDE_DIRS "src"
                       REQUIRES ${REQUIRES}
                      )

# Handle external library dependencies
idf_build_get_property(components BUILD_COMPONENTS)
foreach(component_name IN LISTS build_dependencies)
  if (NOT ${component_name} IN_LIST components)
    message(FATAL_ERROR "Missing component: ${component_name}")
  endif()
  idf_component_get_property(lib_name ${component_name} COMPONENT_LIB)
  target_link_libraries(${COMPONENT_LIB} PUBLIC ${lib_name})
endforeach()

project(ESP32-HUB75-MatrixPanel-I2S-DMA)
