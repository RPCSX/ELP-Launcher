#include "AlternativeStorage.hpp"
#include "Context.hpp"

std::vector<std::shared_ptr<Alternative>> AlternativeStorage::findAlternatives(
    std::string_view groupId, const AlternativeRequirements &requirements) {
  if (auto it = alternativeGroups.find(groupId);
      it != alternativeGroups.end()) {
    return it->second->find(requirements);
  }
  return {};
}

void AlternativeStorage::selectAlternative(
    std::string_view groupId, std::shared_ptr<Alternative> alternative) {
  if (auto it = alternativeGroups.find(groupId);
      it != alternativeGroups.end()) {
    it->second->select(std::move(alternative));
  }
}

bool AlternativeStorage::addAlternativeToGroup(
    std::string_view groupId, std::shared_ptr<Alternative> alternative) {
  auto groupIt = alternativeGroups.find(groupId);
  if (groupIt == alternativeGroups.end()) {
    return false;
  }

  groupIt->second->add(std::move(alternative));
  return true;
}

bool AlternativeStorage::addAlternativeGroup(std::string groupId,
                                             std::string name) {
  auto [it, inserted] =
      alternativeGroups.try_emplace(std::move(groupId), nullptr);
  if (inserted) {
    it->second = std::make_shared<AlternativeGroup>(
        Manifest{.name = name.empty() ? groupId : name});
    it->second->candidateRequirements.alternatives.push_back(groupId);
  }

  return inserted;
}

void AlternativeStorage::removeAlternative(
    std::string_view groupId, const std::shared_ptr<Alternative> &alternative) {
  auto groupIt = alternativeGroups.find(groupId);
  if (groupIt == alternativeGroups.end()) {
    return;
  }

  groupIt->second->remove(alternative);
}

std::shared_ptr<Alternative> AlternativeStorage::findAlternative(
    std::string_view name, const AlternativeRequirements &requirements) {
  if (auto it = alternativeGroups.find(name); it != alternativeGroups.end()) {
    auto result = it->second->getSelectedOrFind(requirements);
    return result.back();
  }

  return {};
}

std::shared_ptr<Alternative> AlternativeStorage::findAlternativeOrResolve(
    Context &context, std::string_view name,
    const AlternativeRequirements &requirements) {
  if (auto it = alternativeGroups.find(name); it != alternativeGroups.end()) {
    auto result = it->second->getSelectedOrFind(requirements);

    if (result.size() == 1) {
      return result.front();
    }

    MethodCallResult response;
    std::vector<Manifest> alternatives;
    alternatives.reserve(result.size());

    for (auto &candidate : result) {
      alternatives.push_back(candidate->manifest());
    }

    if (context.showView("alternative-resolver",
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

  if (context.showView("packages",
                       {{"requirements", requirements}, {"dialog", true}},
                       nullptr, false)) {
    return {};
  }

  if (!alternativeGroups.contains(name)) {
    return {};
  }

  return findAlternative(name, requirements);
}

std::shared_ptr<Alternative>
AlternativeStorage::findAlternativeById(std::string_view id) {
  if (auto it = allAlternatives.find(id); it != allAlternatives.end()) {
    return it->second;
  }
  return {};
}

void AlternativeStorage::removeAlternativeFromAll(
    const std::shared_ptr<Alternative> &alternative) {
  for (auto &[groupName, group] : alternativeGroups) {
    group->remove(alternative);
  }
  allAlternatives.erase(alternative->manifest().id());
}
