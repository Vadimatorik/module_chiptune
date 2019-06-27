#include "ay_ym_file_play.h"

#ifdef MODULE_AY_YM_FILE_PLAY_ENABLED

int aym_base_parse::parse_psg (std::shared_ptr<char> f) {
    (void)f;
    return 0;
}

int aym_base_parse::get_len_psg(std::shared_ptr<char> f, uint32_t &len) {
    mc_file_container c;
    auto p = Formats::Chiptune::PSG::Parse(c, *this);

    (void)f;
    (void)len;

    return 0;
}

#endif

