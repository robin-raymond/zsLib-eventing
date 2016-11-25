
#include <zsLib/eventing/tool/ICommandLine.h>
#include <zsLib/eventing/tool/OutputStream.h>

#include <iostream>

namespace zsLib { namespace eventing { namespace tool { ZS_DECLARE_SUBSYSTEM(zsLib_eventing_tool) } } }

using namespace zsLib::eventing::tool;

int main(int argc, char * const argv[])
{
  int result = 0;

  ICommandLine::StringList arguments;

  if (argc > 0) {
    argv = &(argv[1]);
    --argc;
  }

  output().installStdOutput();
  output().installDebugger();

  ICommandLine::outputHeader();
  arguments = ICommandLine::toList(argc, argv);
  return ICommandLine::performDefaultHandling(arguments);
}
