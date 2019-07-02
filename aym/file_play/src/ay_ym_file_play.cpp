#include "ay_ym_file_play.h"

#ifdef MODULE_AY_YM_FILE_PLAY_ENABLED

int aym_base_parse::parse (std::shared_ptr<char> f) {
    (void)f;
    return EINVAL;
}

int aym_base_parse::get_len (std::shared_ptr<char> f, uint32_t &len) {
    return this->get_len_pt3(f, len);
}

int aym_base_parse::parse_psg (std::shared_ptr<char> f) {
    (void)f;
   return EINVAL;
}

int aym_base_parse::get_len_psg(std::shared_ptr<char> f, uint32_t &len) {
    (void)f;
    (void)len;
    
    //auto p = Formats::Chiptune::PSG::Parse(this->c, *this);

    return 0;
}

int aym_base_parse::parse_pt3 (std::shared_ptr<char> f) {
    (void)f;
    return EINVAL;
}

int aym_base_parse::get_len_pt3 (std::shared_ptr<char> f, uint32_t &len) {
    (void)f;
    (void)len;

    auto p = Formats::Chiptune::PT3::Parse(this->c, *this);

    return 0;
}

#endif

