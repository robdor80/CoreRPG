#include <corerpg/version.hpp>

namespace corerpg {

std::string_view library_version() noexcept {
    return CORERPG_VERSION_STRING;
}

SemanticVersion current_core_version() {
    return SemanticVersion::parse(library_version()).value();
}

} // namespace corerpg
