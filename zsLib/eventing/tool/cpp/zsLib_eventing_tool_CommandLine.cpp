/*

Copyright (c) 2016, Robin Raymond
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.

*/

#include <zsLib/eventing/tool/internal/zsLib_eventing_tool_CommandLine.h>

#include <zsLib/eventing/tool/ICompiler.h>
#include <zsLib/eventing/tool/OutputStream.h>

namespace zsLib { namespace eventing { namespace tool { ZS_DECLARE_SUBSYSTEM(zsLib_eventing_tool) } } }

namespace zsLib
{
  namespace eventing
  {
    namespace tool
    {
      namespace internal
      {
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICommandLine
      #pragma mark

      //-----------------------------------------------------------------------
      ICommandLineTypes::Flags ICommandLineTypes::toFlag(const char *value)
      {
        String str(value);
        for (ICommandLine::Flags index = ICommandLine::Flag_First; index <= ICommandLine::Flag_Last; index = static_cast<ICommandLine::Flags>(static_cast<std::underlying_type<ICommandLine::Flags>::type>(index) + 1)) {
          if (0 == str.compareNoCase(ICommandLine::toString(index))) return index;
        }

        return Flag_None;
      }

      //-----------------------------------------------------------------------
      const char *ICommandLineTypes::toString(Flags flag)
      {
        switch (flag)
        {
          case Flag_None:         return "";
          case Flag_Config:       return "c";
          case Flag_Question:     return "?";
          case Flag_Help:         return "h";
          case Flag_HelpAlt:      return "help";
          case Flag_Source:       return "s";
          case Flag_OutputName:   return "o";
          case Flag_Author:       return "author";
        }

        return "unknown";
      }

      //-----------------------------------------------------------------------
      void ICommandLine::outputHeader()
      {
        output() << "zsLibEventTool (v0.1)\n";
        output() << "(c)2016 Robin Raymond. All rights reserved.\n\n";
      }

      //-----------------------------------------------------------------------
      void ICommandLine::outputHelp()
      {
        output() <<
          " -?\n"
          " -h\n"
          " -help    output this help text.\n"
          "\n"
          " -c       config_file_name          - input event provider json configuration file.\n"
          " -s       source_file_name_1 ... n  - input C/C++ source file.\n"
          " -o       output_name ... n         - output name.\n"
          " -author  \"John Q Public\"         - manifest author.\n"
          "\n";
      }

      //-----------------------------------------------------------------------
      StringList ICommandLine::toList(
                                      int inArgc,
                                      const char * const inArgv[]
                                      )
      {
        StringList result;
        for (auto iter = 0; iter < inArgc; ++iter) {
          if (NULL == inArgv[iter]) continue;
          if (0 == (*(inArgv[iter]))) continue;

          result.push_back(String(inArgv[iter]));
        }
        return result;
      }

      //-----------------------------------------------------------------------
      StringList ICommandLine::toList(
                                      int inArgc,
                                      const wchar_t * const inArgv[]
                                      )
      {
        StringList result;
        for (auto iter = 0; iter < inArgc; ++iter) {
          if (NULL == inArgv[iter]) continue;
          if (0 == (*(inArgv[iter]))) continue;

          result.push_back(String(inArgv[iter]));
        }
        return result;
      }

      //-----------------------------------------------------------------------
      int ICommandLine::performDefaultHandling(const StringList &arguments)
      {
        int result = 0;

        try
        {
          ICompilerTypes::Config config;
          bool didOutputHelp {false};
          prepare(arguments, config, didOutputHelp);
          validate(config, didOutputHelp);
          process(config);
        } catch (const InvalidArgument &e) {
          output() << "[Error] " << e.message() << "\n\n";
          result = -1;
        } catch (const Failure &e) {
          output() << "[Error] " << e.message() << "\n\n";
          result = e.result();
        } catch (const FailureWithLine &e) {
          output() << "[Error] " << e.message() << "\n";
          output() << "[Info]  LINE=" << e.lineNumber() << "\n\n";
          result = e.result();
        } catch (const ICommandLine::NoopException &) {
          // do nothing expection
        }
        return result;
      }

