#include "Context.hpp"
#include <fstream>
#include <iterator>
#include <mutex>

inline std::pair<std::string_view, std::string_view>
splitOnce(std::string_view string, std::string_view separator) {
  auto pos = string.find(separator);

  if (pos == std::string_view::npos) {
    return {string, {}};
  }

  return {string.substr(0, pos), string.substr(pos + separator.length())};
}

inline std::pair<std::string_view, std::string_view>
splitOnce(std::string_view string, char separator) {
  auto pos = string.find(separator);

  if (pos == std::string_view::npos) {
    return {string, {}};
  }

  return {string.substr(0, pos), string.substr(pos + 1)};
}

void Context::addAlternative(std::shared_ptr<Alternative> alternative) {
  auto altId = alternative->manifest().id();
  if (auto [it, inserted] = allAlternatives.try_emplace(altId, alternative);
      !inserted) {
    // FIXME: merge alternatives
  }

  for (auto &groupId : alternative->manifest().contributes.alternatives) {
    auto groupIt = alternativeGroups.find(groupId);
    if (groupIt == alternativeGroups.end()) {
      std::fprintf(
          stderr,
          "Ignoring adding alternative '%s' to non existing group '%s'\n",
          alternative->manifest().id().c_str(), groupId.c_str());
      continue;
    }

    groupIt->second->add(alternative);
  }

  for (auto &ext : alternative->manifest().contributes.methods) {
    methodHandlers.addAlternativeGroup(ext, "");
    methodHandlers.addAlternativeToGroup(ext, alternative);
  }

  for (auto &ext : alternative->manifest().contributes.views) {
    views.addAlternativeGroup(ext, "");
    views.addAlternativeToGroup(ext, alternative);
  }
}

std::error_code Context::showView(std::string_view name, MethodCallArgs args,
                                  MethodCallResult *response, bool tryResolve) {
  std::shared_ptr<Alternative> alt;

  if (tryResolve) {
    alt = views.findAlternativeOrResolve(*this, name, {});
  } else {
    alt = views.findAlternative(name, {});
  }

  if (alt != nullptr) {
    auto ec = activate(alt);
    if (ec && ec != std::errc::already_connected) {
      return ec;
    }

    alt->callMethod(*this, "view/show",
                    {{"id", name}, {"args", std::move(args)}},
                    createShowErrorFn());
    return {};
  }

  return std::make_error_code(std::errc::no_such_file_or_directory);
}

std::error_code Context::hideView(std::string_view name) {
  if (auto alt = findAlternative(name, {})) {
    if (!isActive(alt)) {
      return std::make_error_code(std::errc::not_connected);
    }

    alt->callMethod(*this, "view/hide", {{"id", name}}, createShowErrorFn());
    return {};
  }

  return std::make_error_code(std::errc::no_such_file_or_directory);
}

void Context::sendNotification(std::string_view name,
                               const NotificationArgs &args) {
  auto handlersIt = notificationHandlers.find(name);
  if (handlersIt == notificationHandlers.end()) {
    return;
  }

  for (auto &handler : handlersIt->second) {
    handler(args);
  }
}

Connection Context::createNotificationHandler(
    std::string name,
    std::move_only_function<void(const NotificationArgs &)> handler) {
  auto &container = notificationHandlers[std::move(name)];
  container.push_back(std::move(handler));
  auto it = std::prev(container.end());

  return {[it = std::move(it), &container] { container.erase(it); }};
}

void Context::callMethod(
    std::string_view name, const AlternativeRequirements &requirements,
    const MethodCallArgs &args,
    std::move_only_function<void(const MethodCallResult &)> responseHandler) {
  if (auto alternative = methodHandlers.findAlternative(name, requirements)) {
    alternative->callMethod(*this, name, args, std::move(responseHandler));
  } else {
    responseHandler({{"error", elp::ErrorCode::MethodNotFound}});
  }
}

void Context::addPackage(const Url &source, const Url &path,
                         Manifest &&manifest) {
  if (findAlternativeById(manifest.id())) {
    // TODO: merge manifests
    return;
  }

  for (auto ext : manifest.contributes.packages) {
    if (ext.install) {
      auto extPath = Url::makeFromRelative(path, ext.install->path);
      if (!ext.icon.empty()) {
        ext.icon = Url::makeFromRelative(path, ext.icon).toString();
      }
      addPackage(path, std::move(extPath), std::move(ext));
    }
  }

  manifest.source = source.toString();
  manifest.path = path.toString();

  auto ui = manifest.ui;
  auto id = manifest.id();
  auto alt = std::make_shared<Alternative>(std::move(manifest));
  addAlternative(alt);

  sendNotification("packages/change", {
                                          {"add", nlohmann::json::array({id})},
                                      });

  if (!ui.empty()) {
    Url::makeFromRelative(path, ui).asyncGet().then([=](QByteArray bytes) {
      IdToSchemaMap uiIdMap;
      auto uiSchemaNode = parseUiFile(uiIdMap, bytes);
      alt->setUiSchema(std::move(uiSchemaNode), std::move(uiIdMap));
    });
  }
}

std::error_code Context::activate(std::string_view id) {
  auto alt = findAlternativeById(id);
  if (alt == nullptr) {
    return std::make_error_code(std::errc::no_such_file_or_directory);
  }
  return activate(alt);
}

std::error_code Context::deactivate(std::string_view id) {
  auto alt = findAlternativeById(id);
  if (alt == nullptr) {
    return std::make_error_code(std::errc::no_such_file_or_directory);
  }

  return deactivate(alt);
}

bool Context::isActive(std::string_view id) {
  auto alt = findAlternativeById(id);
  if (alt == nullptr) {
    return false;
  }

  return isActive(alt);
}

