CMAKE_MINIMUM_REQUIRED(VERSION 3.13.2)

project(MODULE_CHIPTUNE)

if (NOT DEFINED HARDWARE_FLAGS)
    message(FATAL_ERROR "The value of the variable <<HARDWARE_FLAGS>> is not specified!")
endif ()

if (NOT DEFINED ${PROJECT_NAME}_COMP_FLAGS)
    message(FATAL_ERROR "The value of the variable <<${PROJECT_NAME}_COMP_FLAGS>> is not specified!")
endif ()

if (NOT DEFINED LIB_CFG_H_FILES_PATH)
    message(FATAL_ERROR "The value of the variable <<LIB_CFG_H_FILES_PATH>> is not specified!")
endif ()

if (NOT DEFINED USER_OS_H_FILES_PATH)
    message(FATAL_ERROR "The value of the variable <<USER_OS_H_FILES_PATH>> is not specified!")
endif ()

if (NOT DEFINED MODULE_MC_HARDWARE_INTERFACES_H_FILES_PATH)
    message(FATAL_ERROR "The value of the variable <<MODULE_MC_HARDWARE_INTERFACES_H_FILES_PATH>> is not specified!")
endif ()

if (NOT DEFINED MODULE_SHIFT_REGISTER_BASE_H_FILES_PATH)
    message(FATAL_ERROR "The value of the variable <<MODULE_SHIFT_REGISTER_BASE_H_FILES_PATH>> is not specified!")
endif ()

if (NOT DEFINED MODULE_RUN_TIME_LOGGER_H_FILES_PATH)
    message(FATAL_ERROR "The value of the variable <<MODULE_RUN_TIME_LOGGER_H_FILES_PATH>> is not specified!")
endif ()

if (NOT DEFINED MODULE_FATFS_H_FILES_PATH)
    message(FATAL_ERROR "The value of the variable <<MODULE_FATFS_H_FILES_PATH>> is not specified!")
endif ()

if (NOT DEFINED MODULE_FAT_CONTROLLER_H_FILES_PATH)
    message(FATAL_ERROR "The value of the variable <<MODULE_FAT_CONTROLLER_H_FILES_PATH>> is not specified!")
endif ()

include_directories(
        ${LIB_CFG_H_FILES_PATH}
        ${USER_OS_H_FILES_PATH}
        ${MODULE_MC_HARDWARE_INTERFACES_H_FILES_PATH}
        ${MODULE_SHIFT_REGISTER_BASE_H_FILES_PATH}
        ${MODULE_RUN_TIME_LOGGER_H_FILES_PATH}
        ${MODULE_FATFS_H_FILES_PATH}
        ${MODULE_FAT_CONTROLLER_H_FILES_PATH}
        aym/file_play/inc
        aym/file_play/readers/inc
        aym/file_play/mc_file_container/inc
        aym/low_lavel/inc
        aym/note_mode/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/submodules/zxtune/include
        ${CMAKE_CURRENT_SOURCE_DIR}/submodules/zxtune/src/binary
        ${CMAKE_CURRENT_SOURCE_DIR}/submodules/zxtune/src
        ${CMAKE_CURRENT_SOURCE_DIR}/submodules/zxtune/src/formats
        ${CMAKE_CURRENT_SOURCE_DIR}/submodules/zxtune/src/formats/chiptune
        ${CMAKE_CURRENT_SOURCE_DIR}/submodules/zxtune/src/formats/chiptune/aym
)

file(GLOB ${PROJECT_NAME}_SRC_LIST
        "${CMAKE_CURRENT_SOURCE_DIR}/*/*/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*/*/*/src/*.cpp")

add_library(${PROJECT_NAME} STATIC ${${PROJECT_NAME}_SRC_LIST})

target_compile_options(${PROJECT_NAME} PRIVATE ${HARDWARE_FLAGS} ${${PROJECT_NAME}_COMP_FLAGS})