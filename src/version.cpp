#include <corerpg/version.hpp>

namespace corerpg {

std::string_view library_version() noexcept {
    return CORERPG_VERSION_STRING;
}

} // namespace corerpg
