#pragma once

#include "Alternative.hpp"
#include "Protocol.hpp"

#include <boost/process.hpp>

#include <memory>
#include <system_error>

struct Server : Alternative {
  using Alternative::Alternative;

  std::error_code activate(Context &context, std::string_view role,
                           MethodCallArgs args,
                           MethodCallResult *response) override;
  std::error_code deactivate(Context &context, std::string_view role) override;

  Protocol *protocol() { return m_protocol.get(); }

  boost::process::ipstream &getStdout() { return m_stdout; }
  boost::process::ipstream &getStderr() { return m_stderr; }
  boost::process::opstream &getStdin() { return m_stdin; }

private:
  boost::process::group m_process_group;
  boost::process::child m_process;
  boost::process::ipstream m_stdout;
  boost::process::ipstream m_stderr;
  boost::process::opstream m_stdin;

  std::unique_ptr<Protocol> m_protocol;
};
