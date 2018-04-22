#pragma once

#include "ay_ym_low_lavel.h"            // Для обращения к используемому AY.
#include "ff.h"                         // FatFS от ChaN.
#include "user_os.h"                    // Пользовательская OS.

// Структура первоначальной инициализации AY чипа.
struct ay_ym_file_mode_struct_cfg_t {
    AyYmLowLavel*            ay_hardware;
    USER_OS_STATIC_MUTEX*       microsd_mutex;                 // Для эксклюзивного доступа к microsd (создается пользователем заранее).
                                                               // Может быть nullptr, если картой никто не пользуется.
    void ( *pwr_chip_on )       ( uint32_t chip, bool state ); // chip - номер чипа, который следует включить/выключить. state == true - включить.
    // ВАЖНО! ay_low может полностью обесточивать всю аналоговую часть. Тут же управление конкретными чипами!
};

enum class EC_AY_FILE_MODE_ANSWER {
    OK                      = 0,
    ARG_ERROR,                                             // Ошибка входного аргумента ( например, попросили сыграть 2-й файл, когда в папке всего 1 ).
    WRITE_FILE_ERROR,
    OPEN_FILE_ERROR,
    OPEN_DIR_ERROR,
    OPEN_READ_DIR_ERROR,
    READ_FILE_ERROR,
    FIND_ERROR,
    TRACK_END,                                              // Трек закончился сам успешно.
    TRACK_STOPPED,                                          // Трек остановили насильно.
};

class AyYmFileMode {
public:
    AyYmFileMode ( ay_ym_file_mode_struct_cfg_t* cfg );

    //**********************************************************************
    // Воспроизводим psg файл.
    // Важно! В папке по пути dir_path лолжен существовать файл списка воспроизведения.
    //**********************************************************************
    EC_AY_FILE_MODE_ANSWER     psg_file_play ( char* full_name_file, uint8_t number_chip );

    // Завершает psg_file_play из другого потока.
    void                        psg_file_stop                   ( void );                                           // Останавливакем воспроизведение.
                                                                    // валидных psg файлов.
    // Получаем длину файла (если валидный).
    // Файл должен находится в текущей директории.
    EC_AY_FILE_MODE_ANSWER     psg_file_get_long               ( char* name, uint32_t& result_long );

private:
    EC_AY_FILE_MODE_ANSWER     psg_part_copy_from_sd_to_array  ( uint32_t sektor, uint16_t point_buffer, uint8_t number_sector, UINT *l );


    // Ждем, пока все данные из очереди ay низкого уровня будут переданы (файл будет воиспроизведен до конца).
    void                ay_delay_ay_low_queue_clean     ( void );

    // Очищаем чип через очередь.
    void                clear_chip                      ( uint8_t chip_number );

    const ay_ym_file_mode_struct_cfg_t* const cfg;

    bool        emergency_team = false;
};






