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
    OPEN_FILE_ERROR         = -1,
    OPEN_DIR_ERROR          = -2,
    OPEN_READ_DIR_ERROR      = -3,
    READ_FILE_ERROR         = -4,
    END_TRACK               = 255
};

class ay_ym_file_mode {
public:
    ay_ym_file_mode ( ay_ym_file_mode_struct_cfg_t* cfg );

    EC_AY_FILE_MODE        psg_file_play           ( void );                                           // Открываем файл с заданным именем в заданной директори (выбирается заранее).
    void                   psg_file_stop           ( void );                                           // Останавливакем воспроизведение.
    EC_AY_FILE_MODE        find_psg_file           ( void );           // Производим поиск psg файлов в заранее выбранной директории и составляем список
                                                                    // валидных psg файлов.
    EC_AY_FILE_MODE        psg_file_get_name       ( uint32_t psg_file_number, char* buf_name, uint32_t& time );    // Получаем имя и длительность файла.

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

    // Для создания задачи обновления кольцевого буфера.
    USER_OS_STATIC_STACK_TYPE           task_stack[300] = { 0 };
    USER_OS_STATIC_TASK_STRUCT_TYPE     task_struct   = USER_OS_STATIC_TASK_STRUCT_INIT_VALUE;

    uint32_t    sektor;                 // Сектор, который следует считать в следущий раз.


    UINT        l;                      // Ею будем отслеживать опустошение буффера. К тому же, в ней после считывания хранится число реально скопированныйх байт.
    uint8_t     emergency_team;         // Неотложная команда! Что нужно выполнить сразу, как только будет установлено значение. Работает почти как очередь.
    // Например, 0 игнорируется всеми. Потом мы поставили воспроизведение файла (с помощью очереди), но нам нужно сейчас остановить воспроизведение полностью (не пауза!).
    // В этом случае мы выставляем 1 и задача, копирующая в AY очередь данные вызовет процедуру очистки AY и вернет в очередь уведомление о том, что воспроизведение остановлено.

};