std::error_code Context::activate(const std::shared_ptr<Alternative> &alt) {
  if (activeList.insert(alt).second) {
    auto ec = alt->activate(*this);
    if (ec) {
      activeList.erase(alt);
    }
    return ec;
  }

  return std::make_error_code(std::errc::already_connected);
}

std::error_code Context::deactivate(const std::shared_ptr<Alternative> &alt) {
  if (activeList.erase(alt)) {
    auto ec = alt->deactivate(*this);
    if (ec) {
      activeList.insert(alt);
    }
    return ec;
  }

  return std::make_error_code(std::errc::not_connected);
}

bool Context::isActive(const std::shared_ptr<Alternative> &alt) {
  return activeList.contains(alt);
}

void Context::loadSettings() {
  if (std::ifstream f{configPath / "settings.json"}) {
    try {
      f >> settings;
    } catch (...) {
    }
  }

  auto &packages = getSettings("installed-packages", Settings::array());
  for (auto &package : packages.get<std::set<std::string>>()) {
    // FIXME: should be load package
    updatePackageSource(Url(package));
  }
}
void Context::saveSettings() {
  if (std::ofstream f{configPath / "settings.json"}) {
    f << settings;
  }
}

Settings &Context::getSettings(std::string_view path, Settings defValue) {
  auto result = &settings;
  while (!path.empty()) {
    auto [chunk, rest] = splitOnce(path, '/');
    path = rest;

    if (!path.empty()) {
      result = &*(*result).emplace(chunk, Settings::object_t{}).first;
    } else {
      result = &*(*result).emplace(chunk, std::move(defValue)).first;
    }
  }
  return *result;
}

Settings &Context::getSettingsFor(const std::shared_ptr<Alternative> &alt,
                                  std::string_view path, Settings defValue) {
  if (path.empty()) {
    return *settings.emplace(alt->manifest().id(), std::move(defValue)).first;
  }

  auto result =
      &*settings.emplace(alt->manifest().id(), Settings::object_t{}).first;
  while (!path.empty()) {
    auto [chunk, rest] = splitOnce(path, '/');
    path = rest;

    if (!path.empty()) {
      result = &*(*result).emplace(chunk, Settings::object_t{}).first;
    } else {
      result = &*(*result).emplace(chunk, std::move(defValue)).first;
    }
  }
  return *result;
}

void Context::editPackageSources(std::span<const Url> add,
                                 std::span<const Url> remove) {
  auto &sources = getSettings("package-sources", Settings::array());
  auto sourcesSet = sources.get<std::set<std::string>>();
  std::vector<std::string> removeList;
  std::vector<std::string> addList;

  for (auto &url : remove) {
    auto source = url.toString();
    if (sourcesSet.erase(source) == 0) {
      continue;
    }

    removeList.push_back(std::move(source));
  }

  for (auto &url : add) {
    auto source = url.toString();
    if (!sourcesSet.insert(source).second) {
      continue;
    }

    addList.push_back(std::move(source));
  }

  if (!removeList.empty() || !addList.empty()) {
    sources = sourcesSet;
    sendNotification("package-sources/change",
                     {{"add", addList}, {"remove", removeList}});

    for (auto add : addList) {
      updatePackageSource(Url(add));
    }
  }
}

void Context::installPackage(std::string_view id) {
  // FIXME: support remote packages and archives
  // FIXME: unpack archive to dataPath
  auto alt = findAlternativeById(id);
  if (alt == nullptr) {
    // FIXME: report error
    return;
  }

  auto optInstall = alt->manifest().install;
  if (!optInstall) {
    // FIXME: report error
    return;
  }

  auto path = alt->manifest().path;

  auto &packages = getSettings("installed-packages", Settings::array());
  auto packagesSet = packages.get<std::set<std::string>>();
  if (packagesSet.insert(path).second) {
    updatePackageSource(Url(path));
    packages = packagesSet;
  }
}

void Context::updatePackageSource(const Url &url) {
  // FIXME: fetch packages list only
  auto completeUrl = [&] {
    if (url.isLocalPath()) {
      auto path = url.toLocalPath();
      if (std::filesystem::is_directory(path)) {
        return Url(path / "manifest.json");
      }
    }

    return url;
  }();

  auto extension =
      std::filesystem::path(completeUrl.underlying().fileName().toStdString())
          .extension();
  if (extension != ".json") {
    std::fprintf(stderr, "Unsupported package type '%s'\n", extension.c_str());
    return;
  }

  completeUrl.asyncGet()
      .then(QtFuture::Launch::Async,
            [=, this](QByteArray bytes) {
              try {
                if (!bytes.isEmpty()) {
                  auto manifestJson = nlohmann::json::parse(std::string_view(
                      reinterpret_cast<char *>(bytes.data()), bytes.size()));
                  {
                    std::lock_guard lock(mutex);
                    addPackage(url, url, manifestJson.get<Manifest>());
                  }
                }
              } catch (const std::exception &ex) {
                std::fprintf(
                    stderr,
                    "failed to parse manifest from package '%s': %s. "
                    "manifest: %s\n",
                    url.toString().c_str(), ex.what(),
                    std::string(
                        std::string_view(reinterpret_cast<char *>(bytes.data()),
                                         bytes.size()))
                        .c_str());
              }
            })
      .onFailed([=](const std::exception &ex) {
        std::fprintf(stderr, "failed to fetch package source from '%s': %s\n",
                     url.toString().c_str(), ex.what());
      });
}

void Context::updatePackageSources() {
  for (auto &source : getSettings("package-sources", Settings::array())) {
    updatePackageSource(Url(source.get<std::string>()));
  }
}
