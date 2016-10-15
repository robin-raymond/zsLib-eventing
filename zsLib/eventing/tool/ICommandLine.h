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

#pragma once

#include <zsLib/eventing/tool/types.h>

namespace zsLib
{
  namespace eventing
  {
    namespace tool
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICommandLineTypes
      #pragma mark

      interaction ICommandLineTypes
      {
        ZS_DECLARE_STRUCT_PTR(Config);
        ZS_DECLARE_TYPEDEF_PTR(std::list<String>, StringList);

        enum Flags
        {
          Flag_First,

          Flag_None = Flag_First,

          Flag_Config,
          Flag_Question,
          Flag_Help,
          Flag_HelpAlt,
          Flag_Source,
          Flag_OutputName,

          Flag_Last = Flag_HelpAlt,
        };

        static Flags toFlag(const char *str);
        static const char *toString(Flags flag);

        struct Config
        {
          StringList  mArguments;
          String      mConfigFile;
          StringList  mSourceFiles;
          String      mOutputName;
        };

      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICommandLine
      #pragma mark

      interaction ICommandLine : public ICommandLineTypes
      {
        static void outputHeader();
        static void outputHelp();

        static StringList toList(
                                 int inArgc,
                                 const char * const inArgv[]
                                 );
        static StringList toList(
                                 int inArgc,
                                 const wchar_t * const inArgv[]
                                 );

        static int performDefaultHandling(const StringList &arguments);

        static void prepare(
                            StringList arguments,
                            Config &outConfig
                            ) throw (InvalidArgument);

        static void validate(Config &config) throw (InvalidArgument);
        static void process(Config &config) throw (Failure);
      };

    } // namespace tool
  } // namespace eventing
} // namespace zsLib