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

#include <zsLib/eventing/IEventingTypes.h>
#include <zsLib/eventing/IIDLTypes.h>

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
      #pragma mark ICompilerTypes
      #pragma mark

      interaction ICompilerTypes
      {
        ZS_DECLARE_TYPEDEF_PTR(IEventingTypes::Provider, Provider);
        ZS_DECLARE_TYPEDEF_PTR(IIDLTypes::Project, Project);

        enum Modes
        {
          Mode_First,
          
          Mode_Eventing = Mode_First,
          Mode_IDL,
          
          Mode_Last = Mode_IDL,
        };

        static const char *toString(Modes value);
        static Modes toMode(const char *value) throw (InvalidArgument);

        enum IDLOutputs
        {
          IDLOutput_First,

          IDLOutput_CX = IDLOutput_First,
          IDLOutput_ObjectiveC,
          IDLOutput_JavaAndroid,
          
          IDLOutput_Last = IDLOutput_JavaAndroid,
        };

        static const char *toString(IDLOutputs value);
        static IDLOutputs toIDLOutput(const char *value) throw (InvalidArgument);

        struct Config
        {
          Modes           mMode {Mode_First};
          IDLOutputs  mIDLOutput {IDLOutput_First};
          String          mConfigFile;

          StringList      mSourceFiles;
          String          mOutputName;
          String          mAuthor;

          ProviderPtr     mProvider;
          ProjectPtr      mProject;
        };
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IProcess
      #pragma mark

      interaction ICompiler : public ICompilerTypes
      {
        static ICompilerPtr create(const Config &config);

        virtual void process() throw (Failure, FailureWithLine) = 0;
      };
    }
  }
}
