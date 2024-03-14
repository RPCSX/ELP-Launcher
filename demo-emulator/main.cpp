#include <cstddef>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

using namespace std::string_view_literals;

struct Transport {
  virtual ~Transport() = default;
  virtual std::error_code send(std::span<const std::byte> data) = 0;
  virtual std::error_code receive(std::vector<std::byte> &result) = 0;
};

struct StdioTransport : Transport {
  std::error_code send(std::span<const std::byte> data) override {
    std::fprintf(stdout, "Content-Length: %zu\r\n\r\n", data.size());
    auto result = std::fwrite(data.data(), 1, data.size(), stdout);
    if (result != data.size()) {
      return std::make_error_code(std::errc::io_error);
    }
    return {};
  }

  std::error_code receive(std::vector<std::byte> &result) override {
    char header[512];
    std::size_t size = 0;
    std::fscanf(stdin, "Content-Length: %zu\r\n\r\n", &size);
    result.resize(size);

    std::size_t offset = 0;
    if (std::fread(result.data(), 1, size, stdin) != size) {
      return std::make_error_code(std::errc::io_error);
    }
    return {};
  }
};

class Protocol {
public:
  Protocol() = default;
  Protocol(std::unique_ptr<Transport> transport)
      : pTransport(std::move(transport)) {}

  virtual ~Protocol() = default;

  Transport *transport() const { return pTransport.get(); }

private:
  std::unique_ptr<Transport> pTransport;
};

using json = nlohmann::json;

class ElpProtocol : Protocol {
public:
  void addMethodHandler(std::string method,
                        std::move_only_function<json(json)> handler) {
    methodHandlers[std::move(method)] = std::move(handler);
  }

  std::error_code sendMethodCall(std::string_view method, json params) {
    auto message =
        json{{"json-rpc", "2.0"}, {"method", method}, {"params", params}}
            .dump();

    return transport()->send(
        {reinterpret_cast<const std::byte *>(message.data()), message.size()});
  }

private:
  std::map<std::string, std::move_only_function<json(json)>, std::less<>>
      methodHandlers;
  std::vector<std::byte> receiveBuffer;

  std::jthread thread{[this](std::stop_token token) {
    while (!token.stop_requested()) {
      if (auto error = transport()->receive(receiveBuffer)) {
        // TODO: process error
        return;
      }

      auto message = json(
          std::string_view(reinterpret_cast<const char *>(receiveBuffer.data()),
                           receiveBuffer.size()));
      std::fprintf(stderr, "%s\n", message.dump().c_str());
      return;
    }
  }};
};

static std::vector<std::string> handleQueryGpuDevices() {
  return {
      "Dummy GPU",
      "Dummy GPU 2",
  };
}

static std::vector<std::string> handleQueryPadDevices() {
  return {
      "Dummy PAD",
      "Dummy PAD 2",
      "Dummy PAD 3",
  };
}

int main(int argc, const char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    if (argv[i] == "--elp"sv) {
    }
  }
}
