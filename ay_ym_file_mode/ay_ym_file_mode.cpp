#include "ay_ym_file_mode.h"

ay_ym_file_mode::ay_ym_file_mode ( ay_ym_file_mode_struct_cfg_t* cfg ) : cfg( cfg ) {
    // Для отдачи команд задачи обновления кольцевого буффера.
    this->queue_update        = USER_OS_STATIC_QUEUE_CREATE( 1, sizeof( uint8_t ), this->queue_update_buf, &this->queue_update_st );
    // Задача будет ставить симафор всякий раз, как часть кольцевого буффера была обнавлена.
    this->c_buf_semaphore     = USER_OS_STATIC_BIN_SEMAPHORE_CREATE( &this->c_buf_semaphore_buf );
    // Задача обновления кольцевого буффера.
    USER_OS_STATIC_TASK_CREATE( this->buf_update_task, "ay_file", 300, ( void* )this, this->cfg->circular_buffer_task_prio, this->task_stack, &this->task_struct );
}

// Открываем карту и копируем нужный кусок
// ( 1 кусок - number_sector * 512 байт, счет с 0 ).
EC_AY_FILE_MODE ay_ym_file_mode::psg_part_copy_from_sd_to_array ( uint32_t sektor, uint16_t point_buffer, uint8_t number_sector, UINT *l ) {
    (void)sektor;(void)point_buffer;(void)number_sector;(void)l;
  /*  USER_OS_TAKE_MUTEX( *this->cfg->microsd_mutex, portMAX_DELAY );         // Ждем, пока освободится microsd.
    if ( f_lseek( &this->file, sektor * 512 ) == FR_OK) {                    // Переходим к сектору в файле.
        // Читаем в кольцевой буфер кусок.
        if ( f_read( &this->file, &this->cfg->p_circular_buffer[ point_buffer ], 512 * number_sector, l) == FR_OK ) {
            USER_OS_GIVE_MUTEX( *this->cfg->microsd_mutex );                // Показываем, что карта нам теперь не нужна.
            return AY_FILE_MODE::OK;
        };
    };*/
    return EC_AY_FILE_MODE::READ_FILE_ERROR;
}

/*
 * При появлении в очереди элемента - обновляем данные.
 * Имя директории и файла кладется в ay_file_mode_cfg_t заранее.
 * Файл должен быть открыт заранее и закрыт после чтения последнего блока.
 * Предполагается, что все готова для чтения.
 */
void ay_ym_file_mode::buf_update_task ( void* p_obj ) {
    ay_ym_file_mode* o = ( ay_ym_file_mode* )p_obj;
    while ( true ) {
        uint8_t buffer_queue;
        uint8_t offset = o->cfg->circular_buffer_size / 512;                    // Сколько секторов за 1 раз следует считать. Зависит от буффера.
        while ( true ) {
            USER_OS_QUEUE_RECEIVE( o->queue_update, &buffer_queue, portMAX_DELAY );
            if ( buffer_queue == 0 ) {                                            // Решаем, какую часть кольцевого буфера сейчас перезаполняем.
                if ( o->psg_part_copy_from_sd_to_array( o->sektor, 0,                            offset, &o->l) != EC_AY_FILE_MODE::OK )
                    break;    // Если не считалось (проблемы с картой или еще что - выходим с ошибкой. Все манипуляции с картой внутри метода.
            } else {        // Пишем с середины кольцевого буфера.
                if ( o->psg_part_copy_from_sd_to_array( o->sektor, o->cfg->circular_buffer_size, offset, &o->l) != EC_AY_FILE_MODE::OK )
                    break;
            }
            o->sektor += offset;                                                // В следущий раз - другой блок.
            USER_OS_GIVE_BIN_SEMAPHORE( o->c_buf_semaphore );                    // Буффером можно пользоваться.
        }
    }
}

// Очищаем AY через очередь.
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



// Останавливаем трек и чистим буффера.
void ay_ym_file_mode::psg_file_stop ( void ) {
    this->emergency_team = 1; // ay_psg_file_play_from_microSD сканирует эту переменную.
}

