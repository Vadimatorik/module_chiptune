#pragma once

#include "ff.h"                         // FatFS от ChaN.
#include "user_os.h"                    // Пользовательская OS.
#include "ay_ym_low_lavel.h"            // Для обращения к используемому AY.

// Структура первоначальной инициализации AY чипа.
struct ay_ym_file_mode_struct_cfg_t {
    ay_ym_low_lavel*            ay_hardware;
    USER_OS_STATIC_MUTEX*       microsd_mutex;                 // Для эксклюзивного доступа к microsd (создается пользователем заранее).
                                                               // Может быть nullptr, если картой никто не пользуется.
    USER_OS_STATIC_QUEUE*       queue_feedback;                // Для того, чтобы уведомить о каком-либо событии какой-либо поток. Например, что произошла остановка плеера.
                                                               // Очередь под uint8_t переменную. Достаточно одного элемента.
    const uint8_t               circular_buffer_task_prio;     // Приоритет задачи для обновления кольцевого буффера.
    uint16_t                    circular_buffer_size;          // Размер половины кольцевого буффера (должен быть кратен 512 байт).
    uint8_t*                    p_circular_buffer;             // Кольцевой буффер. Размер circular_buffer_size * 2. Выделяется пользователем заранее.

};

enum class EC_AY_FILE_MODE {
    OK                      = 0,
    ARG_ERROR               = 1,                              // Ошибка входного аргумента ( например, попросили сыграть 2-й файл, когда в папке всего 1 ).
    WRITE_FILE_ERROR        = 2,
    OPEN_FILE_ERROR         = 3,
    OPEN_DIR_ERROR          = 4,
    OPEN_READ_DIR_ERROR     = 5,
    READ_FILE_ERROR         = 6,
    END_TRACK               = 255
};

class ay_ym_file_mode {
public:
    ay_ym_file_mode ( ay_ym_file_mode_struct_cfg_t* cfg );

    //**********************************************************************
    // Метод:
    // 1. Производит поиск всех файлов с расширением ".psg".
    // 2. Для каждого файла запрашивает его длину
    //    (метод psg_file_get_long).
    // 3. Создает в текущей директории файл psg_list.txt и помещает в него
    //    имена всех корректных psg файлов + время их воспроизведения.
    // Параметры:
    // [IN]  dir_path - строка-путь к файлу в стиле fatfs.
    //**********************************************************************
    EC_AY_FILE_MODE        find_psg_file           ( char* dir_path );

    //**********************************************************************
    // Получаем имя и длительность файла.
    // Параметры:
    // [IN]  dir_path           - строка-путь к файлу в стиле fatfs.
    // [IN]  psg_file_number    - номер файла в списке.
    // [OUT] name               - сюда помещается считанное имя.
    // [OUT] time               - сюда помещается считанное время.
    //**********************************************************************
    EC_AY_FILE_MODE        psg_file_get_name       ( char* dir_path, uint32_t psg_file_number, char* name, uint32_t& time );

    EC_AY_FILE_MODE        psg_file_play           ( uint32_t psg_file_number );     // Открываем файл с заданным именем в заданной директори (выбирается заранее).
    void                   psg_file_stop           ( void );                                           // Останавливакем воспроизведение.
                                                                    // валидных psg файлов.

    // Очищаем чип через очередь.
    void    clear_chip              ( uint8_t chip_number );

    static  void buf_update_task    ( void* p_obj );

private:
    EC_AY_FILE_MODE     psg_part_copy_from_sd_to_array ( uint32_t sektor, uint16_t point_buffer, uint8_t number_sector, UINT *l );

    // Получаем длину файла (если валидный).
    // Файл должен находится в текущей директории.
    EC_AY_FILE_MODE    psg_file_get_long ( char* name, uint32_t& result_long );

    // Количество файлов в текущей директории.
    uint32_t           file_count;

    const ay_ym_file_mode_struct_cfg_t* const cfg;

    // Очередь через которую производится передача команды задаче обновления буфера.
    uint8_t     queue_update_buf[ sizeof( uint8_t ) ] = { 0 };
    USER_OS_STATIC_QUEUE_STRUCT     queue_update_st = USER_OS_STATIC_QUEUE_STRUCT_INIT_VALUE;
    USER_OS_STATIC_QUEUE            queue_update;

    // Симафор показывает, что мы можем пользоваться буффером, т.к. его уже заполнили.
    USER_OS_STATIC_BIN_SEMAPHORE_BUFFER c_buf_semaphore_buf = USER_OS_STATIC_BIN_SEMAPHORE_BUFFER_INIT_VALUE;
    USER_OS_STATIC_BIN_SEMAPHORE        c_buf_semaphore     = nullptr;

    // Задача обработки кольцевого буфера.
    USER_OS_STATIC_STACK_TYPE           task_stack[300] = { 0 };
    USER_OS_STATIC_TASK_STRUCT_TYPE     task_struct   = USER_OS_STATIC_TASK_STRUCT_INIT_VALUE;
};






