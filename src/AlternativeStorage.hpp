#pragma once

#include "Alternative.hpp"
#include "AlternativeGroup.hpp"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct AlternativeStorage {
  std::map<std::string, std::shared_ptr<AlternativeGroup>, std::less<>>
      alternativeGroups;

  std::map<std::string, std::shared_ptr<Alternative>, std::less<>>
      allAlternatives;

  std::vector<std::shared_ptr<Alternative>>
  findAlternatives(std::string_view groupId,
                   const AlternativeRequirements &requirements) {
    if (auto it = alternativeGroups.find(groupId);
        it != alternativeGroups.end()) {
      return it->second->find(requirements);
    }
    return {};
  }

  void selectAlternative(std::string_view groupId,
                         std::shared_ptr<Alternative> alternative) {
    if (auto it = alternativeGroups.find(groupId);
        it != alternativeGroups.end()) {
      it->second->select(std::move(alternative));
    }
  }

  bool addAlternativeToGroup(std::string_view groupId,
                             std::shared_ptr<Alternative> alternative) {
    auto groupIt = alternativeGroups.find(groupId);
    if (groupIt == alternativeGroups.end()) {
      return false;
    }

    groupIt->second->add(std::move(alternative));
    return true;
  }

  bool addAlternativeGroup(std::string groupId, std::string name) {
    auto [it, inserted] =
        alternativeGroups.try_emplace(std::move(groupId), nullptr);
    if (inserted) {
      it->second = std::make_shared<AlternativeGroup>(
          Manifest{.name = name.empty() ? groupId : name});
      it->second->candidateRequirements.alternatives.push_back(groupId);
    }

    return inserted;
  }

  void removeAlternative(std::string_view groupId,
                         const std::shared_ptr<Alternative> &alternative) {
    auto groupIt = alternativeGroups.find(groupId);
    if (groupIt == alternativeGroups.end()) {
      return;
    }

    groupIt->second->remove(alternative);
  }

  void
  removeAlternativeFromAll(const std::shared_ptr<Alternative> &alternative) {
    for (auto &[groupName, group] : alternativeGroups) {
      group->remove(alternative);
    }
    allAlternatives.erase(alternative->manifest().id());
  }
};
