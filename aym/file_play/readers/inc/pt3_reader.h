#include "mc_file_container.h"

class pt3_reader  {
public:
    constexpr pt3_reader () {};

public:
    int parse_pt3 (mc_file_container &c);
    int get_len_pt3 (mc_file_container &c, uint32_t &len);

private:
    virtual int sleep (uint32_t num) = 0;
    virtual int set_reg (uint8_t reg, uint8_t data) = 0;

};