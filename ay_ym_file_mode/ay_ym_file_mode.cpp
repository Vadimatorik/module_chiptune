#include "ay_ym_file_mode.h"

ay_ym_file_mode::ay_ym_file_mode ( ay_ym_file_mode_struct_cfg_t* cfg ) : cfg( cfg ) {}

void ay_ym_file_mode::clear_chip ( uint8_t chip_number ) {
    ay_queue_struct buf;
    buf.number_chip         = chip_number;

    // Отключаем все каналы и шумы.
    buf.reg                 = 7;
    buf.data                = 0b111111;

    this->cfg->ay_hardware->queue_add_element( &buf );

    for (int l = 0; l<7; l++) {        // Очищаем первые 7 регистров.
        buf.reg = l;
        this->cfg->ay_hardware->queue_add_element( &buf );
    }
    for (int l = 8; l<16; l++) {        // Остальные.
        buf.reg = l;
        this->cfg->ay_hardware->queue_add_element( &buf );
    }
}



// Останавливаем трек и чистим буфер
// (завершает метод psg_file_play из другого потока).
void ay_ym_file_mode::psg_file_stop ( void ) {
    this->emergency_team = 1;
}

// Ждем, пока все данные из очереди будут переданы.
void ay_ym_file_mode::ay_delay_ay_low_queue_clean ( void ) {
    while( this->cfg->ay_hardware->queue_empty_check() != true ) {           // Ждем, пока AY освободится.
        USER_OS_DELAY_MS(20);
    }
}


// Открываем файл с выбранным именем и воспроизводим его.
EC_AY_FILE_MODE ay_ym_file_mode::psg_file_play ( char* dir_path, uint32_t psg_file_number ) {
    if ( psg_file_number > this->file_count ) {
        return EC_AY_FILE_MODE::ARG_ERROR;
    }

    // Достаем имя файла и его длину из файла-списка.
    EC_AY_FILE_MODE     func_res;
    char                name[256];
    uint32_t            len_i;
    FRESULT             r;
    func_res = this->psg_file_get_name( dir_path, psg_file_number, name, len_i );
    if ( func_res != EC_AY_FILE_MODE::OK )
        return func_res;

    // Открываем наш psg файл.
    DIR     d;
    r = f_opendir( &d, dir_path );
    if ( r != FR_OK )
        return EC_AY_FILE_MODE::OPEN_DIR_ERROR;


    FIL     file;
    r = f_open( &file, name, FA_OPEN_EXISTING | FA_READ );
    if ( r != FR_OK ) {
        return EC_AY_FILE_MODE::OPEN_FILE_ERROR;
    }

    // Если мы тут, то мы достали название + длину файла из списка, успешно зашли в папку с файлом, открыли его.
    this->cfg->ay_hardware->play_state_set( 1 );
    this->clear_chip( 0 );                              // Обязательно стираем настройки старой мелодии. Чтобы звук по началу не был говном.

    ay_queue_struct     bq = { 0, 0, 0 };               // Буффер для одного элемента очереди.

    bool                flag = false;                   // Чтобы различать, что мы считали. Регистр (0) - или значение (1). Сначала - регистр.
    volatile uint32_t   p = 16;                         // Номер элемента в буффере, из которого мы будем выдавать данные.
                                                        // Начинаем с 16-го байта, т.к. до него у нас заголовок.

    uint32_t file_size = f_size( &file );

    // Вытягиваем первый блок данных.
    uint8_t b[512];
    UINT    l;
    r =  f_read( &file, b, 512, &l );        // l не проверяем потом, т.к. анализ массива все равно производится на основе длины файла.
    if ( r != FR_OK )
        return EC_AY_FILE_MODE::READ_FILE_ERROR;

    // Проверка наличия стартового байта.
    if ( ( b[16] == 0xfe ) | ( b[16] == 0xff ) ) {
        p = 17;
    } else {
        p = 16;
    }

    for ( uint32_t l_p = p; l_p < file_size; l_p++, p++ ) {
        if ( this->emergency_team != 0 ) {            // Если пришла какая-то срочная команда!
            if ( this->emergency_team == 1 ) {        // Если нужно остановить воспроизведение.
                this->emergency_team = 0;             // Мы приняли задачу.
                this->cfg->ay_hardware->full_clear(); // Очищаем AY, очереди. Потом отключаем его.
                return EC_AY_FILE_MODE::OK;
            }
        };
        if ( p == 512 ) {
            r =  f_read( &file, b, 512, &l );
            if ( r != FR_OK )
                return EC_AY_FILE_MODE::READ_FILE_ERROR;
            p = 0;
        }

        if ( flag == false ) {
            if ( b[p] == 0xFF ) {                                        // 0xFF - простая задержка на ~20 мс. Очередь сама разберется, как с ней быть.
                bq.reg = 0xFF;
                this->cfg->ay_hardware->queue_add_element( &bq );
            } else {
                bq.reg = b[p];                                           // Регистр мы просто записываем. Но не отправляем в очередь.
                flag = true;
            }
        } else {
            bq.data = b[p];                                              // Теперь, когда у нас есть актуальное значение регистра и данных в него,                                      // кидаем пачку в очередь.
            this->cfg->ay_hardware->queue_add_element( &bq );
            flag = false;
        };

    };

    this->ay_delay_ay_low_queue_clean();                                // Ждем, пока все данные в AY передадутся.
    this->cfg->ay_hardware->full_clear();                               // Очищаем AY, очереди. Потом отключаем его.
    return EC_AY_FILE_MODE::OK;
}


