cmake_minimum_required(VERSION 3.22.0)

include(${SdkRootDirPath}/cmake/extension/mcux.cmake)

project(tof_demo LANGUAGES C CXX ASM PROJECT_BOARD_PORT_PATH ${board_root}/${board}/demo_apps/tof_demo)

include(${SdkRootDirPath}/CMakeLists.txt)

if(DEFINED ENV{TOF_DEMO_ROOT})
    set(TOF_ROOT "$ENV{TOF_DEMO_ROOT}")
else()
    # Default layout: this repo contains ./mcuxsdk_ws and the workspace contains mcuxsdk/.
    # SdkRootDirPath points at <repo>/mcuxsdk_ws/mcuxsdk, so ../.. is the repo root.
    get_filename_component(TOF_ROOT "${SdkRootDirPath}/../.." ABSOLUTE)
endif()

mcux_add_source(
    BASE_PATH ${TOF_ROOT}
    SOURCES src/tof_demo.c
            src/tmf8828_quick.c
            src/par_lcd_s035.c
            src/platform/display_hal.c
)

mcux_add_include(BASE_PATH ${TOF_ROOT} INCLUDES src)

mcux_add_source(
    BASE_PATH ${SdkRootDirPath}
    SOURCES components/touch/gt911/fsl_gt911.c
)

mcux_add_include(
    BASE_PATH ${SdkRootDirPath}
    INCLUDES components/touch/gt911
)

include(${SdkRootDirPath}/${board_root}/${board}/demo_apps/tof_demo/reconfig.cmake OPTIONAL)

mcux_convert_binary(BINARY ${APPLICATION_BINARY_DIR}/${MCUX_SDK_PROJECT_NAME}.bin)
