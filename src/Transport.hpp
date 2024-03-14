#pragma once

#include <memory>
#include <span>
#include <string_view>
#include <system_error>

struct Transport {
  virtual ~Transport() = default;
  virtual std::errc sendMessage(std::span<const char> bytes) = 0;
  virtual void flush() = 0;
  virtual std::errc receive(std::span<char> &dest) = 0;
  virtual std::errc receiveErrorStream(std::span<char> &dest) = 0;
};

class Server;
std::unique_ptr<Transport> createTransport(std::string_view name, Server *executable);