/*
 * Получаем длину файла PSG в "колличестве 0xFF". По сути, 1 0xFF = 20 мс.
 * Т.к. между ними данные передаются практически мгновенно.
 * Мы должны заранее находится в нужной директории.
 * Передаем указатель на строку-имя файла.
 * Можно напрямую из file_info. Чтобы не тратить время на копирование.
 * ВАЖНО!: метод не следит за mutex-ом MicroSD! => вызываеться данный метод может только из под другого метода,
 * который разрулит все проблемы.
 * ВАЖНО!: Т.к. метод дочерний, то указатель на буффер ему тоже нужно передать. Причем там должно быть 512 байт.
 * Как вариант - на момент создания списка - использовать кольцевой буффер.
 */
EC_AY_FILE_MODE ay_ym_file_mode::psg_file_get_long ( char* name, uint32_t& result_long ) {
    FIL         file_psg;

    // Если открыть не удалось - значит либо файла не сущетсвует, либо еще чего.
    if ( f_open( &file_psg, name, FA_OPEN_EXISTING | FA_READ ) != FR_OK )
        return EC_AY_FILE_MODE::OPEN_FILE_ERROR;

                result_long     = 0;
    uint8_t     flag_one_read   = 0;     // Флаг первого чтения. Чтобы сразу перескачить заголовок.
    UINT        l               = 0;     // Ею будем отслеживать опустошение буффера.
                                         // К тому же, в ней после считывания хранится число реально скопированныйх байт.
    uint16_t    p               = 16;    // Номер элемента в буффере, с которого идет анализ.

    uint32_t    file_size = f_size( &file_psg );                // Полный размер файла (всего).

    if ( file_size < 16 ) {                                     // Если помимо заголовка ничего нет - выходим.
        f_close( &file_psg );
        return EC_AY_FILE_MODE::OPEN_FILE_ERROR;
    }

    uint8_t b[512];                                             // Буффер на 512 элементов.
    // Начинаем с 16-го байта (счет с 0), т.к. первые 16 - заголовок.
    for ( uint32_t loop_byte_file = 16; loop_byte_file < file_size; loop_byte_file++ ) {
        if ( l == 0 ) {                                         // Если байты закончались - считываем еще 512.
            if ( f_read( &file_psg, b, 512, &l ) != FR_OK ) {
                f_close( &file_psg );
                return EC_AY_FILE_MODE::READ_FILE_ERROR;
            };
            if ( flag_one_read != 0 ) {                         // Если чтение не первое.
                p = 0;
            } else {                                            // В случае первого чтения.
                p = 16;                                         // Пропускаем заголовок, по-этому, сразу отнимаем.
                l-=16;
                flag_one_read = 1;
            }
        }
        if ( b[p] == 0xFF ) {
            result_long++;                                      // Если нашли 0xFF - то это пауза. => 20 мс.
        }
        p++;
        l--;
    };

    f_close( &file_psg );                                       // Закрываем файл.
    return EC_AY_FILE_MODE::OK;
}

