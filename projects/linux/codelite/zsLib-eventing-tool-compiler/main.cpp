
#include <zsLib/eventing/tool/ICommandLine.h>
#include <zsLib/eventing/tool/OutputStream.h>

#include <iostream>

#include <signal.h>

namespace zsLib { namespace eventing { namespace tool { ZS_DECLARE_SUBSYSTEM(zsLib_eventing_tool) } } }

using namespace zsLib::eventing::tool;

void CtrlHandler(int sig)
{
    ICommandLine::interrupt();
}


int main(int argc, char * const argv[])
{
  ICommandLine::StringList arguments;

  if (argc > 0) {
    argv = &(argv[1]);
    --argc;
  }

  output().installStdOutput();
  output().installDebugger();

  signal(SIGINT, CtrlHandler);

  arguments = ICommandLine::toList(argc, argv);
  return ICommandLine::performDefaultHandling(arguments);
}
