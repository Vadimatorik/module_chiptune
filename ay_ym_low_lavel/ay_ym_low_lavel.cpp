#include "ay_ym_low_lavel.h"

void ay_ym_low_lavel::init ( void ) const {
    this->semaphore     = USER_OS_STATIC_BIN_SEMAPHORE_CREATE( &this->semaphore_buf );

    memset( this->cfg->r7_reg, 0b111111,  this->cfg->ay_number );         // Все чипы отключены.

    USER_OS_STATIC_TASK_CREATE( this->task, "ay_low", 300, ( void* )this, this->cfg->task_prio, this->task_stack, &this->task_struct );
}


// Выбираем нужные регистры в AY/YM.
/*
 * По сути, мы просто выдаем в расширитель портов то, что лежит в буффере по адресу p_sr_data.
 * Предполагается, что туда уже положили значения регистров для каждого AY/YM.
 * BDIR и BC1 дергаются так, чтобы произошел выбор регистра.
 */

void ay_ym_low_lavel::out_reg ( void ) const {
     this->cfg->sr->write();
     this->cfg->bc1->set(cfg);
     this->cfg->bdir->set();
     this->cfg->bdir->reset();
     this->cfg->bc1->reset();
}

/*
 * Загружаем в заранее выбранный регистр значение.
 * Предполагается, что в буффер по адресу p_sr_data уже были загружены нужные значения.
 */

void ay_ym_low_lavel::out_data ( void ) const {
     this->cfg->sr->write();
     this->cfg->bdir->set();
     this->cfg->bdir->reset();
}

/*
 * Вызывается в прерывании по переполнению таймера, настроенного на прерывание раз в 50 мс (по-умолчанию, значение может меняться).
 */

void ay_ym_low_lavel::timer_interrupt_handler ( void ) const {
    static USER_OS_PRIO_TASK_WOKEN     prio;
    USER_OS_GIVE_BIN_SEMAPHORE_FROM_ISR( this->semaphore, &prio );    // Отдаем симафор и выходим (этим мы разблокируем поток, который выдает в чипы данные).
}

/*
 * Запихиваем в очередь на выдачу в AY из массива данные.
 * Данные должны быть расположены в формате регистр(16-8 бит)|значение(7-0 бит).
 * Задача выполняется из под FreeRTOS.
 */

void ay_ym_low_lavel::queue_add_element ( ay_queue_struct* item ) const {
    ay_low_out_data buf;
    buf.reg     = item->reg;
    buf.data    = item->data;

    xQueueSend( this->cfg->p_queue_array[item->number_chip], &buf, portMAX_DELAY );
}

// Включить/выключить 1 канал одного из чипов. Через очередь.

void ay_ym_low_lavel::set_channel ( uint8_t number_ay, uint8_t channel, bool set ) const {
    if ( set ) {                                                                // Если включить канал.
        this->cfg->r7_reg[number_ay] &= ~( 1 << channel);
    } else {
        this->cfg->r7_reg[number_ay] |= 1 << channel;                                // Если отключить.
    }
    ay_queue_struct buf;
    buf.data = this->cfg->r7_reg[number_ay];
    buf.reg  = 7;
    buf.number_chip = number_ay;
    this->queue_add_element( &buf );    // Выбираем R7.
}

// Чистим все AY/YM без использования очереди. Предполагается, что при этом никак не может произойти выдача из очереди.

void ay_ym_low_lavel::hardware_clear ( void ) const {
    /*
     * В каждом AY необходимо в регистр R7 положить 0b111111 (чтобы остановить генерацию звука и шумов).
     * А во все остальные регистры 0.
     */
    for (int loop_ay = 0; loop_ay <  this->cfg->ay_number; loop_ay++){
        this->cfg->r7_reg[loop_ay] = 0b111111;    // Сохраняем во внутреннее хранилище новое сосотояние всех чипов.
    };
    memset( this->cfg->p_sr_data, 7,  this->cfg->ay_number );        // Во всех чипах выбираем 7-й регистр (регистр управления).
    this->out_reg();
    memset( this->cfg->p_sr_data, 0b111111,  this->cfg->ay_number ); // Во все 7-е регистры кладем команду отключения всех шумов и звуков на каналах.
    this->out_data();
    for (int l = 0; l<7; l++) {            // 0..6 регистры заполняются 0-ми.
        memset( this->cfg->p_sr_data, l,  this->cfg->ay_number );
        this->out_reg();
        memset( this->cfg->p_sr_data, 0,  this->cfg->ay_number );
        this->out_data();
    };
    for (int l = 8; l<0xF; l++){            // 8..15 регистры заполняются 0-ми.
        memset( this->cfg->p_sr_data, l,  this->cfg->ay_number );
        this->out_reg();
        memset( this->cfg->p_sr_data, 0,  this->cfg->ay_number);
        this->out_data();
    };
}

/*
 * Данный поток будет выдавать из очереди 50 раз
 * в секунду данные (данные разделяются 0xFF). Разблокируется из ay_timer_handler.
 */

