#include "ay_ym_file_play.h"

#ifdef MODULE_AY_YM_FILE_PLAY_ENABLED

int aym_base_parse::parse (std::shared_ptr<char> f) {
    (void)f;
    return EINVAL;
}

int aym_base_parse::get_len (std::shared_ptr<char> f, uint32_t &len) {
    int rv = 0;

    if ((rv = this->c.open_file(f)) != 0) {
        return rv;
    }

    if ((rv = this->get_len_psg(this->c, len)) != 0) {
        return rv;
    }

    if ((rv = this->c.close_file()) != 0) {
        return rv;
    }

    return rv;
}

#endif

