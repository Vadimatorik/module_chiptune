#include "ay_ym_file_mode.h"

ay_ym_file_mode::ay_ym_file_mode ( ay_ym_file_mode_struct_cfg_t* cfg ) : cfg( cfg ) {}

void ay_ym_file_mode::clear_chip ( uint8_t chip_number ) {
    ay_queue_struct buf;
    buf.number_chip         = chip_number;

    // Отключаем все каналы и шумы.
    buf.reg                 = 7;
    buf.data                = 0b111111;

    this->cfg->ay_hardware->queue_add_element( &buf );

    buf.data                = 0;
    for ( uint32_t l = 0; l<7; l++ ) {          // Очищаем первые 7 регистров.
        buf.reg = l;
        this->cfg->ay_hardware->queue_add_element( &buf );
    }

    for ( uint32_t l = 8; l < 16; l++ ) {       // Остальные.
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
EC_AY_FILE_MODE ay_ym_file_mode::psg_file_play ( char* full_name_file, uint8_t number_chip ) {
    FRESULT             r;
    FIL                 file;
    r = f_open( &file, full_name_file, FA_OPEN_EXISTING | FA_READ );
    if ( r != FR_OK ) {
        return EC_AY_FILE_MODE::OPEN_FILE_ERROR;
    }
    this->cfg->pwr_chip_on( number_chip, true );        // Включаем питание.

    // Если мы тут, то мы достали название + длину файла из списка, успешно зашли в папку с файлом, открыли его.
    this->cfg->ay_hardware->play_state_set( 1 );
    this->cfg->ay_hardware->full_clear();
    this->clear_chip( number_chip );                    // Обязательно стираем настройки старой мелодии. Чтобы звук по началу не был говном.

    ay_queue_struct     bq = { number_chip, 0, 0 };     // Буффер для одного элемента очереди.

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

    uint32_t l_p = p;
    bool flag_fe    = false;                          // Выставляется, если у нас был FE.
    while ( l_p < file_size ) {
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

        // Проверка на флаг 0xFE.
        // Байт, следующий за 0FEh, помноженный на 4 даст количество
        // прерываний, в течении которых не было вывода на сопроцессор.
        if ( flag_fe == true ) {
            bq.reg = 0xFF;
            for ( uint32_t loop_pause_interrupt = b[p] * 4; loop_pause_interrupt != 0; loop_pause_interrupt-- )
                this->cfg->ay_hardware->queue_add_element( &bq );
            flag_fe = false;
            l_p++,
            p++;
            continue;
        }

        if ( flag == false ) {
            switch ( b[p] ) {

            // 0xFF - простая задержка на ~20 мс. Очередь сама разберется, как с ней быть.
            case 0xFF:  bq.reg = 0xFF;
                        this->cfg->ay_hardware->queue_add_element( &bq );
                        break;

            case 0xFE:  flag_fe = true;
                        break;

            default:    bq.reg = b[p];                                           // Регистр мы просто записываем. Но не отправляем в очередь.
                        flag = true;
                        break;
            }
        } else {
            // Т.к. этот метод рассчитан только на 1 какой-то чип, то мы принимаем данные только для 0..15 регистра.
            if ( bq.reg < 16 ) {
                bq.data = b[p];                                          // Теперь, когда у нас есть актуальное значение регистра и данных в него,                                      // кидаем пачку в очередь.
            }
            this->cfg->ay_hardware->queue_add_element( &bq );
            flag = false;
        };

         l_p++,
         p++;
    };

    this->ay_delay_ay_low_queue_clean();                                // Ждем, пока все данные в AY передадутся.
    this->cfg->ay_hardware->full_clear();                               // Очищаем AY, очереди.
    this->cfg->ay_hardware->play_state_set( 0 );                        // Потом отключаем усилок и чипы.
    this->cfg->pwr_chip_on( number_chip, false );                       // Конкретный чип для галочки тоже.

    return EC_AY_FILE_MODE::OK;
}


//**********************************************************************
// Получаем длину файла PSG в "колличестве 0xFF". По сути, 1 0xFF = 20 мс.
// Т.к. между ними данные передаются практически мгновенно.
// Мы должны заранее находится в нужной директории.
// Передаем указатель на строку-имя файла.
// Можно напрямую из file_info. Чтобы не тратить время на копирование.
// ВАЖНО!: метод не следит за mutex-ом MicroSD! => вызываеться данный метод может только из под другого метода,
// который разрулит все проблемы.
// ВАЖНО!: Т.к. метод дочерний, то указатель на буффер ему тоже нужно передать. Причем там должно быть 512 байт.
// Как вариант - на момент создания списка - использовать кольцевой буффер.
//**********************************************************************
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
