#include "ay_ym_low_lavel.h"



template < uint8_t AY_NUMBER, uint8_t QUEUE_LENGTH >
void ay_ym_low_lavel< AY_NUMBER, QUEUE_LENGTH >::init ( void ) const {
    this->queue         = USER_OS_STATIC_QUEUE_CREATE( QUEUE_LENGTH, sizeof( ay_queue_struct ), this->queue_buf, &this->q_st_buf );
    this->semaphore     = USER_OS_STATIC_BIN_SEMAPHORE_CREATE( &this->semaphore_buf );

    memset( this->r7_reg, 0b111111, AY_NUMBER );         // Все чипы отключены.

    USER_OS_STATIC_TASK_CREATE( this->task, "ay_low", 300, ( void* )this, this->cfg->priority_task, this->task_stack, &this->task_struct );
}


// Выбираем нужные регистры в AY/YM.
/*
 * По сути, мы просто выдаем в расширитель портов то, что лежит в буффере по адресу p_sr_data.
 * Предполагается, что туда уже положили значения регистров для каждого AY/YM.
 * BDIR и BC1 дергаются так, чтобы произошел выбор регистра.
 */
template < uint8_t AY_NUMBER, uint8_t QUEUE_LENGTH >
void ay_ym_low_lavel< AY_NUMBER, QUEUE_LENGTH >::out_reg ( void ) const {
    this->cfg->sr->write();
    this->cfg->bc1->set();
    this->cfg->bdir->set();
    this->cfg->bdir->reset();
    this->cfg->bc1->reset();
}

/*
 * Загружаем в заранее выбранный регистр значение.
 * Предполагается, что в буффер по адресу p_sr_data уже были загружены нужные значения.
 */
template < uint8_t AY_NUMBER, uint8_t QUEUE_LENGTH >
void ay_ym_low_lavel< AY_NUMBER, QUEUE_LENGTH >::out_data ( void ) const {
    this->cfg->sr->write();
    this->cfg->bdir->set();
    this->cfg->bdir->reset();
}

/*
 * Вызывается в прерывании по переполнению таймера, настроенного на прерывание раз в 50 мс (по-умолчанию, значение может меняться).
 */
template < uint8_t AY_NUMBER, uint8_t QUEUE_LENGTH >
void ay_ym_low_lavel< AY_NUMBER, QUEUE_LENGTH >::timer_interrupt_handler ( void ) const {
    static USER_OS_PRIO_TASK_WOKEN     prio;
    USER_OS_GIVE_BIN_SEMAPHORE_FROM_ISR( this->semaphore, &prio );    // Отдаем симафор и выходим (этим мы разблокируем поток, который выдает в чипы данные).
}

/*
 * Запихиваем в очередь на выдачу в AY из массива данные.
 * Данные должны быть расположены в формате регистр(16-8 бит)|значение(7-0 бит).
 * Задача выполняется из под FreeRTOS.
 */
template < uint8_t AY_NUMBER, uint8_t QUEUE_LENGTH >
void ay_ym_low_lavel< AY_NUMBER, QUEUE_LENGTH >::queue_add_element ( ay_queue_struct* item ) const {
    ay_low_out_data buf;
    buf.reg     = item->reg;
    buf.data    = item->data;

    xQueueSend( this->cfg->p_queue_array[item], buf, portMAX_DELAY );
}

// Включить/выключить 1 канал одного из чипов. Через очередь.
template < uint8_t AY_NUMBER, uint8_t QUEUE_LENGTH >
void ay_ym_low_lavel< AY_NUMBER, QUEUE_LENGTH >::set_channel ( uint8_t number_ay, uint8_t channel, bool set ) const {
    if ( set ) {                                                                // Если включить канал.
        this->r7_reg[number_ay] &= ~( 1 << channel);
    } else {
        this->r7_reg[number_ay] |= 1 << channel;                                // Если отключить.
    }
    ay_queue_struct buf;
    buf.data = this->r7_reg[number_ay];
    buf.reg  = 7;
    buf.number_chip = number_ay;
    this->queue_add_element( &buf );    // Выбираем R7.
}

// Чистим все AY/YM без использования очереди. Предполагается, что при этом никак не может произойти выдача из очереди.
template < uint8_t AY_NUMBER, uint8_t QUEUE_LENGTH >
void ay_ym_low_lavel< AY_NUMBER, QUEUE_LENGTH >::hardware_clear ( void ) const {
    /*
     * В каждом AY необходимо в регистр R7 положить 0b111111 (чтобы остановить генерацию звука и шумов).
     * А во все остальные регистры 0.
     */
    for (int loop_ay = 0; loop_ay < AY_NUMBER; loop_ay++){
        this->r7_reg[loop_ay] = 0b111111;    // Сохраняем во внутреннее хранилище новое сосотояние всех чипов.
    };
    memset(this->cfg->p_sr_data, 7, AY_NUMBER);        // Во всех чипах выбираем 7-й регистр (регистр управления).
    this->out_reg();
    memset(this->cfg->p_sr_data, 0b111111, AY_NUMBER); // Во все 7-е регистры кладем команду отключения всех шумов и звуков на каналах.
    this->out_reg();
    for (int l = 0; l<7; l++) {            // 0..6 регистры заполняются 0-ми.
        memset(this->cfg->p_sr_data, l, AY_NUMBER);
        this->out_reg();
        memset(this->cfg->p_sr_data, 0, AY_NUMBER);
        this->out_data();
    };
    for (int l = 8; l<0xF; l++){            // 8..15 регистры заполняются 0-ми.
        memset(this->cfg->p_sr_data, l, AY_NUMBER);
        this->out_reg();
        memset(this->cfg->p_sr_data, 0, AY_NUMBER);
        this->out_data();
    };
}

