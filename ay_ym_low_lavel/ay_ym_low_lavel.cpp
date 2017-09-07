#include "ay_ym_low_lavel.h"

ay_ym_low_lavel::ay_ym_low_lavel ( const ay_ym_low_lavel_cfg_t* const cfg ) : cfg( cfg ) {
    this->semaphore = USER_OS_STATIC_BIN_SEMAPHORE_CREATE( &this->semaphore_buf );
    this->buf_data_chip          = new chip_reg[ this->cfg->ay_number ];
    USER_OS_STATIC_TASK_CREATE( this->task, "ay_low", AY_YM_LOW_LAVEL_TASK_STACK_SIZE, ( void* )this, this->cfg->task_prio, this->task_stack, &this->task_struct );
}


// Выбираем нужные регистры в AY/YM.
/*
 * По сути, мы просто выдаем в расширитель портов то, что лежит в буффере по адресу p_sr_data.
 * Предполагается, что туда уже положили значения регистров для каждого AY/YM.
 * BDIR и BC1 дергаются так, чтобы произошел выбор регистра.
 */
void ay_ym_low_lavel::out_reg ( void ) const {
    if ( this->cfg->mutex != nullptr)
        USER_OS_TAKE_MUTEX( *this->cfg->mutex, portMAX_DELAY );

    this->cfg->sr->write();
    this->cfg->bc1->set();
    this->cfg->bdir->set();
    this->cfg->bdir->reset();
    this->cfg->bc1->reset();

    if ( this->cfg->mutex != nullptr)
        USER_OS_GIVE_MUTEX( *this->cfg->mutex );
}

//**********************************************************************
// Загружаем в заранее выбранный регистр значение.
// Предполагается, что в буффер по адресу p_sr_data
// уже были загружены нужные значения.
//**********************************************************************

void ay_ym_low_lavel::out_data ( void ) const {
    if ( this->cfg->mutex != nullptr)
        USER_OS_TAKE_MUTEX( *this->cfg->mutex, portMAX_DELAY );

     this->cfg->sr->write();
     this->cfg->bdir->set();
     this->cfg->bdir->reset();

    if ( this->cfg->mutex != nullptr)
        USER_OS_GIVE_MUTEX( *this->cfg->mutex );
}

void ay_ym_low_lavel::full_clear ( void ) const {
    for ( int chip_loop = 0; chip_loop <  this->cfg->ay_number; chip_loop++ ) {
        USER_OS_QUEUE_RESET( this->cfg->queue_array[ chip_loop ] );
    }
    this->hardware_clear();
}

//**********************************************************************
// Вызывается в прерывании по переполнению таймера,
// настроенного на прерывание раз в 50 мс
// ( по умолчанию, значение может меняться ).
//**********************************************************************
void ay_ym_low_lavel::timer_interrupt_handler ( void ) const {
    this->cfg->tim_interrupt_task->clear_interrupt_flag();
    static USER_OS_PRIO_TASK_WOKEN     prio;
    USER_OS_GIVE_BIN_SEMAPHORE_FROM_ISR( this->semaphore, &prio );    // Отдаем симафор и выходим (этим мы разблокируем поток, который выдает в чипы данные).
}

// true - если все очереди пусты.
bool ay_ym_low_lavel::queue_empty_check ( void ) {
    for (int chip_loop = 0; chip_loop <  this->cfg->ay_number; chip_loop++) {
        if ( USER_OS_QUEUE_CHECK_WAIT_ITEM( this->cfg->queue_array[ chip_loop ] ) != 0 ) {
            return false;
        }
    }
    return true;
}

//**********************************************************************
// Запихиваем в очередь на выдачу в AY из массива данные.
// Данные должны быть расположены в формате регистр(16-8 бит)|значение(7-0 бит).
// Задача выполняется из под FreeRTOS.
//**********************************************************************
void ay_ym_low_lavel::queue_add_element ( ay_queue_struct* item ) const {
    ay_low_out_data_struct buf;
    buf.reg     = item->reg;
    buf.data    = item->data;

    USER_OS_QUEUE_SEND( this->cfg->queue_array[item->number_chip], &buf, portMAX_DELAY );
}

// Чистим все AY/YM без использования очереди. Предполагается, что при этом никак не может произойти выдача из очереди.
void ay_ym_low_lavel::hardware_clear ( void ) const {
    //**********************************************************************
    // В каждом AY необходимо в регистр R7 положить 0b111111 (чтобы остановить генерацию звука и шумов).
    // А во все остальные регистры 0.
    //**********************************************************************
    for ( uint32_t loop_chip = 0; loop_chip < this->cfg->ay_number; loop_chip++ ) {
        for ( uint32_t reg_l = 0; reg_l < 7; reg_l++ )
            this->buf_data_chip[ loop_chip ].reg[ reg_l ] = 0;
        this->buf_data_chip[ loop_chip ].reg[ 7 ] = 0b111111;
        for ( uint32_t reg_l = 8; reg_l < 16; reg_l++ )
            this->buf_data_chip[ loop_chip ].reg[ reg_l ] = 0;
     }
    this->send_buffer();
}

// Производим перестоновку бит в байте (с учетом реального подключения.
uint8_t ay_ym_low_lavel::connection_transformation ( const uint8_t chip, const uint8_t& data ) const {
    uint8_t buffer = 0;
    for ( uint8_t loop = 0; loop < 8; loop++ ) {
        buffer |= ( ( data & ( 1 << loop ) ) >> loop ) << this->cfg->con_cfg[ chip ].bias_bit[loop];
    }
    return buffer;
}

void ay_ym_low_lavel::reset_flag_wait ( bool* flag_array ) {
    for ( uint32_t chip_loop = 0; chip_loop <  this->cfg->ay_number; chip_loop++ ) {
        flag_array[ chip_loop ] = false;
    }
}

