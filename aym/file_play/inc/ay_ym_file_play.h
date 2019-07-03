#pragma once

#include "project_config.h"
#include "user_os.h"

#ifdef MODULE_AY_YM_FILE_PLAY_ENABLED

#include <stdint.h>
#include <errno.h>
#include <memory>

#include "mc_file_container.h"
#include "psg_reader.h"
#include "pt3_reader.h"

class aym_base_parse : public psg_reader, pt3_reader {
public:
    constexpr aym_base_parse() {}

public:
    int parse (std::shared_ptr<char> f);
    int get_len (std::shared_ptr<char> f, uint32_t &len);

private:
    virtual int set_pwr_chip (const bool state) = 0;
    virtual int init_chip (void) = 0;
    virtual int sleep (const uint32_t num) = 0;
    virtual int set_reg (const uint8_t reg, const uint8_t data) = 0;

private:
    mc_file_container c;

};

#endif





