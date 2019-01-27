# Минимально необходимая для сборки версия cmake.
CMAKE_MINIMUM_REQUIRED (VERSION 3.13.2)

# Имя библиотеки.
project (CHIPTUNE)

# Производится проверка на то, что параметры оптимизации
# проекта (библиотеки) заданы.
# Они должны быть заданы в корневом проекте.
# Пример:
# SET (CHIPTUNE_OPTIMIZATION_FLAGS "-O0;-g3;")
if (NOT DEFINED ${PROJECT_NAME}_OPTIMIZATION_FLAGS)
    message (FATAL_ERROR
             "The value of the variable <<${PROJECT_NAME}_OPTIMIZATION_FLAGS>> is not specified!")
endif ()

# Производится проверка на то, что параметры аппаратной
# части для компилляции проекта (библиотеки) заданы.
# Они должны быть заданы в корневом проекте.
# Параметры должны быть обязательно разделены через <<;>>.
# Пример:
# SET (CHIPTUNE_COMPILER_FLAGS
#      "${MICROCONTROLLER_HARDWARE_FLAGS};${CPP_COMPILER_FLAGS};")
if (NOT DEFINED ${PROJECT_NAME}_COMPILER_FLAGS)
    message (FATAL_ERROR
             "The value of the variable <<${PROJECT_NAME}_COMPILER_FLAGS>> is not specified!")
endif ()

# Для сборки проекта CHIPTUNE требуется обязательно указать путь
# до папки с файлом конфигурации библиотеки CHIPTUNE в проекте
# пользователя (в корневом CMakeList).
# Пример:
# SET (CHIPTUNE_CFG_H_PATH ${CMAKE_SOURCE_DIR}/cfg)
if (NOT DEFINED ${PROJECT_NAME}_CFG_H_PATH)
    message (FATAL_ERROR
            "The value of the variable <<${PROJECT_NAME}_CFG_H_PATH>> is not specified!")
endif()

# Путь до submodule с описанием интерфейсов должен быть указан обязательно.
# Пример:
# SET (MODULE_MC_HARDWARE_INTERFACES_PATH ${CMAKE_SOURCE_DIR}/bsp/submodule/module_mc_hardware_interfaces)
if (NOT DEFINED MODULE_MC_HARDWARE_INTERFACES_PATH)
    message (FATAL_ERROR
            "The value of the variable <<MODULE_MC_HARDWARE_INTERFACES_PATH>> is not specified!")
endif()

# Путь до файла user_os.h с прослойками под конкретную операционную систему.
# Должен быть обязательно указан.
if (NOT DEFINED USER_OS_H_PATH)
    message (FATAL_ERROR
            "The value of the variable <<USER_OS_H_PATH>> is not specified!")
endif()

# Путь к заголовочным файлам операционной системы.
if (NOT DEFINED USER_OS_PATH)
    message (FATAL_ERROR
            "The value of the variable <<USER_OS_PATH>> is not specified!")
endif()

# Путь до сдвиговых регистров.
if (NOT DEFINED SHIFT_REGISTER_BASE_PATH)
    message (FATAL_ERROR
            "The value of the variable <<SHIFT_REGISTER_BASE_PATH>> is not specified!")
endif()

# Для сборки требуется:
include_directories(
    ${${PROJECT_NAME}_CFG_H_PATH} # Путь до файла с разрешающим компиляцию данного файла объявлением.
    ${MODULE_MC_HARDWARE_INTERFACES_PATH}
    ${USER_OS_H_PATH} # Путь до файла user_os с прослойками под конкретную операционную систему.
    ${USER_OS_PATH} # Путь к заголовочным файлам операционной системы.
    ${SHIFT_REGISTER_BASE_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/ay_ym_file_play/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/ay_ym_low_lavel/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/ay_ym_note_mode/inc
)

# Компиляции полежат все <<.cpp>> файлы из директории src.
file (GLOB ${PROJECT_NAME}_SOURCES
      "${CMAKE_CURRENT_SOURCE_DIR}/*/src/*.cpp")

# Собираем библиотеку из .obj файлов, полученных из файлов, добавленных выше.
add_library (${PROJECT_NAME} STATIC ${${PROJECT_NAME}_SOURCES})

# Компиляция файлов происходит по флагам из корневого CMakeList.
target_compile_options (${PROJECT_NAME} PRIVATE
                        "${${PROJECT_NAME}_COMPILER_FLAGS}${${PROJECT_NAME}_OPTIMIZATION_FLAGS}")

# Показ флагов компиляции.
add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
                    COMMENT
                    "${PROJECT_NAME} flags: ${${PROJECT_NAME}_COMPILER_FLAGS}${${PROJECT_NAME}_OPTIMIZATION_FLAGS}")

# Вывод коммита.
add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
                    COMMENT "${PROJECT_NAME} commit:"
                    COMMAND git rev-parse HEAD)

# Вывод размера библиотеки.
add_custom_command (TARGET ${PROJECT_NAME} POST_BUILD
                    DEPENDS ${PROJECT_NAME}.a
                    COMMENT "${PROJECT_NAME} size information:"
                    COMMAND cd ${PROJECT_BINARY_DIR}
                    COMMAND ${ARM_SIZE} lib${PROJECT_NAME}.a -t)