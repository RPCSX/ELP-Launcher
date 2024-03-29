#pragma once

#include "AlternativeRequirements.hpp"
#include <nlohmann/json_fwd.hpp>
#include <set>
#include <string>
#include <vector>

struct Manifest {
  struct Command {
    std::string id;
    std::string title;
    std::vector<nlohmann::json> args;
    std::string binding;
  };

  struct Launch {
    std::string executable;
    std::vector<std::string> args;
    std::string interpreter;
    std::string protocol;
    std::string transport;
    std::string configUi;
  };

  struct Download {
    std::string url;
    std::string type;
  };

  struct Install {
    std::string path;
    std::string type;
  };

  struct ApiSet {
    std::set<std::string> alternatives;
    std::set<std::string> views;
    std::set<std::string> methods;
    std::vector<Manifest> packages;
  };

  std::string source;
  std::string path;
  std::string name;
  std::string branch;
  std::string version;
  std::string versionTag;
  std::string tag;
  std::string license;
  std::string description;
  std::string icon;
  std::optional<Launch> launch;
  std::optional<Download> download;
  std::optional<Install> install;
  std::vector<Command> commands;
  std::set<std::string> capabilities;
  std::string ui;
  ApiSet contributes;
  ApiSet dependencies;

  std::string id() const {
    std::string result = displayId();
    if (!path.empty()) {
      result += '-';
      result += path;
    }
    return result;
  }

  std::string displayId() const {
    std::string result = name;
    if (!branch.empty()) {
      result += '-';
      result += branch;
    }
    if (!version.empty()) {
      result += '-';
      result += version;
    }
    if (!versionTag.empty()) {
      result += '-';
      result += versionTag;
    }
    if (!tag.empty()) {
      result += '-';
      result += tag;
    }
    return result;
  }

  bool match(const AlternativeRequirements &requirements) const;
};

void from_json(const nlohmann::json &json, Manifest::Launch &object);
void to_json(nlohmann::json &json, const Manifest::Launch &object);
void from_json(const nlohmann::json &json, Manifest::Download &object);
void to_json(nlohmann::json &json, const Manifest::Download &object);
void from_json(const nlohmann::json &json, Manifest::Install &object);
void to_json(nlohmann::json &json, const Manifest::Install &object);
void from_json(const nlohmann::json &json, Manifest::Command &object);
void to_json(nlohmann::json &json, const Manifest::Command &object);
void from_json(const nlohmann::json &json, Manifest::ApiSet &object);
void to_json(nlohmann::json &json, const Manifest::ApiSet &object);
void from_json(const nlohmann::json &json, Manifest &object);
void to_json(nlohmann::json &json, const Manifest &object);