// Открываем файл с выбранным именем и воспроизводим его.
EC_AY_FILE_MODE ay_ym_file_mode::psg_file_play ( void ) {
  /*  ay_queue_struct     buffer_queue = { 0, 0, 0 };     // Буффер для элемента, который положем в очередь.


    uint8_t             flag = 0;                       // Чтобы различать, что мы считали. Регистр (0) - или значение (1). Сначала - регистр.
    uint16_t            p_data_buf = 16;                // А это номер элемента в буффере, из которого мы будем выдавать данные. Начинаем с 16-го байта, т.к. до него у нас заголовок.
    uint8_t             cmd_buffer = 0;                 // Сюда будем класть указание, в какую часть буффера класть данные.
    this->sektor = 0;                                   // Читаем с начала.
    this->emergency_team = 0;                           // На случай, если тыкнули на остановку до воспроизведения до этого.
    this->cfg->ay_hardware->play_set_state( 1 );
    this->clear_chip( 0 );                              // Обязательно стираем настройки старой мелодии. Чтобы звук по началу не был говном.

    // Если открыть не удалось - значит либо файла не сущетсвует, либо еще чего. Но мы возвращаем, что файл поврежден.
    if ( f_open( &this->file, this->file_name, FA_OPEN_EXISTING | FA_READ ) != FR_OK ) {        // Открываем файл, из которого будем читать.
        return AY_FILE_MODE::OPEN_FILE_ERROR;
    };

    // Заполняем кольцевой буффер.
    USER_OS_TAKE_BIN_SEMAPHORE( this->c_buf_semaphore, 0 );    // Буффер может быть заполнен другим файлом.
    cmd_buffer = 0;
    USER_OS_QUEUE_SEND( this->queue_update_buf, &cmd_buffer, portMAX_DELAY  );               // Просим задачу в другом потоке заполнить буфер.
    USER_OS_TAKE_BIN_SEMAPHORE( this->c_buf_semaphore, portMAX_DELAY );                     // Ждем, пока она это сделает.
    cmd_buffer = 1;                                                                          // Так же и со второй частью кольцевого буфера.
    USER_OS_QUEUE_SEND( this->queue_update_buf, &cmd_buffer, portMAX_DELAY  );

    uint32_t file_size = f_size( &this->file );

    for ( uint32_t loop_byte_file = 16; loop_byte_file < file_size; loop_byte_file++, p_data_buf++ ) {
        if ( this->emergency_team != 0 ) {            // Если пришла какая-то срочная команда!
            if ( this->emergency_team == 1 ) {        // Если нужно остановить воспроизведение.
                this->emergency_team = 0;             // Мы приняли задачу.
                cmd_buffer = (uint8_t)AY_FILE_MODE::END_TRACK;
                USER_OS_QUEUE_SEND( *this->cfg->queue_feedback, &cmd_buffer, portMAX_DELAY  ); // Сообщаем, что трек закончен.
                return AY_FILE_MODE::OK;                            // Выключаем AY, выдаем в очередь флаг окончания и выходим.
            }
        };
        // Смотрим, не закончилась ли часть буффера.
        if ( p_data_buf == this->cfg->circular_buffer_size ) {
            USER_OS_TAKE_BIN_SEMAPHORE ( this->c_buf_semaphore, portMAX_DELAY );    // К этому времени у нас уже должена была перезаписаться часть буффера.
            cmd_buffer = 0;
            USER_OS_QUEUE_SEND( this->queue_update_buf, ( void * ) &cmd_buffer, portMAX_DELAY  );    // Приказываем перезаписать часть, которую уже выдали.
        };
        if (p_data_buf == this->cfg->circular_buffer_size) {
            USER_OS_TAKE_BIN_SEMAPHORE ( this->c_buf_semaphore, portMAX_DELAY );    // К этому времени у нас уже должена была перезаписаться часть буффера.
            cmd_buffer = 1;
            USER_OS_QUEUE_SEND( this->queue_update_buf, ( void * ) &cmd_buffer, portMAX_DELAY  );    // Приказываем перезаписать часть, которую уже выдали.
            p_data_buf = 0;
        };
        if ( this->cfg->p_circular_buffer[p_data_buf] == 0xFF ) {    // 0xFF - простая задержка на ~20 мс. Очередь сама разберется, как с ней быть.
            buffer_queue.reg = 0xFF;
            this->cfg->ay_hardware->queue_add_element( &buffer_queue );
        } else {
            if (flag == 0) {
                buffer_queue.reg = this->cfg->p_circular_buffer[p_data_buf];                      // Регистр мы просто записываем. Но не отправляем в очередь.
                flag = 1;
            } else {
                buffer_queue.data = this->cfg->p_circular_buffer[p_data_buf];                     // Теперь, когда у нас есть актуальное значение регистра и данных в него, кидаем пачку в очередь.
                this->cfg->ay_hardware->queue_add_element( &buffer_queue );
                flag = 0;
            };
        };
    };
    if ( f_close( &this->file ) != FR_OK ) {                    // Все прочитано, закрываем файл.
        return AY_FILE_MODE::OPEN_FILE_ERROR;                   // Обработка исключительной ситуации.
    };
    this->cfg->ay_hardware->play_set_state( 1 );
   // ay_delay_clean(*this->cfg->fd_ay_hardware);    // Ждем, пока все данные в AY передадутся.
    this->cfg->ay_hardware->play_set_state( 0 );

    cmd_buffer = (uint8_t)AY_FILE_MODE::END_TRACK;
    USER_OS_QUEUE_SEND( *this->cfg->queue_feedback, &cmd_buffer, portMAX_DELAY  ); // Сообщаем, что трек закончен.*/
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
EC_AY_FILE_MODE ay_ym_file_mode::find_psg_file ( void ) {
    FRESULT r;
    FIL     file_list;

    // Создаем файл-список psg файлов.
    // В него будем записывать построчно названия файлов, которые пройдет проверку.
    r = f_open( &file_list, "psg_list.txt", FA_CREATE_ALWAYS | FA_READ | FA_WRITE );
    if ( r != FR_OK ) {
        USER_OS_GIVE_BIN_SEMAPHORE(*this->cfg->microsd_mutex);    // sdcard свободна.
        return EC_AY_FILE_MODE::OPEN_FILE_ERROR;
    }

    DIR d;
    FILINFO fi;

    r = f_findfirst( &d, &fi, "", "*.psg" );                            // Начинаем поиск файлов в заранее выбранной директории.

    uint32_t valid_file_count = 0;
    while ( ( r == FR_OK ) && ( fi.fname[0] != 0 ) ) {                  // Если psg был найден.
        uint32_t len;
        EC_AY_FILE_MODE r_psg_get;
        r_psg_get = this->psg_file_get_long( fi.fname, len );           // Проверяем валидность файла.
        if ( r_psg_get != EC_AY_FILE_MODE::OK ) continue;                  // Если файл бракованный - выходим.
        // Файл рабочий.
        valid_file_count++;
        f_printf( &file_list, "%s\n%u\n", fi.fname, len );
        r = f_findnext( &d, &fi );                                      // Ищем следующий файл.
    }

    f_close( &file_list );
    f_closedir( &d );

    this->file_count = valid_file_count;

    return EC_AY_FILE_MODE::OK;
}

// Получаем имя файла и его длительность по его номеру из отсортированного списка.
EC_AY_FILE_MODE ay_ym_file_mode::psg_file_get_name ( uint32_t psg_file_number, char* buf_name, uint32_t& time ) {
    (void)psg_file_number;(void)buf_name;(void)time;
    /*
    UINT l = 0;                        // Ею будем отслеживать опустошение буффера. К тому же, в ней после считывания хранится число реально скопированныйх байт.
    USER_OS_TAKE_BIN_SEMAPHORE( *this->cfg->microsd_mutex, portMAX_DELAY ); // Ждем, пока освободится microsd.

    volatile int res;
    // Если карта не открылась нормально - отдаем мутекс и выходим.
    res = f_open(&this->file, "psg_list.list", FA_OPEN_EXISTING | FA_READ );
    if (res != FR_OK) {
            USER_OS_GIVE_BIN_SEMAPHORE(*this->cfg->microsd_mutex);
            return AY_FILE_MODE::OPEN_FILE_ERROR;
    };

    if (f_lseek(&this->file, psg_file_number*(256+4)) == FR_OK) {    // Переходим к нужному файлу.
        if (f_read(&this->file, buf_name, 256, &l) != FR_OK) {    // Читаем строку.
            return AY_FILE_MODE::OPEN_READ_DIR_ERROR;
        };
        if (f_read(&this->file, &time, 4, &l) != FR_OK) {            // Читаем время.
            return AY_FILE_MODE::OPEN_READ_DIR_ERROR;
        };
    };

    f_close(&this->file);            // Закрываем файл.

    USER_OS_GIVE_BIN_SEMAPHORE(*this->cfg->microsd_mutex);*/
    return EC_AY_FILE_MODE::OK;
}
/*
// Моя реализация интеллектуальной сортировки без учета регистра.
int _intelligent_sorting (char *string1, char *string2) {
    char char_1, char_2;        // Буфферы для сравнения.
    int result = 0;                // -1 - 0-я меньше; 0 - равны; 1 - s2 больше.
    for (int loop_char = 0; loop_char < 256; loop_char++) { // Проходимся по всем символам строки.
        if ((uint8_t)string1[loop_char] == 0) { result = -1; return result;};
        if ((uint8_t)string2[loop_char] == 0) { result = 1; return result;};
        if (((uint8_t)string1[loop_char] >= (uint8_t)'A') && ((uint8_t)string1[loop_char] <= (uint8_t)'Z')) {    // Если буква большая.
            char_1 = (char)((uint8_t)string1[loop_char] + (uint8_t)('a'-'A'));        // Делаем из большей малую.
        } else char_1 = string1[loop_char];
        if (((uint8_t)string2[loop_char] >= (uint8_t)'A') && ((uint8_t)string2[loop_char] <= (uint8_t)'Z')) {    // Если буква большая.
            char_2 = (char)((uint8_t)string2[loop_char] + (uint8_t)('a'-'A'));        // Делаем из большей малую.
        } else char_2 = string2[loop_char];
        if ((uint8_t)char_1 < (uint8_t)char_2) {
            result = -1;
            return result;
        } else if ((uint8_t)char_1 > (uint8_t)char_2) {
            result = 1;
            return result;
        };
    };
    return result;
}*/
/*
// Сортируем уже существующий список.
int ay_file_sort (int fd) {
    ay_file_mode_cfg_t *ay = eflib_getInstanceByFd (fd);
    UINT wr_status;
    uint8_t flag;     // Флаг указывает, какое из 2-х имен в буффере мы записали в microsd и его можно заменить новым.
    FIL this->file;                // Для записи в list_psg.
    if (f_open(&this->file, "psg_list.list", FA_OPEN_EXISTING | FA_READ | FA_WRITE) != FR_OK) {
        return OPEN_FILE_ERROR;
    };

    for (uint32_t loop = 0; loop<o->dir_number_file; loop++) { // Проходимся по n файлам n-1 раз.
        if (f_lseek(&this->file, 0) != FR_OK) {return READ_FILE_ERROR;};
        while (f_read(&this->file, o->psg_file_buf, 256 + 4, &wr_status) != FR_OK) {};    // Копируем только имя + время.
        flag = 1;
        for (uint32_t file_loop = 1; file_loop < o->dir_number_file; file_loop++) { //Считываем n-1 файлов в пары. После каждой итерации 1 файл записывается заменяя старый.
            if (f_lseek(&this->file, file_loop*(256+4)) != FR_OK) {return READ_FILE_ERROR;};
            if (f_read(&this->file, &o->psg_file_buf[flag*(256+4)], 256 + 4, &wr_status) != FR_OK) {return READ_FILE_ERROR;};
            if (f_lseek(&this->file, (file_loop-1)*(256 + 4)) != FR_OK) {return READ_FILE_ERROR;};
            int string_result =_intelligent_sorting((char*)o->psg_file_buf, (char*)&o->psg_file_buf[256+4]);    // Между временем и строкой 100% будет пробел. => время не тронут.
            if (string_result < 0) {    // Если [0] строка меньше чем [256+4].
                if (f_write(&this->file, &o->psg_file_buf[0], 256 + 4, &wr_status) != FR_OK) {return READ_FILE_ERROR;};    // Обязательно записываем.
                flag = 0;
            } else {
                if (f_write(&this->file, &o->psg_file_buf[256 + 4], 256 + 4, &wr_status) != FR_OK) {return READ_FILE_ERROR;};
                flag = 1;
            };
        };
    };
    int res = f_close(&this->file);
    xSemaphoreGive(*o->cfg->microsd_mutex);
    if (res != FR_OK) {
        return OPEN_FILE_ERROR;
    }
    return 0;
}
*/
