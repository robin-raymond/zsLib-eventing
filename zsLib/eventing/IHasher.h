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

#include <cryptopp/sha.h>
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cryptopp/md5.h>

namespace zsLib
{
  namespace eventing
  {
    interaction IHasherAlgorithm
    {
      virtual const BYTE *digest() const = 0;
      virtual size_t digestSize() const = 0;
      virtual void update(const char *str) = 0;
      virtual void update(const BYTE *buffer, size_t length) = 0;
      virtual const BYTE *finalize() = 0;
    };

    interaction IHasher
    {
      typedef CryptoPP::Weak::MD5 MD5;
      typedef CryptoPP::SHA256 SHA256;
      typedef CryptoPP::SHA1 SHA1;

      template <typename THasher>
      class HasherAlgorithm : public IHasherAlgorithm
      {
      public:
        HasherAlgorithm() {}

        static IHasherAlgorithmPtr create() { return make_shared<HasherAlgorithm>(); }

        virtual const BYTE *digest() const override { return &(mOutput[0]); }
        virtual size_t digestSize() const override { return static_cast<size_t>(mHasher.DigestSize());  }
        virtual void update(const char *str) override { if (!str) return; mHasher.Update(reinterpret_cast<const BYTE *>(str), strlen(str)); }
        virtual void update(const BYTE *buffer, size_t length) override { mHasher.Update(buffer, length); }
        virtual const BYTE *finalize() override { mHasher.Final(&(mOutput[0])); return &(mOutput[0]); }

      private:
        BYTE mOutput[sizeof(THasher)] {};
        mutable THasher mHasher;
      };

      static IHasherAlgorithmPtr md5() { return HasherAlgorithm<MD5>::create(); }
      static IHasherAlgorithmPtr sha1() { return HasherAlgorithm<SHA1>::create(); }
      static IHasherAlgorithmPtr sha256() { return HasherAlgorithm<SHA256>::create(); }

      static SecureByteBlockPtr hash(const char *str, IHasherAlgorithmPtr algorithm = sha1());
      static SecureByteBlockPtr hash(const std::string &str, IHasherAlgorithmPtr algorithm = sha1());
      static SecureByteBlockPtr hash(const BYTE *buffer, size_t length, IHasherAlgorithmPtr algorithm = sha1());
      static SecureByteBlockPtr hash(const SecureByteBlock &buffer, IHasherAlgorithmPtr algorithm = sha1());
      static SecureByteBlockPtr hash(const SecureByteBlockPtr &buffer, IHasherAlgorithmPtr algorithm = sha1());

      static String hashAsString(const char *str, IHasherAlgorithmPtr algorithm = sha1());
      static String hashAsString(const std::string &str, IHasherAlgorithmPtr algorithm = sha1());
      static String hashAsString(const BYTE *buffer, size_t length, IHasherAlgorithmPtr algorithm = sha1());
      static String hashAsString(const SecureByteBlock &buffer, IHasherAlgorithmPtr algorithm = sha1());
      static String hashAsString(const SecureByteBlockPtr &buffer, IHasherAlgorithmPtr algorithm = sha1());
    };
  }
}
