#pragma once

#include "project_config.h"
#include "user_os.h"

#ifdef MODULE_AY_YM_FILE_PLAY_ENABLED

#include <stdint.h>
#include <errno.h>
#include <memory>

#include "mc_file_container.h"
#include "psg_reader.h"

class aym_base_parse : private psg_reader {
public:
    constexpr aym_base_parse() {}

public:
    int parse_psg (std::shared_ptr<char> fullFilePath);
    int get_len_psg (std::shared_ptr<char> fullFilePath, uint32_t &resultLong);

private:
    virtual int open_file (std::shared_ptr<char> f) = 0;
    virtual int close_file (void) = 0;
    virtual int get_len_file (uint32_t &len) = 0;
    virtual int set_offset (const uint32_t offset) = 0;
    virtual int read (uint8_t *buf, const uint32_t num) = 0;
    virtual int set_pwr_chip (const bool state) = 0;
    virtual int init_chip (void) = 0;
    virtual int sleep (const uint32_t num) = 0;
    virtual int set_reg (const uint8_t reg, const uint8_t data) = 0;

private:

};

#endif





