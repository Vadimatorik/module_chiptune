#include "container.h"

class mc_file_container : public Binary::Container {
public:
    Ptr GetSubcontainer(std::size_t offset, std::size_t size) const;
    const void* Start() const;
    std::size_t Size() const;

};