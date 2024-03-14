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

class PackageAlternative : public Alternative {
public:
  using Alternative::Alternative;
  std::error_code activate(Context &context, std::string_view role,
                           MethodCallArgs args,
                           MethodCallResult *response) override {
    return {};
  }
  std::error_code deactivate(Context &context, std::string_view role) override {
    return {};
  }
};

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

  for (auto ext : alternative->manifest().contributes.methods) {
    auto group = "method/" + std::string(ext);
    addAlternativeGroup(group, "");
    addAlternativeToGroup(group, alternative);
  }
}

std::shared_ptr<Alternative>
Context::findAlternative(std::string_view name,
                         const AlternativeRequirements &requirements,
                         bool tryResolve) {
  if (auto it = alternativeGroups.find(name); it != alternativeGroups.end()) {
    auto result = it->second->getSelectedOrFind(requirements);

    if (result.size() == 1) {
      return result.front();
    }

    if (!tryResolve) {
      return result.back();
    }

    MethodCallResult response;
    std::vector<Manifest> alternatives;
    alternatives.reserve(result.size());

    for (auto &candidate : result) {
      alternatives.push_back(candidate->manifest());
    }

    if (showView("alternative-resolver",
                 {
                     {"alternatives", alternatives},
                     {"requirements", requirements},
                     {"groupId", name},
                 },
                 &response, false)) {
      return {};
    }

    if (response.is_number_integer()) {
      auto index = response.get<int>();
      if (index >= 0 && index < result.size()) {
        return result[index];
      }
    }

    return findAlternative(name, requirements);
  }

  if (!tryResolve) {
    return {};
  }

  if (showView("packages", {{"requirements", requirements}, {"dialog", true}},
               nullptr, false)) {
    return {};
  }

  if (!alternativeGroups.contains(name)) {
    return {};
  }

  return findAlternative(name, requirements);
}

std::error_code Context::showView(std::string_view name, MethodCallArgs args,
                                  MethodCallResult *response, bool tryResolve) {
  auto role = "view/" + std::string(name);

  if (auto alt = findAlternative(role, {}, tryResolve)) {
    return alt->activate(*this, role, std::move(args), response);
  }

  return std::make_error_code(std::errc::no_such_file_or_directory);
}

std::error_code Context::hideView(std::string_view name) {
  auto role = "view/" + std::string(name);

  if (auto alt = findAlternative(role, {}, false)) {
    return alt->deactivate(*this, role);
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
  // TODO: split methods and packages

  if (auto alternative =
          findAlternative("method/" + std::string(name), requirements)) {
    alternative->callMethod(*this, name, args, std::move(responseHandler));
  } else {
    responseHandler({{"error", elp::ErrorCode::MethodNotFound}});
  }
}

std::shared_ptr<Alternative> Context::findAlternativeById(std::string_view id) {
  if (auto it = allAlternatives.find(id); it != allAlternatives.end()) {
    return it->second;
  }
  return {};
}

void Context::addPackage(const Url &source, const Url &path,
                         Manifest &&manifest) {
  if (findAlternativeById(manifest.id())) {
    // TODO: merge manifests
    return;
  }

  for (auto ext : manifest.contributes.packages) {
    if (ext.install) {
      auto extPath = Url::makeFromRelative(Url(path), ext.install->path);
      addPackage(path, std::move(extPath), std::move(ext));
    }
  }

  manifest.source = source.toString();
  manifest.path = path.toString();

  auto ui = manifest.ui;
  auto id = manifest.id();
  auto alt = std::make_shared<PackageAlternative>(std::move(manifest));
  addAlternative(alt);

  sendNotification("packages/change", {
                                          {"add", nlohmann::json::array({id})},
                                      });


  if (!ui.empty()) {
    Url::makeFromRelative(path, ui).asyncGet().then(
    [=](QByteArray bytes) {
      IdToSchemaMap uiIdMap;
      auto uiSchemaNode = parseUiFile(uiIdMap, bytes);
      alt->setUiSchema(std::move(uiSchemaNode), std::move(uiIdMap));
    }
  );
  }
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

Settings &Context::getSettingsFor(const std::shared_ptr<Alternative> &alt, std::string_view path, Settings defValue) {
  if (path.empty()) {
    return *settings.emplace(alt->manifest().id(), std::move(defValue)).first;
  }

  auto result = &*settings.emplace(alt->manifest().id(), Settings::object_t{}).first;
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
