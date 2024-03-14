#include "Transport.hpp"
#include "Server.hpp"
#include <sstream>

class StdioTransport : public Transport {
public:
  StdioTransport(std::ostream &in, std::istream &out, std::istream &err)
      : in(&in), out(&out), err(&err) {}

  std::errc sendMessage(std::span<const char> bytes) override {
    std::ostringstream header;
    header << "Content-Length: " << bytes.size() << "\r\n";
    header << "\r\n";
    auto headerString = std::move(header).str();
    in->write(headerString.data(), headerString.size());
    in->write(bytes.data(), bytes.size());

    if (!*in) {
      return std::errc::broken_pipe;
    }

    return {};
  }

  void flush() override { in->flush(); }

  std::errc receive(std::span<char> &dest) override { return {}; }
  std::errc receiveErrorStream(std::span<char> &dest) override { return {}; }

private:
  std::ostream *in;
  std::istream *out;
  std::istream *err;
};

std::unique_ptr<Transport> createTransport(std::string_view name,
                                           Server *executable) {
  if (name == "stdio") {
    return std::make_unique<StdioTransport>(executable->getStdin(),
                                            executable->getStdout(),
                                            executable->getStderr());
  }

  return nullptr;
}
