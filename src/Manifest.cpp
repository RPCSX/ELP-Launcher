#include "Manifest.hpp"

bool Manifest::match(const AlternativeRequirements &requirements) const {
  if (!requirements.name.empty()) {
    if (name != requirements.name) {
      return false;
    }
  }

  for (auto &capability : requirements.capabilities) {
    if (!capabilities.contains(capability)) {
      return false;
    }
  }

  for (auto &method : requirements.methods) {
    if (!contributes.methods.contains(method)) {
      return false;
    }
  }

  for (auto &entity : requirements.alternatives) {
    if (!contributes.alternatives.contains(entity)) {
      return false;
    }
  }

  if (requirements.hasSource) {
    if (source.empty() == *requirements.hasSource) {
      return false;
    }
  }

  if (requirements.hasLaunch) {
    if (launch.has_value() != *requirements.hasLaunch) {
      return false;
    }
    if (!launch->protocol.empty()) {
      return false;
    }
  }

  if (requirements.hasInstall) {
    if (install.has_value() != *requirements.hasInstall) {
      return false;
    }
  }

  if (requirements.hasDownload) {
    if (download.has_value() != *requirements.hasDownload) {
      return false;
    }
  }

  return true;
}

auto jsonGetKeyIfExists = [](const nlohmann::json &json, auto &result, const char *key) {
  if (auto it = json.find(key); it != json.end()) {
    result = std::remove_cvref_t<decltype(result)>(*it);
  } else {
    result = std::remove_cvref_t<decltype(result)>{};
  }
};


void from_json(const nlohmann::json &json, Manifest::Launch &object) {
  object.executable = json.at("executable");
  jsonGetKeyIfExists(json, object.args, "args");
  jsonGetKeyIfExists(json, object.interpreter, "interpreter");
  jsonGetKeyIfExists(json, object.protocol, "protocol");
  jsonGetKeyIfExists(json, object.transport, "transport");
  jsonGetKeyIfExists(json, object.configUi, "configUi");
}

void to_json(nlohmann::json &json, const Manifest::Launch &object) {
  json["executable"] = object.executable;
  json["args"] = object.args;
  json["interpreter"] = object.interpreter;
  json["protocol"] = object.protocol;
  json["transport"] = object.transport;
  json["configUi"] = object.configUi;
}

void from_json(const nlohmann::json &json, Manifest::Download &object) {
  object.url = json.at("url");
  jsonGetKeyIfExists(json, object.type, "type");
}

void to_json(nlohmann::json &json, const Manifest::Download &object) {
  json["url"] = object.url;
  json["type"] = object.type;
}

void from_json(const nlohmann::json &json, Manifest::Install &object) {
  object.path = json.at("path");
  jsonGetKeyIfExists(json, object.type, "type");
}

void to_json(nlohmann::json &json, const Manifest::Install &object) {
  json["path"] = object.path;
  json["type"] = object.type;
}

void from_json(const nlohmann::json &json, Manifest::Command &object) {
  object.id = json.at("id");
  jsonGetKeyIfExists(json, object.title, "title");
  jsonGetKeyIfExists(json, object.args, "args");
  jsonGetKeyIfExists(json, object.binding, "binding");
}

void to_json(nlohmann::json &json, const Manifest::Command &object) {
  json["title"] = object.title;
  json["args"] = object.args;
  json["binding"] = object.binding;
}

void from_json(const nlohmann::json &json, Manifest::ApiSet &object) {
  jsonGetKeyIfExists(json, object.alternatives, "alternatives");
  jsonGetKeyIfExists(json, object.methods, "methods");
  jsonGetKeyIfExists(json, object.packages, "packages");
}

void to_json(nlohmann::json &json, const Manifest::ApiSet &object) {
  json["alternatives"] = object.alternatives;
  json["methods"] = object.methods;
  json["packages"] = object.packages;
}

void from_json(const nlohmann::json &json, Manifest &object) {
  jsonGetKeyIfExists(json, object.source, "source");
  jsonGetKeyIfExists(json, object.path, "path");
  object.name = json.at("name");
  jsonGetKeyIfExists(json, object.branch, "branch");
  jsonGetKeyIfExists(json, object.version, "version");
  jsonGetKeyIfExists(json, object.versionTag, "versionTag");
  jsonGetKeyIfExists(json, object.tag, "tag");
  jsonGetKeyIfExists(json, object.license, "license");
  jsonGetKeyIfExists(json, object.description, "description");
  jsonGetKeyIfExists(json, object.icon, "icon");
  jsonGetKeyIfExists(json, object.launch, "launch");
  jsonGetKeyIfExists(json, object.download, "download");
  jsonGetKeyIfExists(json, object.install, "install");
  jsonGetKeyIfExists(json, object.commands, "commands");
  jsonGetKeyIfExists(json, object.capabilities, "capabilities");
  jsonGetKeyIfExists(json, object.ui, "ui");
  jsonGetKeyIfExists(json, object.contributes, "contributes");
  jsonGetKeyIfExists(json, object.dependencies, "dependencies");
}

void to_json(nlohmann::json &json, const Manifest &object) {
  json["source"] = object.source;
  json["path"] = object.path;
  json["name"] = object.name;
  json["branch"] = object.branch;
  json["version"] = object.version;
  json["versionTag"] = object.versionTag;
  json["tag"] = object.tag;
  json["license"] = object.license;
  json["description"] = object.description;
  json["icon"] = object.icon;
  if (object.launch) {
    json["launch"] = *object.launch;
  }
  if (object.download) {
    json["download"] = *object.download;
  }
  if (object.install) {
    json["install"] = *object.install;
  }
  json["commands"] = object.commands;
  json["capabilities"] = object.capabilities;
  json["ui"] = object.ui;
  json["contributes"] = object.contributes;
  json["dependencies"] = object.dependencies;
}
