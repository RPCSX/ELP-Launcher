#pragma once

#include "Alternative.hpp"
#include "AlternativeRequirements.hpp"
#include <memory>
#include <string>
#include <vector>

struct AlternativeGroup final : Alternative {
  std::vector<std::shared_ptr<Alternative>> candidates;
  std::shared_ptr<Alternative> selected;
  std::string name;
  AlternativeRequirements candidateRequirements;

  using Alternative::Alternative;

  std::vector<std::shared_ptr<Alternative>>
  getSelectedOrFind(const AlternativeRequirements &requirements) {
    if (selected == nullptr || !selected->match(requirements)) {
      return find(requirements);
    }

    return {selected};
  }

  std::vector<std::shared_ptr<Alternative>>
  find(const AlternativeRequirements &requirements);

  void select(std::shared_ptr<Alternative> selection) { selected = selection; }

  void add(std::shared_ptr<Alternative> alternative) {
    if (alternative == nullptr) {
      std::fprintf(stderr, "attempt to add null alternative\n");
      return;
    }

    if (std::find(candidates.begin(), candidates.end(), alternative) !=
        candidates.end()) {
      return;
    }

    candidates.push_back(std::move(alternative));
    selected = nullptr;
  }

  void remove(const std::shared_ptr<Alternative> &alternative);

  void callMethod(
      Context &context, std::string_view name, MethodCallArgs args,
      std::move_only_function<void(MethodCallResult)> responseHandler) override;
  void handleNotification(Context &context, std::string_view name,
                          NotificationArgs args) override;
  std::error_code activate(Context &context) override;
  std::error_code deactivate(Context &context) override;

  const Manifest &manifest() const override {
    if (selected != nullptr) {
      return selected->manifest();
    }

    return Alternative::manifest();
  }
};
