#include "ucilib/types.hpp"

namespace uci {

namespace {

class uci_error_category final : public std::error_category
{
public:
    auto name() const noexcept -> char const* override
    {
        return "uci";
    }

    auto message(int ev) const -> std::string override
    {
        switch (static_cast<errc>(ev)) {
        case errc::engine_not_running:
            return "engine is not running";
        case errc::uci_handshake_timeout:
            return "timeout waiting for uciok";
        case errc::ready_timeout:
            return "timeout waiting for readyok";
        case errc::write_failed:
            return "failed to write to engine stdin";
        case errc::engine_crashed:
            return "engine process crashed";
        case errc::invalid_argument:
            return "invalid argument";
        }
        return "unknown uci error";
    }
};

} // namespace

auto uci_category() -> std::error_category const&
{
    static uci_error_category const cat;
    return cat;
}

auto make_error_code(errc e) -> std::error_code
{
    return {static_cast<int>(e), uci_category()};
}

} // namespace uci