      //-----------------------------------------------------------------------
      void ICommandLine::prepare(
                                 StringList arguments,
                                 ICompilerTypes::Config &outConfig,
                                 bool &outDidOutputHelp
                                 ) throw (InvalidArgument)
      {
        ICompilerTypes::Config config;

        ICommandLine::Flags flag {ICommandLine::Flag_None};

        String processedThusFar;

        while (arguments.size() > 0)
        {
          String arg(arguments.front());
          arguments.pop_front();

          if (processedThusFar.isEmpty()) {
            processedThusFar = arg;
          } else {
            processedThusFar += " " + arg;
          }

          if (arg.isEmpty()) {
            output() << "[Warning] Skipping empty argument after: " + processedThusFar << "\n\n";
            continue;
          }

          if (arg.substr(0, strlen("-")) == "-") {
            arg = arg.substr(strlen("-"));

            switch (flag)
            {
              case ICommandLine::Flag_Source:
              {
                flag = ICommandLine::Flag_None;
                break;
              }
              default:
              {
                break;
              }
            }

            if (ICommandLine::Flag_None != flag) {
              ZS_THROW_INVALID_ARGUMENT(String("2nd flag unexpected at this time: ") + processedThusFar);
            }

            flag = ICommandLine::toFlag(arg);
            switch (flag) {
              case ICommandLine::Flag_None: {
                ZS_THROW_INVALID_ARGUMENT(String("Command line flag is not understood: ") + arg + " within context " + processedThusFar);
              }
              case ICommandLine::Flag_Config: goto process_flag;
              case ICommandLine::Flag_Question:
              case ICommandLine::Flag_Help:
              case ICommandLine::Flag_HelpAlt:
              {
                outDidOutputHelp = true;
                outputHelp();
                return;
              }
              case ICommandLine::Flag_Source: goto process_flag;
              case ICommandLine::Flag_OutputName: goto process_flag;
              case ICommandLine::Flag_Author: goto process_flag;
            }
            ZS_THROW_INVALID_ARGUMENT("Internal error when processing argument: " + arg + " within context: " + processedThusFar);
          }

          // process flag
          {
            switch (flag)
            {
              case ICommandLine::Flag_None: {
                ZS_THROW_INVALID_ARGUMENT(String("Command line argument is not understood: ") + arg + " within context " + processedThusFar);
              }
              case ICommandLine::Flag_Config: {
                config.mConfigFile = arg;
                goto processed_flag;
              }
              case ICommandLine::Flag_Source: {
                config.mSourceFiles.push_back(arg);
                goto process_flag;  // process next source file in the list (maintain same flag)
              }
              case ICommandLine::Flag_OutputName: {
                config.mOutputName = arg;
                goto processed_flag;
              }
              case ICommandLine::Flag_Author: {
                config.mAuthor = arg;
                goto processed_flag;
              }
              default: break;
            }

            ZS_THROW_INVALID_ARGUMENT(String("Internal error when processing argument: ") + arg + " within context: " + processedThusFar);
          }

        processed_flag:
          {
            flag = ICommandLine::Flag_None;
            continue;
          }

        process_flag:
          {
            continue;
          }
        }

        outConfig = config;
      }

      //-----------------------------------------------------------------------
      void ICommandLine::validate(
                                  ICompilerTypes::Config &config,
                                  bool didOutputHelp
                                  ) throw (InvalidArgument, NoopException)
      {
        if (config.mConfigFile.isEmpty()) {
          ZS_THROW_CUSTOM_IF(NoopException, didOutputHelp);
          ZS_THROW_INVALID_ARGUMENT("Configuration file must be specified.");
        }
      }

      //-----------------------------------------------------------------------
      void ICommandLine::process(ICompilerTypes::Config &config) throw (Failure)
      {
        output() << "[Note] Using configuration file: " + config.mConfigFile << "\n";
        output() << "\n";

        auto process = ICompiler::create(config);
        process->process();
      }
    }
  }
}
