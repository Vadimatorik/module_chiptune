#include "psg.h"

class psg_reader : public Formats::Chiptune::PSG::Builder {
public:
    constexpr psg_reader () {};

private:
    virtual int sleep (const uint32_t num) = 0;
    virtual int set_reg (const uint8_t reg, const uint8_t data) = 0;

private:
    void AddChunks(std::size_t count);
    void SetRegister(uint_t reg, uint_t val);

};