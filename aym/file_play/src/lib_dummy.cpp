#include <formats/chiptune.h>

namespace Formats {
namespace Chiptune {

Container::Ptr CreateCalculatingCrcContainer (Binary::Container::Ptr data, std::size_t offset, std::size_t size) {
    (void)data;
    (void)offset;
    (void)size;
    return nullptr;
}

}
}