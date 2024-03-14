#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct AlternativeRequirements {
  std::string name;
  std::vector<std::string> capabilities;
  std::vector<std::string> alternatives;
  std::vector<std::string> methods;
  std::optional<bool> hasSource;
  std::optional<bool> hasLaunch;
  std::optional<bool> hasInstall;
  std::optional<bool> hasDownload;
};

static void from_json(const nlohmann::json &json, AlternativeRequirements &object) {
  if (auto it = json.find("name"); it != json.end()) {
    object.name = *it;
  }
  if (auto it = json.find("capabilities"); it != json.end()) {
    object.capabilities = *it;
  }
  if (auto it = json.find("alternatives"); it != json.end()) {
    object.alternatives = *it;
  }
  if (auto it = json.find("methods"); it != json.end()) {
    object.methods = *it;
  }
  if (auto it = json.find("hasSource"); it != json.end()) {
    object.hasSource = *it;
  }
  if (auto it = json.find("hasLaunch"); it != json.end()) {
    object.hasLaunch = *it;
  }
  if (auto it = json.find("hasInstall"); it != json.end()) {
    object.hasInstall = *it;
  }
  if (auto it = json.find("hasDownload"); it != json.end()) {
    object.hasDownload = *it;
  }
}

static void to_json(nlohmann::json &json, const AlternativeRequirements &object) {
  json["name"] = object.name;
  json["capabilities"] = object.capabilities;
  json["alternatives"] = object.alternatives;
  json["methods"] = object.methods;
  if (object.hasSource) {
    json["hasSource"] = *object.hasSource;
  }
  if (object.hasLaunch) {
    json["hasLaunch"] = *object.hasLaunch;
  }
  if (object.hasInstall) {
    json["hasInstall"] = *object.hasInstall;
  }
  if (object.hasDownload) {
    json["hasDownload"] = *object.hasDownload;
  }
}
