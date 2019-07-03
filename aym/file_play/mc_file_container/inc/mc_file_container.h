#pragma once

#include "fatfs.h"

class mc_file_container {
public:
    int open_file (std::shared_ptr<char> p);
    int close_file (void);

private:
    void resetFlags ( void );

private:
    static const uint32_t BUFFER_SIZE_BYTE = 1024 * 3;

    /// Т.к. методы зачастую читают по 1 байту, то чтобы ускорить этот процесс
    /// сразу копируется значительный кусок трека.
    __attribute__((__aligned__(4)))
    uint8_t f_buf[BUFFER_SIZE_BYTE] = {0};

    /// Смещение, с которого было скопирован кусок.
    uint32_t pointStartSeekBuffer = 0;

    /// Смещение, с которого будет считан следующий байт/последовательность
    /// байт (относительно буффера).
    uint32_t pointInBuffer = 0;

private:
    bool flagStop = false;

private:
    std::shared_ptr<FIL> f = nullptr;
    fatfs fat;

};