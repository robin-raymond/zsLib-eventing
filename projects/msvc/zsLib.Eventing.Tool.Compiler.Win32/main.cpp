
#include <zsLib/eventing/tool/ICommandLine.h>
#include <zsLib/eventing/tool/OutputStream.h>

#include <iostream>

namespace zsLib { namespace eventing { namespace tool { ZS_DECLARE_SUBSYSTEM(zsLib_eventing_tool) } } }

using namespace zsLib::eventing::tool;

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
  switch (fdwCtrlType)
  {
    // Handle the CTRL-C signal. 
  case CTRL_C_EVENT:
    ICommandLine::interrupt();
    return(FALSE);

    // CTRL-CLOSE: confirm that the user wants to exit. 
  case CTRL_CLOSE_EVENT:
    return(TRUE);

    // Pass other signals to the next handler. 
  case CTRL_BREAK_EVENT:
    printf("Ctrl-Break event\n\n");
    return FALSE;

  case CTRL_LOGOFF_EVENT:
    return FALSE;

  case CTRL_SHUTDOWN_EVENT:
    return FALSE;

  default:
    break;
  }
  return FALSE;
}

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

  SetConsoleCtrlHandler(CtrlHandler, TRUE);

  arguments = ICommandLine::toList(argc, argv);
  return ICommandLine::performDefaultHandling(arguments);
}
