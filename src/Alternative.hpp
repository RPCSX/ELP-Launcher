#pragma once
#include "ELP.hpp"
#include "Manifest.hpp"
#include "UiFile.hpp"
#include <mutex>
#include <nlohmann/json.hpp>

using MethodCallResult = nlohmann::json;
using MethodCallArgs = nlohmann::json;
using NotificationArgs = nlohmann::json;

class Context;
class Alternative {
  Manifest m_manifest;
  IdToSchemaMap m_uiSchemaMap;
  std::unique_ptr<SchemaNode> m_uiRoot;
  std::mutex m_mutex;

public:
  Alternative(Manifest manifest) : m_manifest(std::move(manifest)) {}
  virtual ~Alternative() = default;

  void setUiSchema(std::unique_ptr<SchemaNode> uiRoot,
                   IdToSchemaMap uiSchemaMap) {
    std::lock_guard lock(m_mutex);
    m_uiRoot = std::move(uiRoot);
    m_uiSchemaMap = std::move(uiSchemaMap);
  }

  SchemaNode *findUiSchema(std::string_view id) {
    std::lock_guard lock(m_mutex);
    if (auto it = m_uiSchemaMap.find(id); it != m_uiSchemaMap.end()) {
      return it->second;
    }
    return nullptr;
  }

  virtual void
  callMethod(Context &context, std::string_view name, MethodCallArgs args,
             std::move_only_function<void(MethodCallResult)> responseHandler) {
    responseHandler({{"error", elp::ErrorCode::MethodNotFound}});
  }
  virtual void handleNotification(Context &context, std::string_view name,
                                  NotificationArgs args) {}
  virtual std::error_code activate(Context &context) { return {}; }
  virtual std::error_code deactivate(Context &context) { return {}; }

  bool match(const AlternativeRequirements &requirements) const {
    return manifest().match(requirements);
  }

  virtual const Manifest &manifest() const { return m_manifest; }
};
