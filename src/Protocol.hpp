#pragma once
#include "Transport.hpp"
#include <memory>

class Protocol {
public:
  virtual ~Protocol() = default;
  Protocol(std::unique_ptr<Transport> t) : pTransport(std::move(t)) {}

//   void setMethodHandler(std::string method, std::function<void(const VariantValue &value)>) {}
//   void setNotificationHandler() {}

private:
  std::unique_ptr<Transport> pTransport;
};

std::unique_ptr<Protocol> createProtocol(std::string_view name, std::unique_ptr<Transport> transport);
