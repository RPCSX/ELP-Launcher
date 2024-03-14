#include "NativeLauncher.hpp"

void NativeLauncher::callMethod(
    Context &context, std::string_view name, MethodCallArgs args,
    std::move_only_function<void(MethodCallResult)> responseHandler) {

  if (name == "launch") {
    std::error_code ec;

    if (!args.contains("executable")) {
      responseHandler({{"error", elp::ErrorCode::InvalidParam}});
      return;
    }

    auto path = args["executable"].get<std::string>();
    std::vector<std::string> execArgs;

    if (args.contains("args")) {
      execArgs = args["args"];
    }

    auto process = boost::process::child(path, execArgs, ec, m_process_group);

    auto pid = process.id();
    m_processes.emplace(pid, std::move(process));

    if (ec) {
      responseHandler({{"error", ec.message()}});
    } else {
      responseHandler({{"result", pid}});
    }
    return;
  }

  if (name == "terminate") {
    if (!args.contains("pid")) {
      responseHandler({{"error", elp::ErrorCode::InvalidParam}});
      return;
    }

    boost::process::pid_t pid = args["pid"];

    if (auto it = m_processes.find(pid); it != m_processes.end()) {
      it->second.terminate();
      m_processes.erase(it);
      responseHandler(MethodCallResult::object());
    } else {
      responseHandler({{"error", elp::ErrorCode::NotFound}});
    }

    return;
  }

  responseHandler({{"error", elp::ErrorCode::MethodNotFound}});
}
