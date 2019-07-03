#pragma once

#include "fatfs.h"

class mc_file_container {
public:
    int open_file (std::shared_ptr<char> p);
    int close_file (void);
    int size (void);
    int state (void);
    int seek (uint32_t num);

public:
    mc_file_container& operator++();
    operator uint8_t() {
        return this->f_buf[this->pos_in_buf];
    }

    uint8_t operator[] (uint32_t index);

private:
    mc_file_container& operator--() = delete;
    mc_file_container operator++(int) = delete;
    mc_file_container operator--(int) = delete;

private:
    void reset_flags (void);

private:
    static const uint32_t BUFFER_SIZE_BYTE = 1024 * 3;
    const uint32_t MAX_SEEK = BUFFER_SIZE_BYTE;

    /// Т.к. методы зачастую читают по 1 байту, то чтобы ускорить этот процесс
    /// сразу копируется значительный кусок трека.
    __attribute__((__aligned__(4)))
    uint8_t f_buf[BUFFER_SIZE_BYTE] = {0};

    /// Смещение, с которого было скопирован кусок.
    uint32_t sector = 0;

    /// Смещение, с которого будет считан следующий байт/последовательность
    /// байт (относительно буффера).
    uint32_t pos_in_buf = 0;

private:
    bool flag_stop = false;

private:
    int e_status = 0;

private:
    std::shared_ptr<FIL> f = nullptr;
    fatfs fat;

};