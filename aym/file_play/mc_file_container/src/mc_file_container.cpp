#include "mc_file_container.h"

Binary::Container::Ptr mc_file_container::GetSubcontainer(std::size_t offset, std::size_t size) const {
    (void)offset;
    (void)size;
    auto p = std::make_shared<mc_file_container>();
    return p;
}

const void* mc_file_container::Start() const {
    return nullptr;
}

std::size_t mc_file_container::Size() const {
    return 0;
}

