#include "ay_ym_note_mode.h"

// Массив делителей для формирования совместимых с фортепианными звуками нот.

/*
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
}*/

int ay_ym_note::reinit ( uint8_t chip_number ) const {
    ay_queue_struct buf_data;
    buf_data.number_chip  = chip_number;
    buf_data.reg = 7;
    buf_data.data = 0b111000;
    cfg->array_chip->queue_add_element( &buf_data );

    return 0;
}

int ay_ym_note::write_note_to_channel ( uint8_t chip_number, uint8_t channel, uint8_t note ) const {
    uint8_t older_byte  = (uint8_t)( this->array_divider_chip[note] >> 8 );
    uint8_t junior_byte = (uint8_t)this->array_divider_chip[note];

    ay_queue_struct buf_data;
    buf_data.number_chip  = chip_number;

    switch( channel ) {      // Записываем ноту в выбранный канал.
    case 0:
            buf_data.reg = 0;  buf_data.data         = junior_byte;
            cfg->array_chip->queue_add_element( &buf_data );

            buf_data.reg = 1;  buf_data.data         = older_byte;
            cfg->array_chip->queue_add_element( &buf_data );
            break;

    case 1:
            buf_data.reg = 2;  buf_data.data         = junior_byte;
            cfg->array_chip->queue_add_element( &buf_data );

            buf_data.reg = 3;  buf_data.data         = older_byte;
            cfg->array_chip->queue_add_element( &buf_data );
            break;

    case 2:
            buf_data.reg = 4;  buf_data.data         = junior_byte;
            cfg->array_chip->queue_add_element( &buf_data );

            buf_data.reg = 5;  buf_data.data         = older_byte;
            cfg->array_chip->queue_add_element( &buf_data );
            break;
    }

    return 0;
}

int ay_ym_note::set_volume_channel ( uint8_t chip_number, uint8_t channel, uint8_t volume ) const {
    ay_queue_struct buf_data;
    buf_data.number_chip    = chip_number;
    buf_data.data           = volume;

    switch (channel) {
    case 0:
            buf_data.reg = 8;
            break;

    case 1:
            buf_data.reg = 9;
            break;

    case 2:
            buf_data.reg = 10;
            break;
    }

    cfg->array_chip->queue_add_element( &buf_data );

    return 0;
}

