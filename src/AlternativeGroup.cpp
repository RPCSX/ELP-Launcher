#include "AlternativeGroup.hpp"
#include "Context.hpp"

std::vector<std::shared_ptr<Alternative>>
AlternativeGroup::find(const AlternativeRequirements &requirements) {
  std::vector<std::shared_ptr<Alternative>> result;

  for (auto &candidate : candidates) {
    if (candidate->match(requirements)) {
      result.push_back(candidate);
    }
  }

  return result;
}

void AlternativeGroup::remove(const std::shared_ptr<Alternative> &alternative) {
  if (selected == alternative) {
    selected = nullptr;
  }

  for (auto it = candidates.begin(); it != candidates.end(); ++it) {
    if (it->get() == alternative.get()) {
      it = candidates.erase(it);
    } else {
      ++it;
    }
  }
}

void AlternativeGroup::callMethod(
    Context &context, std::string_view name, MethodCallArgs args,
    std::move_only_function<void(MethodCallResult)> responseHandler) {
  if (selected != nullptr) {
    selected->callMethod(context, name, std::move(args),
                         std::move(responseHandler));
    return;
  }

  Alternative::callMethod(context, name, std::move(args),
                          std::move(responseHandler));
}

void AlternativeGroup::handleNotification(Context &context,
                                          std::string_view name,
                                          NotificationArgs args) {
  if (selected != nullptr) {
    selected->handleNotification(context, name, std::move(args));
    return;
  }

  Alternative::handleNotification(context, name, std::move(args));
}

std::error_code AlternativeGroup::activate(Context &context) {
  if (selected == nullptr) {
    std::vector<Manifest> alternatives;
    alternatives.reserve(candidates.size());

    for (auto candidate : candidates) {
      alternatives.push_back(candidate->manifest());
    }

    MethodCallResult resolverResponse;
    if (context.showView("alternative-resolver",
                         {{"alternatives", alternatives}}, &resolverResponse,
                         false)) {
      return std::make_error_code(std::errc::no_such_file_or_directory);
    }

    selected = candidates[resolverResponse.get<std::size_t>()];
  }

  return selected->activate(context);
}

std::error_code AlternativeGroup::deactivate(Context &context) {
  if (selected != nullptr) {
    return selected->deactivate(context);
  }

  return std::make_error_code(std::errc::no_such_file_or_directory);
}
