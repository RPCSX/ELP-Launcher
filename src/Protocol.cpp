#include "Protocol.hpp"

struct ElpProtocol : public Protocol {
    using Protocol::Protocol;
};

std::unique_ptr<Protocol> createProtocol(std::string_view name, std::unique_ptr<Transport> transport) {
    if (name == "ELP") {
        return std::make_unique<ElpProtocol>(std::move(transport));
    }

    return nullptr;
}
