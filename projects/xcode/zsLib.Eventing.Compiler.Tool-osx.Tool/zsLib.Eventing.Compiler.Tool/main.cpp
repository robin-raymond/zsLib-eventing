//
//  main.cpp
//  zsLib.Eventing.Compiler.Tool
//
//  Created by Robin Raymond on 2016-11-21.
//  Copyright © 2016 Robin Raymond. All rights reserved.
//

#include <zsLib/eventing/tool/ICommandLine.h>
#include <zsLib/eventing/tool/OutputStream.h>

#include <iostream>

namespace zsLib { namespace eventing { namespace tool { ZS_DECLARE_SUBSYSTEM(zsLib_eventing_tool) } } }

using namespace zsLib::eventing::tool;

int main(int argc, char * const argv[])
{
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
