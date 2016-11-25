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

#include <zsLib/eventing/types.h>

namespace zsLib
{
  namespace eventing
  {
    interaction IHelper
    {
      static SecureByteBlockPtr loadFile(const char *path) throw (StdError);
      static void saveFile(const char *path, SecureByteBlock &buffer) throw (StdError);
      static ElementPtr read(const SecureByteBlockPtr buffer);
      static ElementPtr read(const SecureByteBlock &buffer);
      static SecureByteBlockPtr writeJSON(
                                          const Document &doc,
                                          bool prettyPrint = true
                                          );
      static SecureByteBlockPtr writeXML(const Document &doc);

      static String getElementText(const ElementPtr &el);
      static String getElementTextAndDecode(const ElementPtr &el);
      static ElementPtr createElementWithText(
                                              const String &elName,
                                              const String &text
                                              );
      static ElementPtr createElementWithNumber(
                                                const String &elName,
                                                const String &numberAsStringValue
                                                );
      static ElementPtr createElementWithTime(
                                              const String &elName,
                                              Time time
                                              );

      static ElementPtr createElementWithTextAndJSONEncode(
                                                           const String &elName,
                                                           const String &textVal
                                                           );
      static String timeToString(const Time &value);
      static Time stringToTime(const String &str);

      static String convertToString(const SecureByteBlock &buffer);
      static SecureByteBlockPtr convertToBuffer(const String &str);

      static String convertToHex(
                                 const BYTE *buffer,
                                 size_t bufferLengthInBytes,
                                 bool outputUpperCase = false
                                 );

      static String convertToHex(
                                 const SecureByteBlock &input,
                                 bool outputUpperCase = false
                                 );
    };
  }
}
