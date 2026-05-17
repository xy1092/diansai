cmake_minimum_required(VERSION 3.20)

project(nuedc_car LANGUAGES C ASM)

set(CMAKE_EXECUTABLE_SUFFIX ".out")
option(NUEDC_MINIMAL_DEBUG "Build with BSP/protocol stubs to enable breakpoint debugging before full pinmux is ready" OFF)
option(NUEDC_NO_ENCODER "Build for open-loop motor control without wheel encoders" OFF)
option(NUEDC_USE_ULTRASONIC "Enable reserved ultrasonic interface BSP on PA12/PA13" OFF)
option(NUEDC_USE_K230 "Enable K230 BSP only when SysConfig defines UART_K230 pins" OFF)

if(DEFINED ENV{MSPM0_SDK_ROOT} AND NOT "$ENV{MSPM0_SDK_ROOT}" STREQUAL "")
  set(MSPM0_SDK_ROOT "$ENV{MSPM0_SDK_ROOT}" CACHE PATH "Path to the MSPM0 SDK root")
else()
  set(MSPM0_SDK_ROOT "" CACHE PATH "Path to the MSPM0 SDK root")
endif()
set(MSPM0_STARTUP_FILE "" CACHE FILEPATH "MSPM0 startup file from the SDK")
set(MSPM0_LINKER_SCRIPT "" CACHE FILEPATH "GNU linker script for MSPM0G3507")

function(find_first_match output_var)
  set(found "")
  foreach(pattern IN LISTS ARGN)
    file(GLOB_RECURSE matches LIST_DIRECTORIES false "${pattern}")
    list(SORT matches)
    if(matches)
      list(GET matches 0 found)
      break()
    endif()
  endforeach()
  set(${output_var} "${found}" PARENT_SCOPE)
endfunction()

if(NOT MSPM0_SDK_ROOT)
  foreach(candidate IN ITEMS
    "/home/xy/ti/mspm0_sdk_2_10_00_04"
    "/home/xy/ti/mspm0_sdk_2_07_00_05"
    "/home/xy/ti/mspm0_sdk"
    "/opt/ti/mspm0_sdk"
    "$ENV{HOME}/ti/mspm0_sdk_2_10_00_04"
    "$ENV{HOME}/ti/mspm0_sdk_2_07_00_05"
  )
    if(EXISTS "${candidate}")
      set(MSPM0_SDK_ROOT "${candidate}" CACHE PATH "Path to the MSPM0 SDK root" FORCE)
      break()
    endif()
  endforeach()
endif()

if(NOT MSPM0_SDK_ROOT)
  message(FATAL_ERROR
    "MSPM0 SDK not found. Install the SDK and set MSPM0_SDK_ROOT.")
elseif(NOT EXISTS "${MSPM0_SDK_ROOT}/.metadata/product.json")
  message(FATAL_ERROR
    "MSPM0 SDK not found. Install the SDK and set MSPM0_SDK_ROOT. "
    "Expected: ${MSPM0_SDK_ROOT}/.metadata/product.json")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/ti_msp_dl_config.c" AND EXISTS "${CMAKE_SOURCE_DIR}/ti_msp_dl_config.h")
  set(SYSCFG_GENERATED_C "${CMAKE_SOURCE_DIR}/ti_msp_dl_config.c")
else()
  message(FATAL_ERROR
    "Missing ti_msp_dl_config.[ch]. Please generate them with CCS/SysConfig first.")
endif()

if(NOT MSPM0_STARTUP_FILE
   OR NOT EXISTS "${MSPM0_STARTUP_FILE}"
   OR MSPM0_STARTUP_FILE MATCHES "/iar/"
   OR MSPM0_STARTUP_FILE MATCHES "/ticlang/")
  find_first_match(auto_startup
    "${MSPM0_SDK_ROOT}/source/ti/devices/msp/m0p/startup_system_files/gcc/startup_mspm0g350x_gcc.c"
    "${MSPM0_SDK_ROOT}/examples/nortos/LP_MSPM0G3507/*/gcc/startup_mspm0g350x_gcc.c"
    "${MSPM0_SDK_ROOT}/*/gcc/startup_mspm0g350x_gcc.c"
    "${MSPM0_SDK_ROOT}/*startup*mspm0g350*gcc.c"
    "${MSPM0_SDK_ROOT}/*startup*mspm0g350*.S"
    "${MSPM0_SDK_ROOT}/*startup*mspm0g350*.s"
  )
  if(auto_startup)
    set(MSPM0_STARTUP_FILE "${auto_startup}" CACHE FILEPATH "MSPM0 startup file from the SDK" FORCE)
  endif()
endif()

if(NOT MSPM0_LINKER_SCRIPT)
  find_first_match(auto_linker
    "${CMAKE_SOURCE_DIR}/*mspm0g3507*.ld"
    "${CMAKE_SOURCE_DIR}/*mspm0g3507*.lds"
    "${CMAKE_SOURCE_DIR}/*linker*.ld"
    "${CMAKE_SOURCE_DIR}/*linker*.lds"
    "${MSPM0_SDK_ROOT}/*mspm0g3507*.ld"
    "${MSPM0_SDK_ROOT}/*mspm0g3507*.lds"
    "${MSPM0_SDK_ROOT}/*gcc*linker*.ld"
    "${MSPM0_SDK_ROOT}/*gcc*linker*.lds"
  )
  if(auto_linker)
    set(MSPM0_LINKER_SCRIPT "${auto_linker}" CACHE FILEPATH "GNU linker script for MSPM0G3507" FORCE)
  endif()
endif()

