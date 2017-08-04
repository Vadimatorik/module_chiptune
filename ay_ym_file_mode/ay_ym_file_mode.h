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

    //**********************************************************************
    // Воспроизводим psg файл.
    // Важно! В папке по пути dir_path лолжен существовать файл списка воспроизведения.
    //**********************************************************************
    EC_AY_FILE_MODE        psg_file_play           ( char* dir_path, uint32_t psg_file_number );

    void                   psg_file_stop           ( void );                                           // Останавливакем воспроизведение.
                                                                    // валидных psg файлов.

    // Очищаем чип через очередь.
    void    clear_chip              ( uint8_t chip_number );

private:
    EC_AY_FILE_MODE     psg_part_copy_from_sd_to_array ( uint32_t sektor, uint16_t point_buffer, uint8_t number_sector, UINT *l );

    // Получаем длину файла (если валидный).
    // Файл должен находится в текущей директории.
    EC_AY_FILE_MODE    psg_file_get_long ( char* name, uint32_t& result_long );

    // Количество файлов в текущей директории.
    uint32_t           file_count;

    const ay_ym_file_mode_struct_cfg_t* const cfg;

    bool    emergency_team = false;
};






