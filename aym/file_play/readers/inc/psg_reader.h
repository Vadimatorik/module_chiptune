#pragma once

#include "mc_file_container.h"

#include <functional>

class psg_reader {
public:
    constexpr psg_reader (void) {}

public:
    int play_psg (mc_file_container &c);
    int get_len_psg (mc_file_container &c, uint32_t &len);

private:
    virtual int sleep (uint32_t num) = 0;
    virtual int set_reg (uint8_t reg, uint8_t data) = 0;

private:
    int inc_sleep (uint32_t num);
    int dummy_set_reg (uint8_t reg, uint8_t data);

private:
    struct func_out {
        std::function <int(psg_reader&, uint32_t num)> sleep;
        std::function <int(psg_reader&, uint8_t reg, uint8_t data)> set_reg;
    };

    int parse (mc_file_container &c, func_out &funcs);

private:
    enum {
        MARKER = 0x1a,

        INT_BEGIN = 0xff,
        INT_SKIP = 0xfe,
        MUS_END = 0xfd
    };

    const uint8_t SIGNATURE[3] = {'P', 'S', 'G'};

    struct header {
        uint8_t sign[3];
        uint8_t marker;
        uint8_t version;
        uint8_t interrupt;
        uint8_t padding[10];
    };

private:
    uint32_t track_len = 0;

};