void ay_ym_low_lavel::task ( void* p_this ) {
        ay_ym_low_lavel* obj = ( ay_ym_low_lavel* ) p_this;
        ay_low_out_data buffer[ obj->cfg->ay_number];     // Буфер для адрес/команда для всех чипов.
        volatile uint32_t flag;                         // Внутренняя переменная "опустошения очереди". Она будет сравниваться с flag_over (читать подробное описание у этой переменной).
        obj->hardware_clear();                // Важно очистить чипы полностью без использования очереди.
        // Костыль для теста!!!
        ay_low_out_data buf;
        buf.reg     = 7;
        buf.data    = 0b11111000;
        xQueueSend(obj->cfg->p_queue_array[0], &buf, portMAX_DELAY);

        buf.reg     = 0;
        buf.data    = 12;
        xQueueSend(obj->cfg->p_queue_array[0], &buf, portMAX_DELAY);

        buf.reg     = 1;
        buf.data    = 2;
        xQueueSend(obj->cfg->p_queue_array[0], &buf, portMAX_DELAY);

        buf.reg     = 8;
        buf.data    = 10;
        xQueueSend(obj->cfg->p_queue_array[0], &buf, portMAX_DELAY);
        xSemaphoreGive(obj->semaphore);

        while( true ) {
            flag = 0;            // Предположим, что данные есть во всех очерядях.
            xSemaphoreTake ( obj->semaphore, portMAX_DELAY );   // Как только произошло прерывание (была разблокировка из ay_timer_handler).
                while (flag != (0xFFFFFFFF >>(32- obj->cfg->ay_number)) ) {    // Выдаем данные, пока все очереди не освободятся.]
                    /*
                     * Собираем из всех очередей пакет регистр/значение (если нет данных, то NO_DATA_FOR_AY).
                     */
                    for ( volatile uint8_t chip_loop = 0; chip_loop <  obj->cfg->ay_number; chip_loop++ ) {    // Собираем регистр/данные со всех очередей всех чипов.
                        volatile uint32_t count = uxQueueMessagesWaiting(obj->cfg->p_queue_array[chip_loop]);
                        if ( count != 0) {    // Если для этого чипа очередь не пуста.
                                xQueueReceive(obj->cfg->p_queue_array[chip_loop], &buffer[chip_loop], 0);    // Достаем этот эхлемент без ожидания, т.к. точно знаем, что он есть.
                                if ( buffer[chip_loop].reg == 0xFF ){    // Если это флаг того, что далее читать можно лишь в следущем прерывании,...
                                    flag |= 1<<chip_loop; // то защищаем эту очередь от последущего считывания в этом прерывании.
                                } else {    // Если пришли реальные данные.
                                    if ( buffer[chip_loop].reg == 7){            // Сохраняем состояние 7-го регситра разрешения генерации звука и шумов. Чтобы в случае паузы было что вернуть. После затирания 0b111111 (отключить генерацию всего).
                                        obj->cfg->r7_reg[chip_loop] =  buffer[chip_loop].reg;
                                    }
                                }
                            } else {
                                flag |= 1<<chip_loop;        // Показываем, что в этой очереди закончились элементы.
                            }
                    }

                    /*
                     * Собранный пакет раскладываем на регистры и на их значения и отправляем.
                     */
                    for (int chip_loop = 0; chip_loop <  obj->cfg->ay_number; chip_loop++){    // Раскладываем на регистры.
                        obj->cfg->p_sr_data[chip_loop] = buffer[chip_loop].reg;
                    }
                    obj->out_reg();
                    for (int chip_loop = 0; chip_loop <  obj->cfg->ay_number; chip_loop++){    // Раскладываем на значения.
                        obj->cfg->p_sr_data[chip_loop] = buffer[chip_loop].data;
                    }
                    obj->out_data();
                }

            /*
             * В случае, если идет отслеживание секунд воспроизведения, то каждую секунду отдаем симафор.
             */
         /*   obj-> this->cfg->tic_ff++;
            if (obj-> this->cfg->tic_ff == 50){        // Если насчитали секунду.
                obj-> this->cfg->tic_ff = 0;
                if (obj-> this->cfg->semaphore_sec_out != NULL){    // Если есть соединение семофором, то отдать его.
                     xSemaphoreGive(*obj-> this->cfg->semaphore_sec_out);
                };
            };*/
    }
}

/*
// Останавливаем/продолжаем с того же места воспроизведение. Синхронно для всех AY/YM.
void ay_play_stait (int fd, uint8_t stait){
    ay_init_t *d = eflib_getInstanceByFd (fd);

    memset( this->cfg->p_sr_data, 7,  this->cfg->ay_number );    // В любом случае писать будем в R7.
    _out_reg(d);

    if (stait){
        port_timer_set_stait(* this->cfg->tim_frequency_ay_fd, 1);
        port_timer_set_stait(* this->cfg->tim_event_ay_fd, 1);                    // Запускаем генерацию сигнала.
        for (int loop_ay = 0; loop_ay< this->cfg->ay_number, TASK_PRIO, STRUCT_INIT; loop_ay++){        // Возвращаем состояние всех AY.
             this->cfg->p_sr_data[loop_ay] = this->cfg->r7_reg[loop_ay];
        };
        _out_data(d);
    } else {
        port_timer_set_stait(* this->cfg->tim_event_ay_fd, 0);    // Останавливаем генерацию сигнала.
        port_timer_set_stait(* this->cfg->tim_frequency_ay_fd, 0);
        memset( this->cfg->p_sr_data, 0b111111,  this->cfg->ay_number );
        _out_data(d);
    };
}

// Убераем все следы воспроизведения предыдущего трека (остановить). Причем на всех чипах.
void ay_music_off (int fd){
    ay_init_t *d = eflib_getInstanceByFd (fd);
    ay_play_stait(fd, 0);    // Останавливаем генерацию сигнала.
    _ay_hardware_clear(d);    // Регистры в исходное состояние.
    // Очищаем очереди всех AY.
    uint16_t buf;
    for (int loop_ay = 0; loop_ay< this->cfg->ay_number, TASK_PRIO, STRUCT_INIT; loop_ay++){        // Извлекаем данные из всех очередей.
        while ((uxQueueMessagesWaiting(this->queue_out[loop_ay]) != 0 )){    // Если очередь не пуста (Есть хотя бы 1 пакет)- извлекаем все.
            xQueueReceive(this->queue_out[loop_ay], &buf, 0);
        }
    };
}
*/
