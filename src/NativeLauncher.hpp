#pragma once

#include "Alternative.hpp"
#include <boost/process.hpp>

struct NativeLauncher : Alternative {
  using Alternative::Alternative;

  void callMethod(
      Context &context, std::string_view name, MethodCallArgs args,
      std::move_only_function<void(MethodCallResult)> responseHandler) override;
  std::error_code activate(Context &context, std::string_view role,
                           MethodCallArgs args,
                           MethodCallResult *response) override {
    return {};
  }
  std::error_code deactivate(Context &context, std::string_view role) override {
    return {};
  }

private:
  boost::process::group m_process_group;
  std::map<boost::process::pid_t, boost::process::child, std::less<>>
      m_processes;
};
