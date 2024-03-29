#pragma once

#include "Alternative.hpp"
#include "AlternativeGroup.hpp"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct AlternativeStorage {
  std::string kind;

  std::map<std::string, std::shared_ptr<AlternativeGroup>, std::less<>>
      alternativeGroups;

  std::map<std::string, std::shared_ptr<Alternative>, std::less<>>
      allAlternatives;

  std::vector<std::shared_ptr<Alternative>>
  findAlternatives(std::string_view groupId,
                   const AlternativeRequirements &requirements);

  void selectAlternative(std::string_view groupId,
                         std::shared_ptr<Alternative> alternative);

  bool addAlternativeToGroup(std::string_view groupId,
                             std::shared_ptr<Alternative> alternative);
  bool addAlternativeGroup(std::string groupId, std::string name);
  void removeAlternative(std::string_view groupId,
                         const std::shared_ptr<Alternative> &alternative);

  std::shared_ptr<Alternative>
  findAlternative(std::string_view name,
                  const AlternativeRequirements &requirements = {});

  std::shared_ptr<Alternative>
  findAlternativeOrResolve(Context &context, std::string_view name,
                           const AlternativeRequirements &requirements = {});
  std::shared_ptr<Alternative> findAlternativeById(std::string_view id);
  void
  removeAlternativeFromAll(const std::shared_ptr<Alternative> &alternative);
};
