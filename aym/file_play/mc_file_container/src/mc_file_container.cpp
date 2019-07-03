#include "mc_file_container.h"
#include <memory>
#include <cmath>

void mc_file_container::reset_flags (void) {
    this->flag_stop = false;

    this->sector = 0;
    this->pos_in_buf = 0;
}

int mc_file_container::open_file (std::shared_ptr<char> p) {
    this->reset_flags();

    int rv = 0;

    this->f = this->fat.openFile(p, rv);
    if (rv != 0) {
        return rv;
    }

    uint32_t size = this->size();

    UINT read_size = 0;
    if (size < BUFFER_SIZE_BYTE) {
        read_size = size;
    } else {
        read_size = BUFFER_SIZE_BYTE;
    }

    rv = this->fat.read(this->f, this->f_buf, read_size);
    if (rv != EOK) {
        this->fat.close_file(this->f);
        return rv;
    }

    return rv;
}

mc_file_container &mc_file_container::operator++ () {
    this->pos_in_buf++;

    if (this->pos_in_buf >= MAX_SEEK) {
        sector += 1;
        this->pos_in_buf = 0;

        uint32_t len = std::min(MAX_SEEK, this->fat.size(this->f) - MAX_SEEK*sector);

        this->e_status = this->fat.read(this->f, this->f_buf, len);
        if (this->e_status != EOK) {
            this->fat.close_file(this->f);
        }
    }

    return *this;
}


/*
mc_file_container &mc_file_container::operator-- () {
    return *this;
}*/
/*
int mc_file_container::read (uint8_t *returnDataBuffer, const uint32_t countByteRead) {
    int	r;

    /// Если то, что мы хотим считать уже есть в буффере.
    if ( this->pointInBuffer + countByteRead <= BUFFER_SIZE_BYTE ) {
        /// Просто копируем из массива и смещаем указатель.
        memcpy( returnDataBuffer, &this->flashBuffer[ this->pointInBuffer ], countByteRead );
        this->pointInBuffer += countByteRead;

        return 0;
    }

    /// Если мы не влезли, то придется скопировть часть из буффера, а часть загрузить из
    /// обновленного буфера (который еще надо обновить...).

    /// Копируем что есть.
    uint32_t fromBufferByte = BUFFER_SIZE_BYTE - this->pointInBuffer;
    memcpy( returnDataBuffer, &this->flashBuffer[ this->pointInBuffer ], fromBufferByte );

    /// Теперь буффер у нас на одно "окно" дальше.
    this->pointStartSeekBuffer += BUFFER_SIZE_BYTE;
    this->pointInBuffer	= 0;

    /// Далее будем класть уже после скопированного.
    uint8_t* nextPart = returnDataBuffer + fromBufferByte;

    /// Узнаем длину трека.
    uint32_t countByteInTreck = this->fat.getFileSize( this->file );

    /// Решаем, сколько скопировать в буффер.
    UINT getCountByte;
    if (	countByteInTreck - this->pointStartSeekBuffer <
            BUFFER_SIZE_BYTE	) {
        getCountByte = countByteInTreck - this->pointStartSeekBuffer;
    } else {
        getCountByte = BUFFER_SIZE_BYTE;
    }

    /// Забираем в буффер данные.
    r =  this->fat.readFromFile( this->file, this->flashBuffer, getCountByte );
    errnoCheckAndReturn( r );

    /// Забираем оставшуюся часть.
    memcpy( nextPart, &this->flashBuffer[ this->pointInBuffer ], countByteRead - fromBufferByte );
    pointInBuffer += countByteRead - fromBufferByte;

    return 0;
*/
/*


   while( this->cfg->ayLow->queueEmptyCheck() != true ) {			/// Ждем, пока AY освободится.
        USER_OS_DELAY_MS(20);
    }
      //this->cfg->ayLow->playStateSet( 0 );								/// Отключаем аппаратку.
    //this->set_pwr_chip(false);									/// Питание чипа.
    //this->cfg->ayLow->queueClear();									/// Чистим очередь.

 */

int mc_file_container::close_file (void) {
    return this->fat.close_file(this->f);
}

int mc_file_container::size (void) {
    return this->fat.size(this->f);
}

int mc_file_container::state (void) {
    return this->e_status;
}

int mc_file_container::seek (uint32_t num) {
    if (num > static_cast<uint32_t>(this->size() - 1)) {
        return 0;
    }

    if ((num > this->sector * this->MAX_SEEK) && (num < (this->sector + 1) * this->MAX_SEEK)) {
        this->pos_in_buf = num - this->sector * this->MAX_SEEK;
        return 0;
    }

    this->sector = num / this->MAX_SEEK;

    uint32_t len = std::min(MAX_SEEK, this->fat.size(this->f) - MAX_SEEK*sector);

    this->e_status = this->fat.read(this->f, this->f_buf, len);
    if (this->e_status != EOK) {
        this->fat.close_file(this->f);
        return this->e_status;
    }

    this->pos_in_buf = num - this->sector * this->MAX_SEEK;

    return 0;
}

uint8_t mc_file_container::operator[] (uint32_t index) {
    if (index > static_cast<uint32_t>(this->size() - 1)) {
        return 0;
    }

    if ((index >= this->sector * this->MAX_SEEK) && (index < (this->sector + 1) * this->MAX_SEEK)) {
        this->pos_in_buf = index - this->sector * this->MAX_SEEK;
        return this->f_buf[pos_in_buf];
    }

    this->sector = index / this->MAX_SEEK;

    uint32_t len = std::min(MAX_SEEK, this->fat.size(this->f) - MAX_SEEK*sector);

    this->e_status = this->fat.read(this->f, this->f_buf, len);
    if (this->e_status != EOK) {
        this->fat.close_file(this->f);
    }

    this->pos_in_buf = index - this->sector * this->MAX_SEEK;
    return this->f_buf[pos_in_buf];
}