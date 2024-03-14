#include "Server.hpp"
#include "Protocol.hpp"
#include "Transport.hpp"

std::error_code Server::activate(Context &context, std::string_view role,
                                 MethodCallArgs args,
                                 MethodCallResult *response) {
  if (m_process) {
    return std::make_error_code(std::errc::file_exists);
  }

  auto launch = manifest().launch.value();

  std::error_code ec;

  m_process = boost::process::child(
      launch.executable, launch.args, ec, boost::process::std_out > m_stdout,
      boost::process::std_err > m_stderr, boost::process::std_in < m_stdin,
      m_process_group);

  if (!ec) {
    auto transport = createTransport(launch.transport, this);
    m_protocol = createProtocol(launch.protocol, std::move(transport));
  }

  return ec;
}

std::error_code Server::deactivate(Context &context, std::string_view role) {
  // TODO
  return {};
}