// Сканируе дерикторию на наличие psg файлов (возвращаем колличетсов ВАЛИДНЫХ файлов).
EC_AY_FILE_MODE ay_ym_file_mode::find_psg_file ( char* dir_path ) {
    EC_AY_FILE_MODE     func_res        = EC_AY_FILE_MODE::OK;
    FRESULT             r;
    FIL                 file_list;

    // Создаем файл-список psg файлов.
    // В него будем записывать построчно названия файлов, которые пройдет проверку.
    if ( this->cfg->microsd_mutex != nullptr )
        USER_OS_TAKE_MUTEX( *this->cfg->microsd_mutex, portMAX_DELAY );    // sdcard занята нами.

    DIR     d;
    FILINFO fi;

    // Начинаем поиск файлов в переданной параметром директории.
    r = f_findfirst( &d, &fi, dir_path, "*.psg" );

    // Если есть хоть 1 подходящий файл в директории - создаем файл-список.
    if ( r == FR_OK ) {
        r = f_open( &file_list, "psg_list.txt", FA_CREATE_ALWAYS | FA_READ | FA_WRITE );
        if ( r != FR_OK ) {
            f_close( &file_list );
            f_closedir( &d );
            if ( this->cfg->microsd_mutex != nullptr )
                USER_OS_GIVE_MUTEX( *this->cfg->microsd_mutex );    // sdcard свободна.
            return EC_AY_FILE_MODE::OPEN_FILE_ERROR;
        }
    }

    uint32_t valid_file_count = 0;
    while ( ( r == FR_OK ) && ( fi.fname[0] != 0 ) ) {                  // Если psg был найден.
        uint32_t len;
        EC_AY_FILE_MODE r_psg_get;
        r_psg_get = this->psg_file_get_long( fi.fname, len );           // Проверяем валидность файла.
        if ( r_psg_get != EC_AY_FILE_MODE::OK ) continue;               // Если файл бракованный - выходим.
        // Файл рабочий.
        valid_file_count++;
        // Для каждого удачного файла - сохранение на 512 байт.


        char b[512] = {0};

        // Имя может быть длинным или коротким.
        if ( fi.fname[0] == 0 ) {
            memcpy( b, fi.altname, 13 );                                // 256 первых - строка имени (255 максимум символов UTF-8) + 0.
        } else {
            memcpy( b, fi.fname, 256 );
        }

        memcpy( &b[256], &len, 4 );                                     // Далее 4 байта uint32_t - время.
        UINT l;                                                         // Количество записанных байт (должно быть 512).
        r = f_write( &file_list, b, 512, &l );
        if ( r != FR_OK ) {
            func_res = EC_AY_FILE_MODE::OPEN_FILE_ERROR;
            break;
        }

        if ( l != 512 ) {                                               // Если запись не прошла - аварийный выход.
            func_res = EC_AY_FILE_MODE::WRITE_FILE_ERROR;
            break;
        }
        // Ищем следующий файл.
        r = f_findnext( &d, &fi );
    }

    // Если не удалось связаться с картой, то выходим без закрытия.
    if ( r == FR_OK ) {
        f_close( &file_list );
        f_closedir( &d );
    }

    if ( this->cfg->microsd_mutex != nullptr )
        USER_OS_GIVE_MUTEX( *this->cfg->microsd_mutex );    // sdcard свободна.

    this->file_count = valid_file_count;

    return func_res;
}

// Получаем имя файла и его длительность по его номеру из составленного списка.
EC_AY_FILE_MODE ay_ym_file_mode::psg_file_get_name ( char* dir_path, uint32_t psg_file_number, char* name, uint32_t& time ) {
    FRESULT r;
    EC_AY_FILE_MODE func_res = EC_AY_FILE_MODE::OK;
    FIL     file_list;
    DIR     d;

    if ( this->cfg->microsd_mutex != nullptr )
        USER_OS_TAKE_MUTEX( *this->cfg->microsd_mutex, portMAX_DELAY );     // sdcard занята нами.

    do {
        r = f_opendir( &d, dir_path );
        if ( r != FR_OK ) {
            func_res = EC_AY_FILE_MODE::OPEN_DIR_ERROR;
            break;
        }

        r = f_open( &file_list, "psg_list.txt", FA_OPEN_EXISTING | FA_READ );
        if ( r != FR_OK ) {
            func_res = EC_AY_FILE_MODE::OPEN_FILE_ERROR;
            break;
        }

        if ( psg_file_number > this->file_count ) {
            func_res = EC_AY_FILE_MODE::ARG_ERROR;
            break;
        }

        psg_file_number *= 512;                                             // Вычисляем начало сектора с информацией о нашем файле.

        r = f_lseek( &file_list, psg_file_number );                         // Перемещаемся к нужному месту.
        if ( r != FR_OK ) {
            func_res = EC_AY_FILE_MODE::READ_FILE_ERROR;
            break;
        }

        uint8_t b[512];
        UINT l;
        r =  f_read( &file_list, b, 512, &l );
        if ( ( r != FR_OK ) | ( l != 512 ) ) {
            func_res = EC_AY_FILE_MODE::READ_FILE_ERROR;
            break;
        }

        memcpy( name, b, 256 );
        memcpy( &time, &b[256], 4 );
    } while ( false );

    if ( r == FR_OK ) {                                                     // Защита от вытащенной карты.
        f_close( &file_list );
        f_closedir( &d );
    }

    if ( this->cfg->microsd_mutex != nullptr )
        USER_OS_GIVE_MUTEX( *this->cfg->microsd_mutex );    // sdcard свободна.

    return func_res;
}
