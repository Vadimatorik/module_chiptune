#include "psg_reader.h"
#include <cstddef>

int psg_reader::parse (mc_file_container &c, func_out &funcs) {
    int rv = 0;

    uint32_t rest_size = c.size();
    if (rest_size < sizeof(header)) {
        return EINVAL;
    };

    header h;
    auto* p = reinterpret_cast<uint8_t*>(&h);
    for (uint32_t i = 0; i < sizeof(header); i++) {
        p[i] = c[i];
    }

    if (h.version == INT_BEGIN) {
        if ((rv = c.seek(offsetof(header, version))) != 0) {
            return rv;
        }
    } else {
        if ((rv = c.seek(sizeof(header))) != 0) {
            return rv;
        }
    }

    while (rest_size) {
        const uint8_t reg = c;
        ++c;
        if ((rv = c.state()) != 0) {
            return rv;
        }
        --rest_size;
        if (INT_BEGIN == reg) {
            funcs.sleep(*this, 1);
        } else if (INT_SKIP == reg) {
            if (rest_size < 1) { // Put byte back.
                break;
            }
            funcs.sleep(*this, 4*c);
            ++c;
            if ((rv = c.state()) != 0) {
                return rv;
            }
            --rest_size;
        } else if (MUS_END == reg) {
            break;
        } else if (reg <= 15) { // Register.
            if (rest_size < 1) { // Put byte back.
                break;
            }
            funcs.set_reg(*this, reg, c);
            ++c;
            if ((rv = c.state()) != 0) {
                return rv;
            }
            --rest_size;
        } else { // Put byte back.
            break;
        }
    }

    return 0;
}

int psg_reader::play_psg (mc_file_container &c) {
    func_out funcs = {
        .sleep = &psg_reader::sleep,
        .set_reg = &psg_reader::set_reg
    };

    return this->parse(c, funcs);
}

int psg_reader::inc_sleep (uint32_t num) {
    this->track_len += num;
    return 0;
}

int psg_reader::dummy_set_reg (uint8_t reg, uint8_t data) {
    (void)reg;
    (void)data;
    return 0;
}

int psg_reader::get_len_psg (mc_file_container &c, uint32_t &len) {
    func_out funcs = {
        .sleep = &psg_reader::inc_sleep,
        .set_reg = &psg_reader::dummy_set_reg
    };

    int rv = 0;
    if ((rv = this->parse(c, funcs)) != 0) {
        return rv;
    }

    len = this->track_len;
    return 0;
}