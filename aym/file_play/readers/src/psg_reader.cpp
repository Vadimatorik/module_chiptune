#include "psg_reader.h"

void psg_reader::AddChunks (std::size_t count) {
    this->sleep(count);
}

void psg_reader::SetRegister (uint_t reg, uint_t val) {
    this->set_reg(reg, val);
}