// False - когда все флаги False.
bool ay_ym_low_lavel::chack_flag_wait ( bool* flag_array ) {
    for ( uint32_t chip_loop = 0; chip_loop <  this->cfg->ay_number; chip_loop++ )
        if ( flag_array[ chip_loop ] == false ) return true;
    return false;
}

// Отправляем данные на все чипы из буфера.
// Это нужно либо для очистки (когда буфер пуст изначально), либо для восстановления после паузы.
void ay_ym_low_lavel::send_buffer ( void ) const {
    for ( uint32_t reg_loop = 0; reg_loop < 16; reg_loop++ ) {
        for ( uint32_t loop_ay = 0; loop_ay < this->cfg->ay_number; loop_ay++ )
            this->cfg->p_sr_data[ loop_ay ] = this->connection_transformation( loop_ay, reg_loop );
        this->out_reg();

        for ( uint32_t loop_ay = 0; loop_ay < this->cfg->ay_number; loop_ay++ )
            this->cfg->p_sr_data[ loop_ay ] = this->connection_transformation( loop_ay, this->buf_data_chip[ loop_ay ].reg[ reg_loop ] );
        this->out_data();
    }
}

//**********************************************************************
// Данный поток будет выдавать из очереди 50 раз
// в секунду данные (данные разделяются 0xFF).
// Разблокируется из ay_timer_handler.
//**********************************************************************

void ay_ym_low_lavel::task ( void* p_this ) {
    ay_ym_low_lavel*            obj = ( ay_ym_low_lavel* ) p_this;
    ay_low_out_data_struct      buffer[ obj->cfg->ay_number ];          // Буфер для адрес/команда для всех чипов.
    bool                        flag_wait[ obj->cfg->ay_number ];       // True - в этом прерывании уже не обрабатываем эту очередь.

    while( true ) {
        obj->reset_flag_wait( flag_wait );                              // Новое прерывание, все флаги можно сбросить.
        USER_OS_TAKE_BIN_SEMAPHORE ( obj->semaphore, portMAX_DELAY );   // Как только произошло прерывание (была разблокировка из ay_timer_handler).
        if ( obj->queue_empty_check() == true ) continue;               // Если в очередях пусто - выходим.

        while ( obj->chack_flag_wait( flag_wait ) ) {
            // Собираем из всех очередей пакет регистр/значение.
            for ( uint32_t chip_loop = 0; chip_loop <  obj->cfg->ay_number; chip_loop++ ) {
                if ( flag_wait[chip_loop] ) continue;
                USER_OS_QUEUE_CHECK_WAIT_ITEM( obj->cfg->queue_array[ chip_loop ] );
                uint32_t count = uxQueueMessagesWaiting( obj->cfg->queue_array[chip_loop] );
                if ( count != 0 ) {                                                                         // Если для этого чипа очередь не пуста.
                    USER_OS_QUEUE_RECEIVE( obj->cfg->queue_array[ chip_loop ], &buffer[ chip_loop ], 0 );   // Достаем этот элемент без ожидания, т.к. точно знаем, что он есть.
                    if ( buffer[ chip_loop ].reg == 0xFF ) {    // Если это флаг того, что далее читать можно лишь в следущем прерывании,...
                        buffer[chip_loop].reg = 17;             // Если этот чип уже неактивен, то пишем во внешний регистр (пустоту). Сейчас и далее.
                        flag_wait[chip_loop] = true;            // Защищаем эту очередь от последущего считывания в этом прерывании.
                    } else {    // Дублируем в памяти данные чипов.
                        obj->buf_data_chip[ chip_loop ].reg[ buffer[ chip_loop ].reg ] =  buffer[chip_loop].data;
                    }
                } else {
                    buffer[chip_loop].reg = 17;
                    flag_wait[chip_loop] = true;     // Показываем, что в этой очереди закончились элементы.
                }
            }

            //**********************************************************************
            // Собранный пакет раскладываем на регистры и на их значения и отправляем.
            //**********************************************************************
            for ( uint32_t chip_loop = 0; chip_loop <  obj->cfg->ay_number; chip_loop++ )
                obj->cfg->p_sr_data[chip_loop] = obj->connection_transformation( chip_loop, buffer[chip_loop].reg );

            obj->out_reg();
            for ( uint32_t chip_loop = 0; chip_loop <  obj->cfg->ay_number; chip_loop++ )
                obj->cfg->p_sr_data[chip_loop] = obj->connection_transformation( chip_loop, buffer[chip_loop].data );

            obj->out_data();
        }
        //**********************************************************************
        // В случае, если идет отслеживание секунд воспроизведения, то каждую секунду отдаем симафор.
        //**********************************************************************
        obj->tic_ff++;
        if ( obj->tic_ff != 50 )                        continue;
            obj->tic_ff = 0;// Если насчитали секунду.
        if ( obj->cfg->semaphore_sec_out == nullptr )   continue;         // Если есть соединение семофором, то отдать его.
            USER_OS_GIVE_BIN_SEMAPHORE( *obj->cfg->semaphore_sec_out );
    }
}

// Останавливаем/продолжаем с того же места воспроизведение. Синхронно для всех AY/YM.
void ay_ym_low_lavel::play_state_set ( uint8_t state ) const {
    this->cfg->pwr_set( state );
    if ( state == 1 ) {
        this->cfg->tim_frequency_ay->on();
        this->cfg->tim_interrupt_task->on();
        this->send_buffer();                                            // Восстанавливаем контекст.
    } else {
        this->cfg->tim_interrupt_task->off();
        this->cfg->tim_frequency_ay->off();
    };
}
