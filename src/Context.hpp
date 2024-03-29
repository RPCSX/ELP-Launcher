#pragma once

#include "Alternative.hpp"
#include "AlternativeStorage.hpp"
#include "Url.hpp"
#include <filesystem>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using Settings = nlohmann::json;

class [[nodiscard]] Connection {
  std::move_only_function<void()> m_destroy;

public:
  Connection() = default;
  Connection(std::move_only_function<void()> destroy)
      : m_destroy(std::move(destroy)) {}
  ~Connection() { destroy(); }
  Connection(Connection &&) = default;
  Connection &operator=(Connection &&) = default;

  void destroy() {
    if (auto fn = std::exchange(m_destroy, nullptr)) {
      fn();
    }
  }
};

using Connections = std::vector<Connection>;

struct Context : AlternativeStorage {
  std::filesystem::path configPath;
  std::filesystem::path dataPath;
  Settings settings;
  std::mutex mutex;

  AlternativeStorage methods;
  AlternativeStorage views;

  std::set<std::shared_ptr<Alternative>> activeList;

  std::map<std::string,
           std::list<std::move_only_function<void(const NotificationArgs &)>>,
           std::less<>>
      notificationHandlers;

  Context();

  void addAlternative(std::shared_ptr<Alternative> alternative);
  void selectAlternative(std::string_view kind, std::string_view groupId,
                         std::shared_ptr<Alternative> alternative);

  std::error_code showView(std::string_view name, MethodCallArgs args = {},
                           MethodCallResult *response = nullptr,
                           bool tryResolve = true);

  std::error_code hideView(std::string_view name);

  void addPackage(const Url &source, const Url &path, Manifest &&manifest);

  void sendNotification(std::string_view name, const NotificationArgs &args);

  Connection createNotificationHandler(
      std::string name,
      std::move_only_function<void(const NotificationArgs &)> handler);

  void callMethod(
      std::string_view name, const AlternativeRequirements &requirements,
      const MethodCallArgs &args,
      std::move_only_function<void(const MethodCallResult &)> responseHandler);

  auto createShowErrorFn(std::move_only_function<void(const MethodCallResult &)>
                             responseHandler = nullptr) {
    auto impl = [this, responseHandler = std::move(responseHandler)](
                    const MethodCallResult &result) mutable {
      if (result.contains("error")) {
        if (auto ec = showView("error", result["error"])) {
          std::printf("show error failed: %s, args: %s\n", ec.message().c_str(),
                      result["error"].dump().c_str());
        }
      }

      if (responseHandler) {
        responseHandler(result);
      }
    };

    return impl;
  }

  void callMethodShowErrors(
      std::string_view name, const AlternativeRequirements &requirements,
      const MethodCallArgs &args,
      std::move_only_function<void(const MethodCallResult &)> responseHandler =
          nullptr) {
    callMethod(name, requirements, args,
               createShowErrorFn(std::move(responseHandler)));
  }

  std::error_code activate(std::string_view id);
  std::error_code deactivate(std::string_view id);
  bool isActive(std::string_view id);

  std::error_code activate(const std::shared_ptr<Alternative> &alt);
  std::error_code deactivate(const std::shared_ptr<Alternative> &alt);
  bool isActive(const std::shared_ptr<Alternative> &alt);

  void loadSettings();
  void saveSettings();
  Settings &getSettings(std::string_view path, Settings defValue = nullptr);
  Settings &getSettingsFor(const std::shared_ptr<Alternative> &alt,
                           std::string_view path, Settings defValue = nullptr);
  void editPackageSources(std::span<const Url> add,
                          std::span<const Url> remove);
  void installPackage(std::string_view id);
  void updatePackageSource(const Url &url);
  void updatePackageSources();
};