/*
 * Данный поток будет выдавать из очереди 50 раз
 * в секунду данные (данные разделяются 0xFF). Разблокируется из ay_timer_handler.
 */
template < uint8_t AY_NUMBER, uint8_t QUEUE_LENGTH >
void ay_ym_low_lavel< AY_NUMBER, QUEUE_LENGTH >::task () const {
        ay_low_out_data buffer[AY_NUMBER];     // Буфер для адрес/команда для всех чипов.
        uint32_t flag;                         // Внутренняя переменная "опустошения очереди". Она будет сравниваться с flag_over (читать подробное описание у этой переменной).
        this->hardware_clear();                // Важно очистить чипы полностью без использования очереди.
        while( true ) {
            flag = 0;            // Предположим, что данные есть во всех очерядях.
            xSemaphoreTake ( this->semaphore, portMAX_DELAY );   // Как только произошло прерывание (была разблокировка из ay_timer_handler).
                while (flag != (0xFFFFFFFF >>(32-AY_NUMBER)) ) {    // Выдаем данные, пока все очереди не освободятся.]
                    /*
                     * Собираем из всех очередей пакет регистр/значение (если нет данных, то NO_DATA_FOR_AY).
                     */
                    for ( int chip_loop = 0; chip_loop < AY_NUMBER; chip_loop++ ) {    // Собираем регистр/данные со всех очередей всех чипов.
                        if ( ( flag & ( 1 << chip_loop ) ) != 0) {    // Если до этого в очереди конкретного чипа еще были элементы.
                            if (uxQueueMessagesWaiting(this->queue_out[chip_loop]) != 0) {    // Если для этого чипа очередь не пуста.
                                xQueueReceive(this->cfg->p_queue_array[chip_loop], &buffer[chip_loop], 0);    // Достаем этот эхлемент без ожидания, т.к. точно знаем, что он есть.
                                if ( buffer.reg == 0xFF ){    // Если это флаг того, что далее читать можно лишь в следущем прерывании,...
                                    flag |= 1<<chip_loop; // то защищаем эту очередь от последущего считывания в этом прерывании.
                                } else {    // Если пришли реальные данные.
                                    if (  buffer.reg == 7){            // Сохраняем состояние 7-го регситра разрешения генерации звука и шумов. Чтобы в случае паузы было что вернуть. После затирания 0b111111 (отключить генерацию всего).
                                        this->r7_reg[chip_loop] =  buffer.reg;
                                    }
                                }
                            } else {
                                flag |= 1<<chip_loop;        // Показываем, что в этой очереди закончились элементы.
                            }
                        }
                    }

                    /*
                     * Собранный пакет раскладываем на регистры и на их значения и отправляем.
                     */
                    for (int chip_loop = 0; chip_loop < AY_NUMBER; chip_loop++){    // Раскладываем на регистры.
                        this->cfg->p_sr_data[chip_loop] = buffer[chip_loop].reg;
                    }
                    this->out_reg();
                    for (int chip_loop = 0; chip_loop < AY_NUMBER; chip_loop++){    // Раскладываем на значения.
                        this->cfg->p_sr_data[chip_loop] = buffer[chip_loop].data;
                    }
                    this->out_data();
                }

            /*
             * В случае, если идет отслеживание секунд воспроизведения, то каждую секунду отдаем симафор.
             */
            this->cfg->tic_ff++;
            if (this->cfg->tic_ff == 50){        // Если насчитали секунду.
                this->cfg->tic_ff = 0;
                if (this->cfg->semaphore_sec_out != NULL){    // Если есть соединение семофором, то отдать его.
                     xSemaphoreGive(*this->cfg->semaphore_sec_out);
                };
            };
    }
}

/*
// Останавливаем/продолжаем с того же места воспроизведение. Синхронно для всех AY/YM.
void ay_play_stait (int fd, uint8_t stait){
    ay_init_t *d = eflib_getInstanceByFd (fd);

    memset(this->cfg->p_sr_data, 7, AY_NUMBER);    // В любом случае писать будем в R7.
    _out_reg(d);

    if (stait){
        port_timer_set_stait(*this->cfg->tim_frequency_ay_fd, 1);
        port_timer_set_stait(*this->cfg->tim_event_ay_fd, 1);                    // Запускаем генерацию сигнала.
        for (int loop_ay = 0; loop_ay<AY_NUMBER; loop_ay++){        // Возвращаем состояние всех AY.
            this->cfg->p_sr_data[loop_ay] = this->r7_reg[loop_ay];
        };
        _out_data(d);
    } else {
        port_timer_set_stait(*this->cfg->tim_event_ay_fd, 0);    // Останавливаем генерацию сигнала.
        port_timer_set_stait(*this->cfg->tim_frequency_ay_fd, 0);
        memset(this->cfg->p_sr_data, 0b111111, AY_NUMBER);
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
    for (int loop_ay = 0; loop_ay<AY_NUMBER; loop_ay++){        // Извлекаем данные из всех очередей.
        while ((uxQueueMessagesWaiting(this->queue_out[loop_ay]) != 0 )){    // Если очередь не пуста (Есть хотя бы 1 пакет)- извлекаем все.
            xQueueReceive(this->queue_out[loop_ay], &buf, 0);
        }
    };
}
*/
