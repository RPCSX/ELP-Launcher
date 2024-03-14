#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace elp {
using VariantObject = nlohmann::json::object_t;
using VariantArray = nlohmann::json::array_t;
using VariantBool = nlohmann::json::boolean_t;
using VariantInt = nlohmann::json::number_integer_t;
using VariantFloat = nlohmann::json::number_float_t;
using VariantString = nlohmann::json::string_t;
using VariantValue = nlohmann::json;

struct InitializeRequest {
  std::string name;
  std::string version;
  std::vector<std::string> capabilities;
};

struct InitializeResponse {
  std::string name;
  std::string version;
  std::vector<std::string> capabilities;
};

struct ExitNotification {
  std::string reason;
};

struct LaunchRequest {
  std::string config;
  std::string executable;
  std::vector<std::string> args;
  std::string dataDirectory;
};

struct LaunchResponse {};

struct DeviceMessageNotification {
  std::string portId;
  std::string deviceType;
  std::string deviceId;
  std::string data;
};

enum class Severity {
  Info,
  Warning,
  Error,
};

struct LogMessageNotification {
  Severity severity;
  std::string content;
};

struct ShowViewRequest {
  std::string viewId;
  std::vector<std::string> params;
};

struct ShowViewResponse {
  std::string data;
};

enum class ErrorCode {
  MethodNotFound = -1,
  InvalidParam = -2,
  NotFound = -3,
};
} // namespace elp
