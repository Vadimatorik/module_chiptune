#include "mc_file_container.h"
#include <memory>

void mc_file_container::resetFlags (void) {
    this->flagStop = false;

    this->pointStartSeekBuffer = 0;
    this->pointInBuffer = 0;
}

int mc_file_container::open_file (std::shared_ptr<char> p) {
    this->resetFlags();

    int r;

    this->f = this->fat.openFile(p, r);
    if (r != 0) {
        return r;
    }

    uint32_t countByteInTreck;
    countByteInTreck = this->fat.getFileSize(this->f);

    if (countByteInTreck < 16)
        return ENOEXEC;

    UINT getCountByte;
    if (countByteInTreck < BUFFER_SIZE_BYTE) {
        getCountByte = countByteInTreck;
    } else {
        getCountByte = BUFFER_SIZE_BYTE;
    }

    r = this->fat.readFromFile(this->f, this->f_buf, getCountByte);
    if (r != EOK) {
        return this->fat.closeFile(this->f);
    }

    return EOK;
}

/*


   while( this->cfg->ayLow->queueEmptyCheck() != true ) {			/// Ждем, пока AY освободится.
        USER_OS_DELAY_MS(20);
    }
      //this->cfg->ayLow->playStateSet( 0 );								/// Отключаем аппаратку.
    //this->set_pwr_chip(false);									/// Питание чипа.
    //this->cfg->ayLow->queueClear();									/// Чистим очередь.

 */

int mc_file_container::close_file (void) {
    return this->fat.closeFile(this->f);
}
