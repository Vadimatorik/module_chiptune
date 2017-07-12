#include "ay_ym_note_mode.h"

// Массив делителей для формирования совместимых с фортепианными звуками нот.

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

