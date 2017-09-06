#include "ay_ym_low_lavel.h"

ay_ym_low_lavel::ay_ym_low_lavel ( const ay_ym_low_lavel_cfg_t* const cfg ) : cfg( cfg ) {
    this->semaphore = USER_OS_STATIC_BIN_SEMAPHORE_CREATE( &this->semaphore_buf );
    memset( this->cfg->r7_reg, 0b111111,  this->cfg->ay_number );         // Все чипы отключены.
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
    this->cfg->tim_interrupt_task->off();
    for ( int chip_loop = 0; chip_loop <  this->cfg->ay_number; chip_loop++ ) {
        USER_OS_QUEUE_RESET( this->cfg->queue_array[ chip_loop ] );
    }
    this->hardware_clear();
    this->cfg->tim_frequency_ay->off();
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
    // Сохраняем во внутреннее хранилище новое сосотояние всех чипов.
    for ( uint32_t loop_ay = 0; loop_ay < this->cfg->ay_number; loop_ay++ )
        this->cfg->r7_reg[loop_ay] = 0b111111;

    // Во всех чипах выбираем 7-й регистр (регистр управления).
    for ( uint32_t loop_ay = 0; loop_ay < this->cfg->ay_number; loop_ay++ )
        this->cfg->p_sr_data[ loop_ay ] = this->connection_transformation( loop_ay, 7 );
    this->out_reg();

    // Во все 7-е регистры кладем команду отключения всех шумов и звуков на каналах.
    for ( uint32_t loop_ay = 0; loop_ay < this->cfg->ay_number; loop_ay++ )
        this->cfg->p_sr_data[ loop_ay ] = this->connection_transformation( loop_ay, 0b111111 );
    this->out_data();


    for (int l = 0; l<7; l++) {            // 0..6 регистры заполняются 0-ми.
        for ( uint32_t loop_ay = 0; loop_ay < this->cfg->ay_number; loop_ay++ )
            this->cfg->p_sr_data[ loop_ay ] = this->connection_transformation( loop_ay, l );
        this->out_reg();

        memset( this->cfg->p_sr_data, 0,  this->cfg->ay_number );   // В 0-ле нечего переставлять :), так что connection_transformation не используется.
        this->out_data();
    };

    for (int l = 8; l<0xF; l++){            // 8..15 регистры заполняются 0-ми.
        for ( uint32_t loop_ay = 0; loop_ay < this->cfg->ay_number; loop_ay++ )
            this->cfg->p_sr_data[ loop_ay ] = this->connection_transformation( loop_ay, l );
        this->out_reg();

        memset( this->cfg->p_sr_data, 0,  this->cfg->ay_number );   // В 0-ле нечего переставлять :), так что connection_transformation не используется.
        this->out_data();
    };
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

//**********************************************************************
// Данный поток будет выдавать из очереди 50 раз
// в секунду данные (данные разделяются 0xFF).
// Разблокируется из ay_timer_handler.
//**********************************************************************

void ay_ym_low_lavel::task ( void* p_this ) {
    ay_ym_low_lavel*            obj = ( ay_ym_low_lavel* ) p_this;
    ay_low_out_data_struct      buffer[ obj->cfg->ay_number ];          // Буфер для адрес/команда для всех чипов.
    bool                        flag_wait[ obj->cfg->ay_number ];       // True - в этом прерывании уже не обрабатываем эту очередь.
    obj->hardware_clear();                                              // Важно очистить чипы полностью без использования очереди.

    while( true ) {
        obj->reset_flag_wait( flag_wait );                              // Новое прерывание, все флаги можно сбросить.
        USER_OS_TAKE_BIN_SEMAPHORE ( obj->semaphore, portMAX_DELAY );   // Как только произошло прерывание (была разблокировка из ay_timer_handler).
        if ( obj->queue_empty_check() == true ) continue;               // Если в очередях пусто - выходим.

        // Собираем из всех очередей пакет регистр/значение.
        for ( uint32_t chip_loop = 0; chip_loop <  obj->cfg->ay_number; chip_loop++ ) {
            if ( !flag_wait[chip_loop] ) continue;
            USER_OS_QUEUE_CHECK_WAIT_ITEM( obj->cfg->queue_array[ chip_loop ] );
            uint32_t count = uxQueueMessagesWaiting( obj->cfg->queue_array[chip_loop] );
            if ( count != 0 ) {                                                                         // Если для этого чипа очередь не пуста.
                USER_OS_QUEUE_RECEIVE( obj->cfg->queue_array[ chip_loop ], &buffer[ chip_loop ], 0);    // Достаем этот элемент без ожидания, т.к. точно знаем, что он есть.
                if ( buffer[ chip_loop ].reg == 0xFF ) {    // Если это флаг того, что далее читать можно лишь в следущем прерывании,...
                    buffer[chip_loop].reg = 17;             // Если этот чип уже неактивен, то пишем во внешний регистр (пустоту). Сейчас и далее.
                    flag_wait[chip_loop] = true;            // Защищаем эту очередь от последущего считывания в этом прерывании.
                } else if ( buffer[chip_loop].reg == 7){    // Сохраняем состояние 7-го регситра разрешения генерации звука и шумов. Чтобы в случае паузы было что вернуть. После затирания 0b111111 (отключить генерацию всего).
                    obj->cfg->r7_reg[chip_loop] =  buffer[chip_loop].data;
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

        //**********************************************************************
        // В случае, если идет отслеживание секунд воспроизведения, то каждую секунду отдаем симафор.
        //**********************************************************************
        obj->tic_ff++;
        if ( obj->tic_ff != 50 )                        return;
        obj->tic_ff = 0;// Если насчитали секунду.
        if ( obj->cfg->semaphore_sec_out == nullptr )   return;         // Если есть соединение семофором, то отдать его.
        USER_OS_GIVE_BIN_SEMAPHORE( *obj->cfg->semaphore_sec_out );
    }
}

// Останавливаем/продолжаем с того же места воспроизведение. Синхронно для всех AY/YM.
void ay_ym_low_lavel::play_state_set ( uint8_t state ) const {
    memset( this->cfg->p_sr_data, 7,  this->cfg->ay_number );           // В любом случае писать будем в R7.
    this->out_reg();

    if ( state == 1 ) {
        this->cfg->tim_frequency_ay->on();
        this->cfg->tim_interrupt_task->on();
        for ( int loop_ay = 0; loop_ay < this->cfg->ay_number; loop_ay++ ) {        // Возвращаем состояние всех AY.
             this->cfg->p_sr_data[loop_ay] = this->cfg->r7_reg[loop_ay];
        };
        this->out_data();
    } else {
        this->cfg->tim_interrupt_task->off();
        this->cfg->tim_frequency_ay->off();
        memset( this->cfg->p_sr_data, 0b111111,  this->cfg->ay_number );
        this->out_data();
    };
}