if(NOT EXISTS "${MSPM0_STARTUP_FILE}")
  message(FATAL_ERROR
    "Unable to find MSPM0 startup file. Set MSPM0_STARTUP_FILE to the SDK startup source.")
endif()

if(NOT EXISTS "${MSPM0_LINKER_SCRIPT}")
  message(FATAL_ERROR
    "Unable to find a GNU linker script. Set MSPM0_LINKER_SCRIPT manually.")
endif()

set(MSPM0_DRIVERLIB_A
  "${MSPM0_SDK_ROOT}/source/ti/driverlib/lib/gcc/m0p/mspm0g1x0x_g3x0x/driverlib.a")
if(NOT EXISTS "${MSPM0_DRIVERLIB_A}")
  message(FATAL_ERROR
    "Unable to find driverlib.a. Expected: ${MSPM0_DRIVERLIB_A}")
endif()

set(NUEDC_APP_SOURCES
  main.c
  ${SYSCFG_GENERATED_C}
  ${MSPM0_STARTUP_FILE}
  app/app_main.c
  app/app_tracking.c
  middleware/filter.c
  middleware/pid.c
  middleware/rtt_log.c
  middleware/telemetry.c
)

if(NUEDC_MINIMAL_DEBUG)
  list(APPEND NUEDC_APP_SOURCES
    bsp/bsp_stub_debug.c
    middleware/protocol_stub_debug.c
  )
else()
  list(APPEND NUEDC_APP_SOURCES
    bsp/bsp_adc.c
    bsp/bsp_imu.c
    bsp/bsp_motor.c
    bsp/bsp_notify.c
    bsp/bsp_oled.c
    bsp/bsp_uart.c
    middleware/protocol.c
  )
  if(NOT NUEDC_NO_ENCODER)
    list(APPEND NUEDC_APP_SOURCES bsp/bsp_encoder.c)
  endif()
  if(NUEDC_USE_ULTRASONIC)
    list(APPEND NUEDC_APP_SOURCES bsp/bsp_ultrasonic.c)
  endif()
  if(NUEDC_USE_K230)
    list(APPEND NUEDC_APP_SOURCES bsp/bsp_k230.c)
  endif()
endif()

add_executable(nuedc_car ${NUEDC_APP_SOURCES})

target_include_directories(nuedc_car PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/app"
  "${CMAKE_SOURCE_DIR}/bsp"
  "${CMAKE_SOURCE_DIR}/middleware"
  "${MSPM0_SDK_ROOT}/source"
  "${MSPM0_SDK_ROOT}/source/third_party/CMSIS/Core/Include"
)

target_compile_definitions(nuedc_car PRIVATE
  __MSPM0G3507__
  $<$<BOOL:${NUEDC_MINIMAL_DEBUG}>:NUEDC_MINIMAL_DEBUG=1>
  $<$<BOOL:${NUEDC_NO_ENCODER}>:NUEDC_NO_ENCODER=1>
  $<$<BOOL:${NUEDC_USE_ULTRASONIC}>:NUEDC_USE_ULTRASONIC=1>
  $<$<BOOL:${NUEDC_USE_K230}>:NUEDC_USE_K230=1>
)

target_compile_options(nuedc_car PRIVATE
  -mcpu=cortex-m0plus
  -mthumb
  -ffunction-sections
  -fdata-sections
  -fno-common
  -Wall
  -Wextra
  -Wno-unused-parameter
  -Og
  -g2
  -gdwarf-4
)

target_link_options(nuedc_car PRIVATE
  -mcpu=cortex-m0plus
  -mthumb
  -nostartfiles
  -T${MSPM0_LINKER_SCRIPT}
  -Wl,-e,Reset_Handler
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_BINARY_DIR}/nuedc_car.map
  --specs=nano.specs
  --specs=nosys.specs
)

target_link_libraries(nuedc_car PRIVATE
  "${MSPM0_DRIVERLIB_A}"
  m
)

find_program(ARM_SIZE arm-none-eabi-size REQUIRED)
find_program(ARM_OBJCOPY arm-none-eabi-objcopy REQUIRED)

add_custom_command(TARGET nuedc_car POST_BUILD
  COMMAND "${ARM_SIZE}" "$<TARGET_FILE:nuedc_car>"
  COMMAND "${ARM_OBJCOPY}" -O ihex "$<TARGET_FILE:nuedc_car>" "${CMAKE_BINARY_DIR}/nuedc_car.hex"
  COMMAND "${ARM_OBJCOPY}" -O binary "$<TARGET_FILE:nuedc_car>" "${CMAKE_BINARY_DIR}/nuedc_car.bin"
  VERBATIM
)

add_custom_target(flash-stlink
  COMMAND "${CMAKE_SOURCE_DIR}/scripts/flash_openocd.sh"
          "interface/stlink.cfg"
          "${CMAKE_SOURCE_DIR}/tools/openocd/mspm0g3507.cfg"
          "$<TARGET_FILE:nuedc_car>"
  DEPENDS nuedc_car
  USES_TERMINAL
)

add_custom_target(flash-jlink-openocd
  COMMAND "${CMAKE_SOURCE_DIR}/scripts/flash_openocd.sh"
          "interface/jlink.cfg"
          "${CMAKE_SOURCE_DIR}/tools/openocd/mspm0g3507.cfg"
          "$<TARGET_FILE:nuedc_car>"
  DEPENDS nuedc_car
  USES_TERMINAL
)

add_custom_target(flash-jlink
  COMMAND "${CMAKE_SOURCE_DIR}/scripts/flash_jlink.sh"
          "${CMAKE_BINARY_DIR}/nuedc_car.bin"
  DEPENDS nuedc_car
  USES_TERMINAL
)
