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

#include <zsLib/eventing/tool/internal/zsLib_eventing_tool_IDLCompiler.h>
#include <zsLib/eventing/tool/internal/zsLib_eventing_tool_Helper.h>

#include <zsLib/eventing/tool/OutputStream.h>

#include <zsLib/eventing/IHelper.h>
#include <zsLib/eventing/IHasher.h>
#include <zsLib/eventing/IEventingTypes.h>

#include <zsLib/Exception.h>
#include <zsLib/Numeric.h>

#include <sstream>
#include <list>
#include <set>

#define ZS_WRAPPER_COMPILER_DIRECTIVE_EXCLUSIZE "EXCLUSIVE"

namespace zsLib { namespace eventing { namespace tool { ZS_DECLARE_SUBSYSTEM(zsLib_eventing_tool) } } }

namespace zsLib
{
  namespace eventing
  {
    ZS_DECLARE_TYPEDEF_PTR(IIDLTypes::Project, Project);

    namespace tool
    {
      ZS_DECLARE_TYPEDEF_PTR(eventing::IHelper, UseHelper);
      ZS_DECLARE_TYPEDEF_PTR(eventing::IHasher, UseHasher);
      typedef std::set<String> HashSet;

      namespace internal
      {
        ZS_DECLARE_TYPEDEF_PTR(IDLCompiler::Token, Token);
        typedef IDLCompiler::TokenList TokenList;

        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark Helpers
        #pragma mark

        //---------------------------------------------------------------------
        static void skipPreprocessor(
                                     const char * &p,
                                     ULONG &ioLineCount
                                     )
        {
          const char *startPos = p;
          
          while (true)
          {
            Helper::skipToEOL(p);

            // see if this preprocessor statement is multi-line
            while (p != startPos)
            {
              --p;
              if (('\n' == *p) ||
                  ('\r' == *p)) {
                break;
              }

              if (isspace(*p)) continue;

              if ('\\' == *p) {
                Helper::skipToEOL(p);
                if (Helper::skipEOL(p, &ioLineCount)) goto next_line;
              }
              Helper::skipToEOL(p);
              Helper::skipEOL(p, &ioLineCount);
              return;
            }

          next_line:
            {
            }
          }
        }
        
        //---------------------------------------------------------------------
        static TokenPtr getCPPDirectiveToken(
                                             const char *p,
                                             ULONG &ioLineCount
                                             )
        {
          if ('/' != *p) return TokenPtr();
          if ('/' != *(p+1)) return TokenPtr();
          if ('!' != *(p+2)) return TokenPtr();

          p += 3;

          const char *start = p;
          Helper::skipToEOL(p);

          String str(p, static_cast<size_t>(p - start));

          auto result = make_shared<Token>();
          result->mTokenType = IDLCompiler::TokenType_Directive;
          result->mToken = str;
          result->mLineCount = ioLineCount;

          Helper::skipEOL(p, &ioLineCount);

          return result;
        }
        
        //---------------------------------------------------------------------
        static TokenPtr getCPPDocToken(
                                       const char *p,
                                       ULONG &ioLineCount
                                       )
        {
          if ('/' != *p) return TokenPtr();
          if ('/' != *(p+1)) return TokenPtr();
          if ('/' != *(p+2)) return TokenPtr();
          
          p += 3;

          const char *start = p;
          Helper::skipToEOL(p);
          
          String str(p, static_cast<size_t>(p - start));
          
          auto result = make_shared<Token>();
          result->mTokenType = IDLCompiler::TokenType_Documentation;
          result->mToken = str;
          result->mLineCount = ioLineCount;

          Helper::skipEOL(p, &ioLineCount);

          return result;
        }
        
        //---------------------------------------------------------------------
        static TokenPtr getQuoteToken(
                                      const char * &p,
                                      ULONG &ioLineCount
                                      )
        {
          ULONG currentLine = ioLineCount;
          
          const char *start = p;
          if (!Helper::skipQuote(p, &ioLineCount)) return TokenPtr();

          auto result = make_shared<Token>();
          result->mTokenType = IDLCompiler::TokenType_Quote;
          result->mToken = String(start, static_cast<size_t>(p - start));
          result->mLineCount = currentLine;
          return result;
        }

        //---------------------------------------------------------------------
        static TokenPtr getCharToken(
                                     const char * &p,
                                     ULONG &ioLineCount
                                     ) throw (FailureWithLine)
        {
          ULONG currentLine = ioLineCount;
          
          const char *start = p;
          if ('\'' != *p) return TokenPtr();
          
          ++p;
          if ('\\' == *p) {
            Helper::decodeCEscape(p, ioLineCount);
          } else {
            ++p;
          }
          if ('\'' != *p) return TokenPtr();
          ++p;

          auto result = make_shared<Token>();
          result->mTokenType = IDLCompiler::TokenType_Char;
          result->mToken = String(start, static_cast<size_t>(p - start));
          result->mLineCount = currentLine;
          return result;
        }

        //---------------------------------------------------------------------
        static TokenPtr getNumberToken(
                                       const char * &p,
                                       ULONG lineCount
                                       )
        {
          const char *start = p;
          
          bool foundNegative = false;
          bool foundDot = false;
          bool foundExponent = false;

          if ('-' == *start) {
            foundNegative = true;
            ++start;
            Helper::skipWhitespaceExceptEOL(start);
          }

          if (!isnumber(*start)) return TokenPtr();

          p = start;
          
          int base = 10;
          
          if ('0' == *p) {
            switch (*(p+1)) {
              case 'x':
              case 'X': {
                base = 16;
                p += 2;
                break;
              }
              case 'b':
              case 'B': {
                base = 2;
                p += 2;
                break;
              }
              case '.': {
                break;
              }
              case '0':
              case '1':
              case '2':
              case '3':
              case '4':
              case '5':
              case '6':
              case '7': {
                base = 8;
                ++p;
                break;
              }
            }
          }

          while ('\0' != *p)
          {
            switch (*p)
            {
              case '.': {
                if (10 != base) goto check_exponent;
                ++p;
                foundDot = true;
                continue;
              }
              case '0':
              case '1': {
                ++p;
                continue;
              }
              case '2':
              case '3':
              case '4':
              case '5':
              case '6':
              case '7': {
                if (base >= 8) {
                  ++p;
                  continue;
                }
                goto check_exponent;
              }
              case '8':
              case '9': {
                if (base >= 10) {
                  ++p;
                  continue;
                }
                goto check_exponent;
              }
              case 'a':
              case 'A':
              case 'b':
              case 'B':
              case 'c':
              case 'C':
              case 'd':
              case 'D':
              case 'e':
              case 'E':
              case 'f':
              case 'F': {
                if (base >= 10) {
                  ++p;
                  continue;
                }
                goto check_exponent;
              }
              default: goto check_exponent;
            }
          }
          
        check_exponent:
          {
            const char *exponentStart = p;
            
            if (('e' != *p) &&
                ('E' != *p)) {
              goto check_postfix;
            }
            if (10 != base) goto check_postfix;

            foundExponent = true;
            ++p;
            
            bool foundExponentNumber = false;
            if (('-' == *p) ||
                ('+' == *p)) {
              ++p;
            }

            while (isnumber(*p)) {
              foundExponentNumber = true;
              ++p;
            }

            if (!foundExponentNumber) {
              // the 'e' does not belong to the number
              p = exponentStart;
              goto done;
            }
          }
          
        check_postfix:
          {
            const char *postFixStart = p;
            
            bool mUnsigned = false;
            bool mFloat = false;
            size_t foundLongs = 0;
            bool lastWasLong = false;
            
            while (true)
            {
              switch (*p) {
                case 'u':
                case 'U':
                {
                  if (mUnsigned) goto invalid_postfix;
                  if (mFloat) goto invalid_postfix;
                  mUnsigned = true;
                  goto not_long;
                }
                case 'l':
                case 'L':
                {
                  if (foundLongs > 0) {
                    if (mFloat) goto invalid_postfix;
                    if (!lastWasLong) goto invalid_postfix;
                  }
                  ++foundLongs;
                  if (foundLongs > 2) goto invalid_postfix;
                  ++p;
                  lastWasLong = true;
                  continue;
                }
                case 'f':
                case 'F':
                {
                  if (10 != base) goto invalid_postfix;
                  if (mUnsigned) goto invalid_postfix;
                  if (foundLongs > 1) goto invalid_postfix;
                  if (mFloat) goto invalid_postfix;
                  mFloat = true;
                  goto not_long;
                }
                default:
                {
                  goto done;
                }
              }
              
            not_long:
              {
                ++p;
                lastWasLong = false;
                continue;
              }
              
            invalid_postfix:
              {
                p = postFixStart;
                goto done;
              }
            }
          }
          
        done:
          {
          }
          
          auto result = make_shared<Token>();
          result->mTokenType = IDLCompiler::TokenType_Number;
          result->mToken = String(start, static_cast<size_t>(p - start));
          if (foundNegative) {
            result->mToken = String("-") + result->mToken;
          }
          result->mLineCount = lineCount;
          return result;
        }
        
        //---------------------------------------------------------------------
        static TokenPtr getIdentifierToken(
                                           const char * &p,
                                           ULONG lineCount
                                           )
        {
          if ((!isalpha(*p)) &&
              ('_' != *p)) return TokenPtr();
          
          const char *start = p;
          
          while ((isalnum(*p)) ||
                 ('_' == *p)) {
            ++p;
          }
          
          auto result = make_shared<Token>();
          result->mTokenType = IDLCompiler::TokenType_Identifier;
          result->mToken = String(start, static_cast<size_t>(p - start));
          result->mLineCount = lineCount;
          return result;
        }
        
        //---------------------------------------------------------------------
        static TokenPtr getOperatorToken(
                                         const char * &p,
                                         ULONG lineCount
                                         )
        {
          static const char *operators[] =
          {
            "{",
            "}",
            "(",
            ")",
            "[",
            "]",
            ";",
            "<",
            ">",
            "::",
            "=",
            ":",
            ",",
            "?",
            //"++",
            //"--",
            //".",
            //"->",
            //"~",
            //"!",
            //"+",
            //"-",
            //"&",
            //"*",
            //".*",
            //"->*",
            //"*",
            //"/",
            //"%",
            //"<<",
            //">>",
            //">=",
            //"<=",
            //"==",
            //"!=",
            //"^",
            //"|",
            //"&&",
            //"||",
            //"*=",
            //"/=",
            //"%=",
            //"+=",
            //"-=",
            //">>=",
            //"<<=",
            //"&=",
            //"^=",
            //"|=",
            NULL
          };

          String valid;
          String test;

          while ('\0' != *p) {
            test = valid;
            test += String(p, static_cast<size_t>(1));
            
            for (int index = 0; NULL != operators[index]; ++index)
            {
              if (test == operators[index]) goto next;
            }
            goto done;
            
          next:
            {
              valid = test;
              ++p;
            }
          }

        done:
          {
          }
          
          auto result = make_shared<Token>();
          result->mTokenType = IDLCompiler::TokenType_Operator;
          if (";" == valid) {
            result->mTokenType = IDLCompiler::TokenType_SemiColon;
          } else if ("(" == valid) {
            result->mTokenType = IDLCompiler::TokenType_Brace;
          } else if (")" == valid) {
            result->mTokenType = IDLCompiler::TokenType_Brace;
          } else if ("{" == valid) {
            result->mTokenType = IDLCompiler::TokenType_CurlyBrace;
          } else if ("}" == valid) {
            result->mTokenType = IDLCompiler::TokenType_CurlyBrace;
          } else if ("[" == valid) {
            result->mTokenType = IDLCompiler::TokenType_SquareBrace;
          } else if ("]" == valid) {
            result->mTokenType = IDLCompiler::TokenType_SquareBrace;
          } else if ("<" == valid) {
            result->mTokenType = IDLCompiler::TokenType_AngleBrace;
          } else if (">" == valid) {
            result->mTokenType = IDLCompiler::TokenType_AngleBrace;
          } else if ("::" == valid) {
            result->mTokenType = IDLCompiler::TokenType_ScopeOperator;
          } else if ("," == valid) {
            result->mTokenType = IDLCompiler::TokenType_CommaOperator;
          } else if (":" == valid) {
            result->mTokenType = IDLCompiler::TokenType_ColonOperator;
          } else if ("=" == valid) {
            result->mTokenType = IDLCompiler::TokenType_EqualsOperator;
          }

          result->mToken = valid;
          result->mLineCount = lineCount;
          return result;
        }
        
        //---------------------------------------------------------------------
        static TokenPtr getUnknownToken(
                                        const char * &p,
                                        ULONG lineCount
                                        )
        {
          if (!p) return TokenPtr();
          
          if ('\0' == *p) return TokenPtr();
          
          auto result = make_shared<Token>();
          result->mTokenType = IDLCompiler::TokenType_Unknown;
          result->mToken = String(p, static_cast<size_t>(1));
          result->mLineCount = lineCount;
          ++p;
          return result;
        }

        //---------------------------------------------------------------------
        static TokenPtr getNextToken(
                                     const char * &p,
                                     bool &ioStartOfLine,
                                     ULONG &ioLineCount
                                     )
        {
          if (!p) return TokenPtr();
          
          while ('\0' != *p)
          {
            ULONG activeLine = ioLineCount;
            
            if (Helper::skipWhitespaceExceptEOL(p)) continue;
            if (Helper::skipEOL(p, &ioLineCount)) {
              ioStartOfLine = true;
              continue;
            }
            
            if (ioStartOfLine) {
              if ('#' == *p) {
                skipPreprocessor(p, ioLineCount);
                ioStartOfLine = true;
                continue;
              }
            }

            if (Helper::skipCComments(p, &ioLineCount)) {
              if (activeLine != ioLineCount) ioStartOfLine = true;
              continue;
            }

            {
              auto result = getCPPDirectiveToken(p, ioLineCount);
              if (result) {
                ioStartOfLine = true;
                return result;
              }
            }

            {
              auto result = getCPPDocToken(p, ioLineCount);
              if (result) {
                ioStartOfLine = true;
                return result;
              }
            }

            if (Helper::skipCPPComments(p)) {
              Helper::skipEOL(p, &ioLineCount);
              ioStartOfLine = true;
              continue;
            }
            
            {
              auto result = getQuoteToken(p, ioLineCount);
              if (result) {
                ioStartOfLine = false;
                return result;
              }
            }
            
            {
              auto result = getNumberToken(p, ioLineCount);
              if (result) {
                ioStartOfLine = false;
                return result;
              }
            }
            
            {
              auto result = getIdentifierToken(p, ioLineCount);
              if (result) {
                ioStartOfLine = false;
                return result;
              }
            }
            
            {
              auto result = getOperatorToken(p, ioLineCount);
              if (result) {
                ioStartOfLine = false;
                return result;
              }
            }
            
            {
              auto result = getUnknownToken(p, ioLineCount);
              if (result) {
                ioStartOfLine = false;
                return result;
              }
            }
          }

          return TokenPtr();
        }

        //---------------------------------------------------------------------
        void tokenize(
                      const char *p,
                      TokenList &outTokens,
                      ULONG startLineNumber = 1
                      )
        {
          bool startOfLine = true;
          ULONG lineCount = startLineNumber;
          
          while (true)
          {
            auto token = getNextToken(p, startOfLine, lineCount);
            if (!token) break;

            outTokens.push_back(token);
          }
        }
        
        //---------------------------------------------------------------------
        void replaceAliases(
                            TokenList &ioTokens,
                            const IEventingTypes::AliasMap &aliases
                            )
        {
          for (auto iter_doNotUse = ioTokens.begin(); iter_doNotUse != ioTokens.end(); )
          {
            auto current = iter_doNotUse;
            ++iter_doNotUse;
            
            auto token = (*current);
            auto found = aliases.find(token->mToken);
            if (found == aliases.end()) continue;

            TokenList replacementTokens;
            tokenize((*found).second.c_str(), replacementTokens, token->mLineCount);
            
            for (auto iterReplace = replacementTokens.rbegin(); iterReplace != replacementTokens.rend(); ++iterReplace)
            {
              auto replaceToken = (*iterReplace);
              ioTokens.insert(current, replaceToken);
            }

            ioTokens.erase(current);
          }
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IDLCompiler
        #pragma mark

        //---------------------------------------------------------------------
        bool IDLCompiler::Token::isBrace() const
        {
          switch (mTokenType) {
            case TokenType_Brace:
            case TokenType_CurlyBrace:
            case TokenType_SquareBrace:
            case TokenType_AngleBrace:    return true;
            default:                      break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::Token::isOpenBrace() const
        {
          switch (mTokenType) {
            case TokenType_Brace:         return "(" == mToken;
            case TokenType_CurlyBrace:    return "{" == mToken;
            case TokenType_SquareBrace:   return "[" == mToken;
            case TokenType_AngleBrace:    return "<" == mToken;
            default:                      break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::Token::isCloseBrace() const
        {
          switch (mTokenType) {
            case TokenType_Brace:         return ")" == mToken;
            case TokenType_CurlyBrace:    return "}" == mToken;
            case TokenType_SquareBrace:   return "]" == mToken;
            case TokenType_AngleBrace:    return ">" == mToken;
            default:                      break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IDLCompiler
        #pragma mark

        //---------------------------------------------------------------------
        IDLCompiler::IDLCompiler(
                                           const make_private &,
                                           const Config &config
                                           ) :
          mConfig(config)
        {
        }

        //---------------------------------------------------------------------
        IDLCompiler::~IDLCompiler()
        {
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IDLCompiler => ICompiler
        #pragma mark

        //---------------------------------------------------------------------
        IDLCompilerPtr IDLCompiler::create(const Config &config)
        {
          IDLCompilerPtr pThis(std::make_shared<IDLCompiler>(make_private{}, config));
          pThis->mThisWeak = pThis;
          return pThis;
        }

        //---------------------------------------------------------------------
        void IDLCompiler::process() throw (Failure, FailureWithLine)
        {
          outputSkeleton();
          read();
          validate();
          if ((mConfig.mOutputName.hasData()) &&
              (mConfig.mProject)) {
          }
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IDLCompiler => (internal)
        #pragma mark

        //---------------------------------------------------------------------
        void IDLCompiler::outputSkeleton()
        {
        }

        //---------------------------------------------------------------------
        void IDLCompiler::read() throw (Failure, FailureWithLine)
        {
          HashSet processedHashes;

          ProjectPtr &project = mConfig.mProject;
          
          SecureByteBlockPtr configRaw;

          try {
            configRaw = UseHelper::loadFile(mConfig.mConfigFile);
          } catch (const StdError &e) {
            ZS_THROW_CUSTOM_PROPERTIES_1(Failure, ZS_EVENTING_TOOL_FILE_FAILED_TO_LOAD, String("Failed to load main configuration file: ") + mConfig.mConfigFile + ", error=" + string(e.result()) + ", reason=" + e.message());
          }
          if (!configRaw) {
            ZS_THROW_CUSTOM_PROPERTIES_1(Failure, ZS_EVENTING_TOOL_FILE_FAILED_TO_LOAD, String("Failed to load main configuration file: ") + mConfig.mConfigFile);
          }
          processedHashes.insert(UseHasher::hashAsString(configRaw));
          auto rootEl = UseHelper::read(configRaw);

          try {
            project = Project::create(rootEl);
          } catch (const InvalidContent &e) {
            ZS_THROW_CUSTOM_PROPERTIES_1(Failure, ZS_EVENTING_TOOL_INVALID_CONTENT, "Failed to parse main configuration: " + e.message());
          }

          StringList sources = mConfig.mSourceFiles;
          mConfig.mSourceFiles.clear();

          ElementPtr sourcesEl = rootEl->findFirstChildElement("includes");
          if (sourcesEl) {
            ElementPtr sourceEl = sourcesEl->findFirstChildElement("include");
            while (sourceEl) {
              auto source = UseHelper::getElementTextAndDecode(sourceEl);

              if (source.hasData()) {
                mConfig.mSourceFiles.push_back(source);
              }
              sourceEl = sourceEl->findNextSiblingElement("include");
            }
          }

          // put back the original configuration files
          for (auto iter = sources.begin(); iter != sources.end(); ++iter) {
            mConfig.mSourceFiles.push_back(*iter);
          }

          ElementPtr includesEl = rootEl->findFirstChildElement("sources");
          if (includesEl) {
            ElementPtr includeEl = includesEl->findFirstChildElement("source");
            while (includeEl) {
              auto source = UseHelper::getElementTextAndDecode(includeEl);

              if (source.hasData()) {
                mConfig.mSourceFiles.push_back(source);
              }
              includeEl = includeEl->findNextSiblingElement("source");
            }
          }

          while (mConfig.mSourceFiles.size() > 0)
          {
            String fileName = mConfig.mSourceFiles.front();
            mConfig.mSourceFiles.pop_front();

            SecureByteBlockPtr file;
            try {
              file = UseHelper::loadFile(fileName);
            } catch (const StdError &e) {
              ZS_THROW_CUSTOM_PROPERTIES_1(Failure, ZS_EVENTING_TOOL_FILE_FAILED_TO_LOAD, String("Failed to load main configuration file: ") + mConfig.mConfigFile + ", error=" + string(e.result()) + ", reason=" + e.message());
            }
            if (!file) {
              ZS_THROW_CUSTOM_PROPERTIES_1(Failure, ZS_EVENTING_TOOL_FILE_FAILED_TO_LOAD, String("Failed to load file: ") + fileName);
            }
            auto hashResult = UseHasher::hashAsString(file);
            auto found = processedHashes.find(hashResult);
            if (found != processedHashes.end()) {
              tool::output() << "[Info] Duplicate file found thus ignoring: " << fileName << "\n";
              continue;
            }
            const char *fileAsStr = reinterpret_cast<const char *>(file->BytePtr());
            auto isJSON = Helper::isLikelyJSON(fileAsStr);

            if (isJSON) {
              try {
                tool::output() << "\n[Info] Reading JSON configuration: " << fileName << "\n\n";
                auto rootEl = UseHelper::read(file);
                if (!rootEl) {
                  ZS_THROW_CUSTOM_PROPERTIES_1(Failure, ZS_EVENTING_TOOL_FILE_FAILED_TO_LOAD, String("Failed to load file as JSON: ") + fileName);
                }
                if (!project) {
                  project = Project::create(rootEl);
                } else {
                  project->parse(rootEl);
                }
              } catch (const InvalidContent &e) {
                ZS_THROW_CUSTOM_PROPERTIES_1(Failure, ZS_EVENTING_TOOL_INVALID_CONTENT, "Failed to parse JSON configuration: " + e.message());
              }
              continue;
            }

            if (!project) {
              ZS_THROW_CUSTOM_PROPERTIES_1(Failure, ZS_EVENTING_TOOL_INVALID_CONTENT, "Project configuration is missing!");
            }

            tool::output() << "\n[Info] Reading C/C++ source file: " << fileName << "\n\n";

            try {
              const char *pos = reinterpret_cast<const char *>(file->BytePtr());

              mTokenListStack = TokenListStack();

              pushTokens(make_shared<TokenList>());

              tokenize(pos, *getTokens());

              replaceAliases(*getTokens(), project->mAliases);

              if (!project->mGlobal) {
                project->mGlobal = Namespace::create(project);
              }

              parseNamespaceContents(project->mGlobal);

            } catch (const InvalidContent &e) {
              ZS_THROW_CUSTOM_PROPERTIES_1(Failure, ZS_EVENTING_TOOL_INVALID_CONTENT, "Invalid content found: " + e.message());
            } catch (const InvalidContentWithLine &e) {
              ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, e.lineNumber(), "Invalid content found: " + e.message());
            }
          }
        }

        //---------------------------------------------------------------------
        void IDLCompiler::validate() throw (Failure)
        {
          auto &project = mConfig.mProject;
          if (!project) return;

//          if (project->mUniqueHash.isEmpty()) {
//            project->mUniqueHash = project->uniqueEventingHash();
//          }
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::parseNamespace(NamespacePtr parent) throw (FailureWithLine)
        {
          auto token = peekNextToken("namespace");
          if (TokenType_Identifier != token->mTokenType) return false;
          if ("namespace" != token->mToken) return false;

          extractNextToken("namespace");  // skip "namespace"

          token = extractNextToken("namespace");
          
          if (TokenType_Identifier != token->mTokenType) {
            ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String("namespace missing identifier"));
          }
          
          String namespaceStr = token->mToken;

          token = extractNextToken("namespace");

          if ((TokenType_CurlyBrace != token->mTokenType) ||
              (token->isOpenBrace())) {
            ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String("namespace expecting \"{\""));
          }

          NamespacePtr namespaceObj;

          {
            auto found = parent->mNamespaces.find(namespaceStr);
            if (found == parent->mNamespaces.end()) {
              namespaceObj = Namespace::create(parent);
              namespaceObj->mName = namespaceStr;
              parent->mNamespaces[namespaceStr] = namespaceObj;
            } else {
              namespaceObj = (*found).second;
            }
          }

          fillContext(namespaceObj);

          parseNamespaceContents(namespaceObj);

          token = extractNextToken("namespace");

          if ((TokenType_CurlyBrace != token->mTokenType) ||
              (token->isCloseBrace())) {
            ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String("namespace expecting \"}\""));
          }

          return true;
        }

        //---------------------------------------------------------------------
        void IDLCompiler::parseNamespaceContents(NamespacePtr namespaceObj) throw (FailureWithLine)
        {
          while (hasMoreTokens()) {
            if (parseDocumentation()) continue;
            if (parseSemiColon()) continue;
            if (parseDirective()) continue;
            if (parseModifiers()) continue;
            if (parseNamespace(namespaceObj)) continue;
            if (parseUsing(namespaceObj)) continue;
            if (parseTypedef(namespaceObj)) continue;
            if (parseStruct(namespaceObj)) continue;
          }
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::parseUsing(NamespacePtr namespaceObj) throw (FailureWithLine)
        {
          const char *what = "using";
          auto token = peekNextToken(what);
          if (TokenType_Identifier != token->mTokenType) return false;

          if ("using" != token->mToken) return false;

          extractNextToken(what);  // skip "using"

          token = peekNextToken(what);
          if (TokenType_Identifier == token->mTokenType) {
            if ("namespace" == token->mToken) {
              extractNextToken(what);  // skip "namespace"

              // extract until ";" found
              String namespacePathStr;
              
              token = peekNextToken(what);
              while (TokenType_SemiColon != token->mTokenType) {
                extractNextToken(what); // skip it
                namespacePathStr += token->mToken;
              }
              
              auto foundNamespace = namespaceObj->findNamespace(namespacePathStr);
              if (!foundNamespace) {
                ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String("using namespace was not found:") + namespacePathStr);
              }

              processUsingNamespace(namespaceObj, foundNamespace);
              return true;
            }
          }

          // extract until ";" found
          String typePathStr;

          token = peekNextToken(what);
          while (TokenType_SemiColon != token->mTokenType) {
            extractNextToken(what); // skip it
            typePathStr += token->mToken;
          }

          auto foundType = namespaceObj->toContext()->findType(typePathStr);
          if (!foundType) {
            ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String("using type was not found:") + typePathStr);
          }

          processUsingType(namespaceObj, foundType);
          return true;
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::parseTypedef(ContextPtr context) throw (FailureWithLine)
        {
          const char *what = "typedef";
          auto token = peekNextToken(what);
          if (TokenType_Identifier != token->mTokenType) return false;
          
          if ("typedef" != token->mToken) return false;

          extractNextToken(what);  // skip "typedef"
          
          TokenList typeTokens;

          token = peekNextToken(what);
          while (TokenType_SemiColon != token->mTokenType) {
            typeTokens.push_back(extractNextToken(what));
          }

          if (typeTokens.size() < 2) {
            ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String("typedef typename was not found"));
          }

          TokenPtr lastToken = typeTokens.back();
          typeTokens.pop_back();
          
          if (TokenType_Identifier != lastToken->mTokenType) {
            ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String("typedef identifier was not found"));
          }
          
          String typeName = lastToken->mToken;
          processTypedef(context, typeTokens, typeName);
          return true;
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::parseStruct(ContextPtr context) throw (FailureWithLine)
        {
          const char *what = "interface/struct";
          
          bool isInterface {};

          auto token = peekNextToken(what);
          if (TokenType_Identifier != token->mTokenType) return false;

          bool foundTemplate = false;
          TokenList templateTokens;
          if ("template" == token->mToken) {
            foundTemplate = true;
            extractNextToken(what);  // skip "template"

            if (!extractToClosingBraceToken(what, templateTokens, false)) {
              ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " template expecting arguments");
            }

            token = peekNextToken(what);  // get type of struct/internface
          }

          if (("class" != token->mToken) &&
              ("interface" != token->mToken) &&
              ("interaction" != token->mToken) &&
              ("struct" != token->mToken)) {
            if (foundTemplate) {
              ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " template expecting keyword struct or interface");
            }
            return false;
          }
          
          extractNextToken(what); // skip "struct/interface" keyword

          isInterface = ("struct" == token->mToken ? false : true);

          token = extractNextToken(what);

          String structName = token->mToken;

          if (TokenType_Identifier != token->mTokenType) {
            ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " expecting name identifier");
          }

          token = peekNextToken(what);
          if (TokenType_SemiColon == token->mTokenType) {
            if (foundTemplate) {
              ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " template is missing template body");
            }
            processStructForward(context, structName);
            return true;
          }

          bool created {};
          auto newStruct = processStructForward(context, structName, &created);
          if (!created) {
            ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " struct/interface was not created: " + structName);
          }

          if (foundTemplate) {
            bool foundDefault = false;

            pushTokens(templateTokens);
            while (hasMoreTokens()) {
              if (parseComma()) continue;
              token = extractNextToken(what); // get generic name
              
              if (TokenType_Identifier != token->mTokenType) {
                ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " template expecting generic name");
              }

              auto genericType = GenericType::create(newStruct);
              genericType->mName = token->mToken;
              
              TypePtr defaultType;

              TokenList typeTokens;
              if (hasMoreTokens()) {
                token = peekNextToken(what);
                if (TokenType_EqualsOperator == token->mTokenType) {
                  extractNextToken(what); // skip "="
                  extractToComma(what, typeTokens);
                  TypedefTypePtr createdTypedef;
                  defaultType = findTypeOrCreateTypedef(newStruct, typeTokens, createdTypedef);
                  foundDefault = true;
                }
              }
              
              if ((foundDefault) &&
                  (!defaultType)) {
                ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " template expecting default type");
              }

              newStruct->mGenerics.push_back(genericType);
              newStruct->mGenericDefaultTypes.push_back(defaultType);
            }
            popTokens(); //templateTokens
          }

          token = extractNextToken(what);
          
          if (TokenType_ColonOperator == token->mTokenType) {
            extractNextToken(what); // skip ":"

            TokenList inheritTypeTokens;

            token = peekNextToken(what);
            while (TokenType_CurlyBrace != token->mTokenType) {
              if (parseComma()) {
                if (inheritTypeTokens.size() < 1) {
                  ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " expecting related type name");
                }
                processRelated(newStruct, inheritTypeTokens);
                inheritTypeTokens.clear();
                goto next;
              }

              inheritTypeTokens.push_back(token);
              extractNextToken(what); // skip token;

            next:
              {
                token = peekNextToken(what);
                continue;
              }
            }

            if (inheritTypeTokens.size() < 1) {
              ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " expecting related type name");
            }
            processRelated(newStruct, inheritTypeTokens);
          }

          if (TokenType_CurlyBrace != token->mTokenType) {
            ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " template expecting generic name");
          }
          
          TokenList structTokens;
          extractToClosingBraceToken(what, structTokens);
          
          pushTokens(structTokens);

          while (hasMoreTokens()) {
            if (parseDocumentation()) continue;
            if (parseSemiColon()) continue;
            if (parseDirective()) continue;
            if (parseModifiers()) continue;
            if (parseTypedef(newStruct)) continue;
            if (parseStruct(newStruct)) continue;
          }
          
          popTokens();  // structTokens

#define TODO 1
#define TODO 2

          return true;
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::parseDocumentation()
        {
          bool found = false;

          while (hasMoreTokens()) {
            auto token = peekNextToken("documentation");
            if (TokenType_Documentation != token->mTokenType) return found;

            found = true;
            mPendingDocumentation.push_back(extractNextToken("documentation"));
          }

          return found;
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::parseSemiColon()
        {
          auto token = peekNextToken(";");

          if (TokenType_SemiColon != token->mTokenType) return false;
          extractNextToken(";");
          return true;
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::parseComma()
        {
          const char *what = ",";
          auto token = peekNextToken(what);

          if (TokenType_CommaOperator != token->mTokenType) return false;
          extractNextToken(what);
          return true;
        }
        
        //---------------------------------------------------------------------
        bool IDLCompiler::parseModifiers() throw (FailureWithLine)
        {
          const char *what = "modifiers";
          
          auto token = peekNextToken(what);
          if (TokenType_SquareBrace != token->mTokenType) return false;
          
          TokenList allModifierTokens;
          extractToClosingBraceToken(what, allModifierTokens);

          pushTokens(allModifierTokens);
          
          while (hasMoreTokens()) {
            TokenList modifierTokens;
            extractToComma(what, modifierTokens);
            parseComma(); // skip over a comma

            pushTokens(modifierTokens);

            token = extractNextToken(what);
            if (TokenType_Identifier != token->mTokenType) {
              ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " expecting identifier");
            }
            
            String modiferNameStr = token->mToken;
            modiferNameStr.toLower();
            
            try {
              auto modifier = toModifier(modiferNameStr);
              auto totalParams = getTotalParams(modifier);

              StringList values;

              if (hasMoreTokens()) {
                TokenList modifierParamTokens;
                extractToClosingBraceToken(what, modifierParamTokens);

                pushTokens(modifierParamTokens);

                while (hasMoreTokens()) {
                  TokenList paramTokens;
                  extractToComma(what, paramTokens);

                  std::stringstream value;
                  bool added {false};

                  pushTokens(paramTokens);
                  while (hasMoreTokens()) {
                    auto token = extractNextToken(what);
                    if (added) {
                      value << " ";
                    }
                    value << token->mToken;
                    added = true;
                  }
                  popTokens();  // paramTokens

                  values.push_back(value.str());
                }
                popTokens(); // modifierParamTokens
              } else {
                if (0 != totalParams) {
                  ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " expecting parameters");
                }
              }
              
              if (-1 != totalParams) {
                if (totalParams != values.size()) {
                  ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " expecting total parameter mismatch: " + string(totalParams) + ", found=" + string(values.size()));
                }
              }

              if (mPendingModifiers.end() != mPendingModifiers.find(modiferNameStr)) {
                ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " modifier is already set:" + modiferNameStr);
              }
              mPendingModifiers[modiferNameStr] = values;

            } catch (const InvalidArgument &) {
              ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " modifier is not recognized:" + token->mToken);
            }

            popTokens();  // modifierTokens
          }

          popTokens();  // allModifierTokens
          return true;
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::parseDirective() throw (FailureWithLine)
        {
          const char *what = "directive";
          auto token = peekNextToken(what);

          if (TokenType_Directive != token->mTokenType) return false;
          extractNextToken(what);

          pushDirectiveTokens(token);

          {
            bool ignoreMode = false;
            do {
              if (!parseDirectiveExclusive(ignoreMode)) goto done;

              if (!ignoreMode) goto done;
              popTokens();

              while (hasMoreTokens()) {
                token = extractNextToken(what);
                if (pushDirectiveTokens(token)) goto check_exclusive_again;
              }
              break;

            check_exclusive_again:
              {
              }

            } while (ignoreMode);
          }

        done:
          {
          }

          popTokens();

          return true;
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::pushDirectiveTokens(TokenPtr token) throw (FailureWithLine)
        {
          if (!token) return false;
          if (TokenType_Directive != token->mTokenType) return false;

          TokenList tokens;
          tokenize(token->mToken.c_str(), tokens, token->mLineCount);

          pushTokens(tokens);
          return false;
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::parseDirectiveExclusive(bool &outIgnoreMode) throw (FailureWithLine)
        {
          const char *what = "Directive " ZS_WRAPPER_COMPILER_DIRECTIVE_EXCLUSIZE;
          auto token = peekNextToken(what);

          if (TokenType_Identifier != token->mTokenType) return false;
          if (ZS_WRAPPER_COMPILER_DIRECTIVE_EXCLUSIZE != token->mToken) return false;

          outIgnoreMode = true;

          extractNextToken(what);

          token = extractNextToken(what);
          if (TokenType_Identifier != token->mTokenType) {
            ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " expecting identifier");
          }

          String exclusiveId = token->mToken;

          if ((0 == exclusiveId.compareNoCase("x")) ||
              (mConfig.mProject->mDefinedExclusives.end() != mConfig.mProject->mDefinedExclusives.find(exclusiveId)))
          {
            outIgnoreMode = false;
          }
          return true;
        }

        //---------------------------------------------------------------------
        ElementPtr IDLCompiler::getDocumentation()
        {
          if (mPendingDocumentation.size() < 1) return ElementPtr();

          String resultStr = "<documentation>";
          bool first = true;
          while (mPendingDocumentation.size() > 0) {
            auto token = mPendingDocumentation.front();
            if (!first) {
              resultStr += " ";
            }

            resultStr += token->mToken;
            first = false;

            mPendingDocumentation.pop_front();
          }

          resultStr += "</documentation>";
          return UseHelper::toXML(resultStr);
        }

        //---------------------------------------------------------------------
        ElementPtr IDLCompiler::getDirectives()
        {
          if (mPendingDirectives.size() < 1) return ElementPtr();
          
          ElementPtr rootEl = Element::create("directives");

          while (mPendingDirectives.size() > 0) {
            auto el = mPendingDirectives.front();
            rootEl->adoptAsLastChild(el);
            mPendingDirectives.pop_front();
          }

          return rootEl;
        }

        //---------------------------------------------------------------------
        void IDLCompiler::mergeDocumentation(ElementPtr &existingDocumentation)
        {
          auto rootEl = getDocumentation();
          if (!rootEl) return;

          if (!existingDocumentation) {
            existingDocumentation = rootEl;
            return;
          }
          
          auto childEl = rootEl->getFirstChild();
          while (childEl) {
            auto nextEl = childEl->getNextSibling();
            childEl->orphan();
            existingDocumentation->adoptAsLastChild(childEl);
            childEl = nextEl;
          }
        }

        //---------------------------------------------------------------------
        void IDLCompiler::mergeDirectives(ElementPtr &existingDirectives)
        {
          if (mPendingDirectives.size() < 1) return;

          if (!existingDirectives) {
            existingDirectives = getDirectives();
            return;
          }

          while (mPendingDirectives.size() > 0) {
            auto el = mPendingDirectives.front();
            existingDirectives->adoptAsLastChild(el);
            mPendingDirectives.pop_front();
          }
        }

        //---------------------------------------------------------------------
        void IDLCompiler::mergeModifiers(ContextPtr context) throw (FailureWithLine)
        {
          const char *what = "merge modifiers";
          
          if (!context) return;
          
          for (auto iter = mPendingModifiers.begin(); iter != mPendingModifiers.end(); ++iter)
          {
            auto &name = (*iter).first;
            auto &values = (*iter).second;
            auto modifier = toModifier(name);
            if (context->hasModifier(modifier)) {
              ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_UNEXPECTED_EOF, getLastLineNumber(), String(what) + " has duplicate modifier: " + name);
            }
            context->setModifier(modifier, values);
          }

          mPendingModifiers.clear();
        }

        //---------------------------------------------------------------------
        void IDLCompiler::fillContext(ContextPtr context)
        {
          if (!context) return;
          mergeDocumentation(context->mDocumentation);
          mergeModifiers(context);
        }

        //---------------------------------------------------------------------
        String IDLCompiler::makeTypenameFromTokens(const TokenList &tokens) throw (InvalidContent)
        {
          String result;
          
          bool lastWasIdentifier = false;
          bool lastWasScope = false;
          
          for (auto iter = tokens.begin(); iter != tokens.end(); ++iter)
          {
            auto token = (*iter);
            if (TokenType_Identifier == token->mTokenType) {
              if (lastWasIdentifier) {
                ZS_THROW_CUSTOM(InvalidContent, "two identifiers found");
              }
              result += token->mToken;
              lastWasIdentifier = true;
              lastWasScope = false;
            } else if (TokenType_ScopeOperator == token->mTokenType) {
              if (lastWasScope) {
                ZS_THROW_CUSTOM(InvalidContent, "two scopes found");
              }
              result += token->mToken;
              lastWasIdentifier = false;
              lastWasScope = true;
            }
          }
          
          return result;
        }

        //---------------------------------------------------------------------
        void IDLCompiler::pushTokens(const TokenList &tokens)
        {
          mTokenListStack.push(make_shared<TokenList>(tokens));
          if (tokens.size() > 0) {
            mLastTokenStack.push(tokens.front());
          } else {
            mLastTokenStack.push(TokenPtr());
          }
        }

        //---------------------------------------------------------------------
        void IDLCompiler::pushTokens(TokenListPtr tokens)
        {
          mTokenListStack.push(tokens);
          if (tokens->size() > 0) {
            mLastTokenStack.push(tokens->front());
          } else {
            mLastTokenStack.push(TokenPtr());
          }
        }

        //---------------------------------------------------------------------
        IDLCompiler::TokenListPtr IDLCompiler::getTokens() const
        {
          if (mTokenListStack.size() < 1) return TokenListPtr();
          return mTokenListStack.top();
        }

        //---------------------------------------------------------------------
        IDLCompiler::TokenListPtr IDLCompiler::popTokens()
        {
          TokenListPtr result = mTokenListStack.top();

          mTokenListStack.pop();
          mLastTokenStack.pop();

          if (mLastTokenStack.size() > 0) {
            auto token = mLastTokenStack.top();
            if (token) mLastToken = token;
          }

          return result;
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::hasMoreTokens() const
        {
          if (mTokenListStack.size() < 1) return false;
          if (getTokens()->size() < 1) return false;
          return true;
        }

        //---------------------------------------------------------------------
        TokenPtr IDLCompiler::peekNextToken(const char *whatExpectingMoreTokens) throw (FailureWithLine)
        {
          if (mTokenListStack.size() > 0) {
            if (getTokens()->size() > 0) return getTokens()->front();
          }

          TokenPtr lastToken;
          if (mLastTokenStack.size() > 0) {
            mLastTokenStack.top();
          } else {
            lastToken = mLastToken;
          }

          ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_UNEXPECTED_EOF, lastToken ? lastToken->mLineCount : 0, String(whatExpectingMoreTokens) + " unexpectedly reached EOF");
          return TokenPtr();
        }

        //---------------------------------------------------------------------
        TokenPtr IDLCompiler::extractNextToken(const char *whatExpectingMoreTokens) throw (FailureWithLine)
        {
          if (mTokenListStack.size() > 0) {
            if (getTokens()->size() > 0) {
              mLastToken = getTokens()->front();
              mLastTokenStack.pop();
              mLastTokenStack.push(mLastToken);
              getTokens()->pop_front();
              return mLastToken;
            }
          }

          TokenPtr lastToken;
          if (mLastTokenStack.size() > 0) {
            mLastTokenStack.top();
          } else {
            lastToken = mLastToken;
          }

          ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_UNEXPECTED_EOF, lastToken ? lastToken->mLineCount : 0, String(whatExpectingMoreTokens) + " unexpectedly reached EOF");
          return TokenPtr();
        }

        //---------------------------------------------------------------------
        void IDLCompiler::putBackToken(TokenPtr token)
        {
          if (mTokenListStack.size() < 1) {
            ZS_THROW_INVALID_USAGE("must have active stack of tokens");
          }

          auto tokens = getTokens();
          tokens->push_front(token);

          mLastToken = token;
          mLastTokenStack.pop();
          mLastTokenStack.push(token);
        }

        //---------------------------------------------------------------------
        void IDLCompiler::putBackTokens(const TokenList &tokens)
        {
          if (mTokenListStack.size() < 1) {
            ZS_THROW_INVALID_USAGE("must have active stack of tokens");
          }

          auto existingTokens = getTokens();
          
          insertBefore(*existingTokens, tokens);
          
          TokenPtr firstToken;
          if (existingTokens->size() > 0) {
            firstToken = existingTokens->front();
            mLastToken = firstToken;
          }

          mLastTokenStack.pop();
          mLastTokenStack.push(firstToken);
        }

        //---------------------------------------------------------------------
        ULONG IDLCompiler::getLastLineNumber() const
        {
          if (!mLastToken) return 1;
          return mLastToken->mLineCount;
        }

        //---------------------------------------------------------------------
        void IDLCompiler::insertBefore(
                                           TokenList &tokens,
                                           const TokenList &insertTheseTokens
                                           )
        {
          if (tokens.size() < 1) {
            tokens = insertTheseTokens;
            return;
          }
          
          for (auto iter = insertTheseTokens.rbegin(); iter != insertTheseTokens.rend(); ++iter)
          {
            tokens.push_front(*iter);
          }
        }
        
        //---------------------------------------------------------------------
        void IDLCompiler::insertAfter(
                                          TokenList &tokens,
                                          const TokenList &insertTheseTokens
                                          )
        {
          if (tokens.size() < 1) {
            tokens = insertTheseTokens;
            return;
          }
          
          for (auto iter = insertTheseTokens.begin(); iter != insertTheseTokens.end(); ++iter)
          {
            tokens.push_back(*iter);
          }
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::extractToClosingBraceToken(
                                                         const char *whatExpectingClosingToken,
                                                         TokenList &outTokens,
                                                         bool includeOuterBrace
                                                         ) throw (FailureWithLine)
        {
          auto token = peekNextToken(whatExpectingClosingToken);
          if (!token->isBrace()) return false;
          if (!token->isOpenBrace()) return false;


          size_t countBrace = 0;
          size_t countCurly = 0;
          size_t countSquare = 0;
          size_t countAngle = 0;

          do {
            token = extractNextToken(whatExpectingClosingToken);
            outTokens.push_back(token);

            if (token->isBrace()) {
              if (token->isOpenBrace()) {
                switch (token->mTokenType) {
                  case TokenType_Brace:       ++countBrace; break;
                  case TokenType_CurlyBrace:  ++countCurly; break;
                  case TokenType_SquareBrace: ++countSquare; break;
                  case TokenType_AngleBrace:  ++countAngle; break;
                  default:                    break;
                }
              } else {
                switch (token->mTokenType) {
                  case TokenType_Brace:       if (countBrace < 1) goto brace_mismatch; --countBrace; break;
                  case TokenType_CurlyBrace:  if (countCurly < 1) goto brace_mismatch; --countCurly; break;
                  case TokenType_SquareBrace: if (countSquare < 1) goto brace_mismatch; --countSquare; break;
                  case TokenType_AngleBrace:  if (countAngle < 1) goto brace_mismatch; --countAngle; break;
                  default:                    break;
                }
              }
            }
          } while ((countBrace > 0) ||
                   (countCurly > 0) ||
                   (countSquare > 0) ||
                   (countAngle > 0));

          {
            goto done;
          }

        brace_mismatch:
          {
            ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(whatExpectingClosingToken) + " brace mismatch");
          }

        done:
          {
            if (!includeOuterBrace) {
              if (outTokens.size() > 1) {
                outTokens.pop_front();
                outTokens.pop_back();
              }
            }
          }

          return true;
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::extractToComma(
                                             const char *whatExpectingComma,
                                             TokenList &outTokens
                                             ) throw (FailureWithLine)
        {
          return extractToTokenType(whatExpectingComma, TokenType_CommaOperator, outTokens);
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::extractToEquals(
                                              const char *whatExpectingComma,
                                              TokenList &outTokens
                                              ) throw (FailureWithLine)
        {
          return extractToTokenType(whatExpectingComma, TokenType_EqualsOperator, outTokens);
        }

        //---------------------------------------------------------------------
        bool IDLCompiler::extractToTokenType(
                                                 const char *whatExpectingComma,
                                                 TokenTypes searchTokenType,
                                                 TokenList &outTokens,
                                                 bool includeFoundToken,
                                                 bool processBrackets
                                                 ) throw (FailureWithLine)
        {
          while (hasMoreTokens()) {
            auto token = extractNextToken(whatExpectingComma);
            if (searchTokenType == token->mTokenType) {
              if (!includeFoundToken) {
                putBackToken(token);
              }
              break;
            }
            
            if ((processBrackets) &&
                (token->isBrace())) {
              putBackToken(token);
              if (token->isCloseBrace()) return true;

              TokenList braceTokens;
              extractToClosingBraceToken(whatExpectingComma, braceTokens, true);
              for (auto iter = braceTokens.begin(); iter != braceTokens.end(); ++iter) {
                outTokens.push_back(*iter);
              }
              continue;
            }
            outTokens.push_back(token);
          }
          return true;
        }

        //---------------------------------------------------------------------
        void IDLCompiler::processUsingNamespace(
                                                    NamespacePtr currentNamespace,
                                                    NamespacePtr usingNamespace
                                                    )
        {
          if (currentNamespace == usingNamespace) return;

          for (auto iter = usingNamespace->mEnums.begin(); iter != usingNamespace->mEnums.end(); ++iter)
          {
            auto name = (*iter).first;
            auto type = (*iter).second;
            
            auto found = currentNamespace->mTypedefs.find(name);
            if (found != currentNamespace->mTypedefs.end()) continue;
            
            auto newTypedef = TypedefType::create(currentNamespace);
            newTypedef->mName = name;
            newTypedef->mOriginalType = type;
            currentNamespace->mTypedefs[name] = newTypedef;
          }

          for (auto iter = usingNamespace->mStructs.begin(); iter != usingNamespace->mStructs.end(); ++iter)
          {
            auto name = (*iter).first;
            auto type = (*iter).second;
            
            auto found = currentNamespace->mTypedefs.find(name);
            if (found != currentNamespace->mTypedefs.end()) continue;

            auto newTypedef = TypedefType::create(currentNamespace);
            newTypedef->mName = name;
            newTypedef->mOriginalType = type->getTypeBypassingTypedefIfNoop();
            currentNamespace->mTypedefs[name] = newTypedef;
          }

          for (auto iter = usingNamespace->mTypedefs.begin(); iter != usingNamespace->mTypedefs.end(); ++iter)
          {
            auto name = (*iter).first;
            auto type = (*iter).second;

            auto found = currentNamespace->mTypedefs.find(name);
            if (found != currentNamespace->mTypedefs.end()) continue;

            auto newTypedef = TypedefType::create(currentNamespace);
            newTypedef->mName = name;
            newTypedef->mOriginalType = type;
            currentNamespace->mTypedefs[name] = newTypedef;
          }
        }

        //---------------------------------------------------------------------
        void IDLCompiler::processUsingType(
                                               NamespacePtr currentNamespace,
                                               TypePtr usingType
                                               )
        {
          usingType = usingType->getTypeBypassingTypedefIfNoop();

          auto name = usingType->getMappingName();

          auto found = currentNamespace->mTypedefs.find(name);
          if (found != currentNamespace->mTypedefs.end()) return;

          auto newTypedef = TypedefType::create(currentNamespace);
          newTypedef->mName = name;
          newTypedef->mOriginalType = usingType;
          currentNamespace->mTypedefs[name] = newTypedef;
        }

        //---------------------------------------------------------------------
        void IDLCompiler::processTypedef(
                                         ContextPtr context,
                                         const TokenList &typeTokens,
                                         const String &typeName
                                         ) throw (FailureWithLine)
        {
          TypedefTypePtr createdTypedef;
          auto type = findTypeOrCreateTypedef(context, typeTokens, createdTypedef);

          TypePtr originalType = type;
          
          if (!createdTypedef) {
            createdTypedef = TypedefType::create(context);
            createdTypedef->mOriginalType = type;
            type = createdTypedef;
          } else {
            originalType = createdTypedef->mOriginalType.lock();
          }
          
          if (!originalType) {
            ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String("typedef original type was not found"));
          }
          
          createdTypedef->mName = typeName;
          fillContext(createdTypedef);
          
          {
            auto namespaceObj = context->toNamespace();
            if (namespaceObj) {
              
              auto found = namespaceObj->mTypedefs.find(typeName);
              if (found != namespaceObj->mTypedefs.end()) return;  // assume types are the same
              namespaceObj->mTypedefs[createdTypedef->getMappingName()] = createdTypedef;
              return;
            }
          }
          
          {
            auto structObj = context->toStruct();
            if (structObj) {
              auto found = structObj->mTypedefs.find(typeName);
              if (found != structObj->mTypedefs.end()) return; // asumme types are the same
              structObj->mTypedefs[createdTypedef->getMappingName()] = createdTypedef;
              return;
            }
          }
          
          ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String("typedef found in context that does not allow typedefs"));
        }

        //---------------------------------------------------------------------
        void IDLCompiler::processRelated(
                                         StructPtr structObj,
                                         const TokenList &typeTokens
                                         ) throw (FailureWithLine)
        {
          const char *what = "struct/interface inherited";

          TypedefTypePtr typedefType;
          auto type = findTypeOrCreateTypedef(structObj, typeTokens, typedefType);

          if (!type) {
            ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " related type was not found");
          }
        }
        
        //---------------------------------------------------------------------
        IDLCompiler::StructPtr IDLCompiler::processStructForward(
                                                                 ContextPtr context,
                                                                 const String &typeName,
                                                                 bool *wasCreated
                                                                 ) throw (FailureWithLine)
        {
          if (wasCreated) *wasCreated = false;
          
          {
            auto namespaceObj = context->toNamespace();
            if (namespaceObj) {
              auto found = namespaceObj->mStructs.find(typeName);
              if (found != namespaceObj->mStructs.end()) {
                auto result = (*found).second;
                fillContext(result);
                return (*found).second;
              }
              
              if (wasCreated) *wasCreated = true;
              StructPtr structObj = Struct::create(context);
              structObj->mName = typeName;
              fillContext(structObj);
              
              namespaceObj->mStructs[structObj->getMappingName()] = structObj;
              return structObj;
            }
          }
          
          {
            auto outerStructObj = context->toStruct();
            if (outerStructObj) {
              auto found = outerStructObj->mStructs.find(typeName);
              if (found != outerStructObj->mStructs.end()) {
                auto result = (*found).second;
                fillContext(result);
                return (*found).second;
              }
              
              if (wasCreated) *wasCreated = true;
              StructPtr structObj = Struct::create(context);
              structObj->mName = typeName;
              fillContext(structObj);

              outerStructObj->mStructs[structObj->getMappingName()] = structObj;
              return structObj;
            }
          }

          ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String("struct/class forward not attached to namespace or struct context"));
          return StructPtr();
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IDLCompilerHelper
        #pragma mark
        
        class IDLCompilerHelper : protected IDLCompiler
        {
        public:
          struct FoundBasicTypeModifiers
          {
            bool mAnyBasicTypeModifiers {false};
            bool mAnyOtherModifier {false};
            
            bool mSigned {false};
            bool mUnsigned {false};
            bool mChar {false};
            bool mShort {false};
            bool mInt {false};
            size_t mTotalLongs {false};
            bool mFloat {false};
            bool mDouble {false};

            bool mConst {false};

            bool mLastWasTypename {false};
            bool mLastWasScope {false};
            
            String mTypeName;
            
            //-----------------------------------------------------------------
            void throwInvalidModifier() throw (InvalidContent)
            {
              ZS_THROW_CUSTOM(InvalidContent, "has invalid type modifier");
            }
            
            //-----------------------------------------------------------------
            void insert(const String &modifierStr) throw (InvalidContent)
            {
              if ("signed" == modifierStr) {
                if (mUnsigned || mSigned || mFloat || mDouble) throwInvalidModifier();
                mSigned = true;
                mAnyBasicTypeModifiers = true;
                return;
              }
              if ("unsigned" == modifierStr) {
                if (mUnsigned || mSigned || mFloat || mDouble) throwInvalidModifier();
                mUnsigned = true;
                mAnyBasicTypeModifiers = true;
                return;
              }
              if ("long" == modifierStr) {
                if ((mTotalLongs > 1) || mChar || mShort || mFloat) throwInvalidModifier();
                if ((mTotalLongs > 1) && (mDouble)) throwInvalidModifier();
                ++mTotalLongs;
                mAnyBasicTypeModifiers = true;
                return;
              }
              if ("char" == modifierStr) {
                if ((mTotalLongs > 0) || mChar || mShort || mInt || mFloat || mDouble) throwInvalidModifier();
                mChar = true;
                mAnyBasicTypeModifiers = true;
                return;
              }
              if ("short" == modifierStr) {
                if ((mTotalLongs > 0) || mChar || mShort || mFloat || mDouble) throwInvalidModifier();
                mShort = true;
                mAnyBasicTypeModifiers = true;
                return;
              }
              if ("int" == modifierStr) {
                if (mChar || mInt || mFloat || mDouble) throwInvalidModifier();
                mInt = true;
                mAnyBasicTypeModifiers = true;
                return;
              }
              if ("long" == modifierStr) {
                if ((mTotalLongs > 1) || mChar || mShort || mFloat) throwInvalidModifier();
                if ((mTotalLongs > 0) && mDouble) throwInvalidModifier();
                ++mTotalLongs;
                mAnyBasicTypeModifiers = true;
                return;
              }
              if ("float" == modifierStr) {
                if (mSigned || mUnsigned || (mTotalLongs > 0) || mChar || mInt || mFloat || mDouble) throwInvalidModifier();
                mFloat = true;
                mAnyBasicTypeModifiers = true;
                return;
              }
              if ("double" == modifierStr) {
                if (mSigned || mUnsigned || (mTotalLongs > 1) || mChar || mInt || mFloat || mDouble) throwInvalidModifier();
                mDouble = true;
                mAnyBasicTypeModifiers = true;
                return;
              }
              if ("const" == modifierStr) {
                if (mConst) throwInvalidModifier();
                mConst = true;
                mAnyOtherModifier = true;
                return;
              }

              if (mTypeName.hasData()) {
                ZS_THROW_CUSTOM(InvalidContent, "has type name redeclared");
              }
              if (mLastWasTypename) throwInvalidModifier();
              mLastWasTypename = true;
              mLastWasScope = false;
              mTypeName += modifierStr;
            }
            
            //-----------------------------------------------------------------
            void insertScope() throw (InvalidContent)
            {
              if (mLastWasScope) throwInvalidModifier();
              mLastWasTypename = false;
              mLastWasScope = true;
              mTypeName += "::";
            }
            
            //-----------------------------------------------------------------
            PredefinedTypedefs mergePredefined(PredefinedTypedefs existingBasicType) throw (InvalidContent)
            {
              PredefinedTypedefs &newBasicType = existingBasicType;
              
              switch (existingBasicType) {
                case PredefinedTypedef_void:
                {
                  if (mAnyBasicTypeModifiers) throwInvalidModifier();
                  break;
                }
                case PredefinedTypedef_bool:
                {
                  if (mAnyBasicTypeModifiers) throwInvalidModifier();
                  break;
                }

                case PredefinedTypedef_uchar: {
                  if (mSigned || mChar || mShort || mInt || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  break;
                }
                case PredefinedTypedef_char: {
                  if (mChar || mShort || mInt || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  if (mSigned) newBasicType = PredefinedTypedef_schar;
                  if (mUnsigned) newBasicType = PredefinedTypedef_uchar;
                  break;
                }
                case PredefinedTypedef_schar:
                {
                  if (mUnsigned || mChar || mShort || mInt || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  break;
                }
                case PredefinedTypedef_ushort:
                {
                  if (mSigned || mChar || mShort || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  mInt = false;
                  break;
                }
                case PredefinedTypedef_short: {
                  if (mChar || mShort || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  mInt = false;
                  if (mSigned) newBasicType = PredefinedTypedef_sshort;
                  if (mUnsigned) newBasicType = PredefinedTypedef_ushort;
                  break;
                }
                case PredefinedTypedef_sshort:
                {
                  if (mUnsigned || mChar || mShort || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  mInt = false;
                  break;
                }
                case PredefinedTypedef_uint:
                {
                  if (mSigned || mChar || mInt || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  if (mShort) newBasicType = PredefinedTypedef_ushort;
                  break;
                }
                case PredefinedTypedef_int:
                {
                  if (mChar || mInt || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  if (mShort) {
                    if (mSigned)
                      newBasicType = PredefinedTypedef_sshort;
                    else if (mUnsigned)
                      newBasicType = PredefinedTypedef_ushort;
                    else
                      newBasicType = PredefinedTypedef_short;
                  } else {
                    if (mSigned) newBasicType = PredefinedTypedef_sint;
                    if (mUnsigned) newBasicType = PredefinedTypedef_uint;
                  }
                  break;
                }
                case PredefinedTypedef_sint:
                {
                  if (mUnsigned || mChar || mInt || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  if (mShort) newBasicType = PredefinedTypedef_sshort;
                  break;
                }
                case PredefinedTypedef_ulong:
                {
                  if (mSigned || mChar || mShort || (mTotalLongs > 1) || mFloat || mDouble) throwInvalidModifier();
                  mInt = false;
                  if (mTotalLongs > 0) newBasicType = PredefinedTypedef_ulonglong;
                  break;
                }
                case PredefinedTypedef_long:
                {
                  if (mChar || mShort || (mTotalLongs > 1) || mFloat || mDouble) throwInvalidModifier();
                  mInt = false;
                  if (mTotalLongs > 0) {
                    if (mSigned)
                      newBasicType = PredefinedTypedef_slonglong;
                    else if (mUnsigned)
                      newBasicType = PredefinedTypedef_ulonglong;
                    else
                      newBasicType = PredefinedTypedef_longlong;
                  } else {
                    if (mSigned) newBasicType = PredefinedTypedef_slong;
                    if (mUnsigned) newBasicType = PredefinedTypedef_ulong;
                  }
                  break;
                }
                case PredefinedTypedef_slong:
                {
                  if (mUnsigned || mChar || mShort || (mTotalLongs > 1) || mFloat || mDouble) throwInvalidModifier();
                  mInt = false;
                  if (mTotalLongs > 0) newBasicType = PredefinedTypedef_slonglong;
                  break;
                }
                case PredefinedTypedef_ulonglong:
                {
                  if (mSigned || mChar || mShort || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  mInt = false;
                  break;
                }
                case PredefinedTypedef_longlong:
                {
                  if (mChar || mShort || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  mInt = false;
                  if (mSigned) newBasicType = PredefinedTypedef_slonglong;
                  if (mUnsigned) newBasicType = PredefinedTypedef_ulonglong;
                  break;
                }
                case PredefinedTypedef_slonglong:
                {
                  if (mUnsigned || mChar || mShort || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  mInt = false;
                  break;
                }
                case PredefinedTypedef_uint8:
                case PredefinedTypedef_uint16:
                case PredefinedTypedef_uint32:
                case PredefinedTypedef_uint64:
                case PredefinedTypedef_byte:
                case PredefinedTypedef_word:
                case PredefinedTypedef_dword:
                case PredefinedTypedef_qword:
                {
                  if (mSigned || mChar || mShort || mInt || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  break;
                }
                case PredefinedTypedef_int8:
                {
                  if (mChar || mShort || mInt || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  if (mSigned) newBasicType = PredefinedTypedef_sint8;
                  if (mUnsigned) newBasicType = PredefinedTypedef_uint8;
                  break;
                }
                case PredefinedTypedef_sint8:
                case PredefinedTypedef_sint16:
                case PredefinedTypedef_sint32:
                case PredefinedTypedef_sint64:
                {
                  if (mUnsigned || mChar || mShort || mInt || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  break;
                }
                case PredefinedTypedef_int16:
                {
                  if (mChar || mShort || mInt || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  if (mSigned) newBasicType = PredefinedTypedef_sint16;
                  if (mUnsigned) newBasicType = PredefinedTypedef_uint16;
                  break;
                }
                case PredefinedTypedef_int32:
                {
                  if (mChar || mShort || mInt || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  if (mSigned) newBasicType = PredefinedTypedef_sint32;
                  if (mUnsigned) newBasicType = PredefinedTypedef_uint32;
                  break;
                }
                case PredefinedTypedef_int64:
                {
                  if (mChar || mShort || mInt || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  if (mSigned) newBasicType = PredefinedTypedef_sint64;
                  if (mUnsigned) newBasicType = PredefinedTypedef_uint64;
                  break;
                }
                  
                case PredefinedTypedef_float:
                case PredefinedTypedef_float32:
                case PredefinedTypedef_float64:
                {
                  if (mSigned || mUnsigned || mChar || mShort || mInt || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  break;
                }
                case PredefinedTypedef_double:
                {
                  if (mSigned || mUnsigned || mChar || mShort || mInt || (mTotalLongs > 1) || mFloat || mDouble) throwInvalidModifier();
                  if (mTotalLongs > 0) newBasicType = PredefinedTypedef_ldouble;
                  break;
                }
                case PredefinedTypedef_ldouble:
                {
                  if (mSigned || mUnsigned || mChar || mShort || mInt || (mTotalLongs > 0) || mFloat || mDouble) throwInvalidModifier();
                  break;
                }
                case PredefinedTypedef_pointer:
                case PredefinedTypedef_binary:
                case PredefinedTypedef_size:
                case PredefinedTypedef_string:
                case PredefinedTypedef_astring:
                case PredefinedTypedef_wstring: throwInvalidModifier();
              }
              return newBasicType;
            }

            //-----------------------------------------------------------------
            PredefinedTypedefs getBasicType()
            {
              if (mChar) {
                if (mUnsigned) return PredefinedTypedef_uchar;
                if (mSigned) return PredefinedTypedef_schar;
                return PredefinedTypedef_char;
              }
              if (mShort) {
                if (mUnsigned) return PredefinedTypedef_ushort;
                if (mSigned) return PredefinedTypedef_sshort;
                return PredefinedTypedef_short;
              }
              if (mFloat) return PredefinedTypedef_float;
              if (mDouble) {
                if (mTotalLongs > 0) return PredefinedTypedef_ldouble;
                return PredefinedTypedef_double;
              }
              
              if (mTotalLongs > 1) {
                if (mUnsigned) return PredefinedTypedef_ulonglong;
                if (mSigned) return PredefinedTypedef_slonglong;
                return PredefinedTypedef_longlong;
              }

              if (mTotalLongs > 0) {
                if (mUnsigned) return PredefinedTypedef_ulong;
                if (mSigned) return PredefinedTypedef_slong;
                return PredefinedTypedef_long;
              }
              
              if (mInt) {
                if (mUnsigned) return PredefinedTypedef_uint;
                if (mSigned) return PredefinedTypedef_sint;
                return PredefinedTypedef_int;
              }
              ZS_THROW_CUSTOM(InvalidContent, "is not a basic type");
              return PredefinedTypedef_int;
            }

            //-----------------------------------------------------------------
            TypePtr processType(
                                ContextPtr context,
                                TypedefTypePtr &outCreatedTypedef
                                ) throw (InvalidContent)
            {
              if ((mShort) && (mInt)) mInt = false; // strip redundant information

              if (mTypeName.hasData()) {
                auto existingType = context->findType(mTypeName);
                if (!existingType) throwInvalidModifier();

                BasicTypePtr basicType;
                TypedefTypePtr typedefObj = existingType ? existingType->toTypedefType() : TypedefTypePtr();
                
                if (typedefObj) {
                  typedefObj->resolveTypedefs();
                }

                while (typedefObj) {
                  TypePtr foundType = typedefObj->mOriginalType.lock();
                  
                  if (foundType) {
                    basicType = foundType->toBasicType();
                    typedefObj = foundType->toTypedefType();
                  }
                }

                if (basicType) {
                  outCreatedTypedef = TypedefType::create(context);

                  if (typedefObj) {
                    outCreatedTypedef->mModifiers = typedefObj->mModifiers;
                  }

                  PredefinedTypedefs newBasicType = mergePredefined(basicType->mBaseType);
                  auto foundNewBasicType = context->findType(IEventingTypes::toString(newBasicType));
                  if (!foundNewBasicType) {
                    ZS_THROW_CUSTOM(InvalidContent, "did not find new basic type");
                  }
                  outCreatedTypedef->mOriginalType = foundNewBasicType;
                  return outCreatedTypedef;
                }

                if (mAnyBasicTypeModifiers) throwInvalidModifier();
                if (!mAnyOtherModifier) return existingType;

                outCreatedTypedef = TypedefType::create(context);
                outCreatedTypedef->mOriginalType = existingType;
                outCreatedTypedef->resolveTypedefs();
                return outCreatedTypedef;
              }

              if (!mAnyBasicTypeModifiers) throwInvalidModifier();

              auto predefinedType = getBasicType();
              auto existingBasicType = context->findType(IEventingTypes::toString(predefinedType));
              if (!existingBasicType) {
                ZS_THROW_CUSTOM(InvalidContent, "did not find basic type");
              }

              if (mAnyOtherModifier) {
                outCreatedTypedef = TypedefType::create(context);
                outCreatedTypedef->mOriginalType = existingBasicType;
                return outCreatedTypedef;
              }
              return existingBasicType;
            }
            
          };
        };

        //---------------------------------------------------------------------
        IDLCompiler::TypePtr IDLCompiler::findTypeOrCreateTypedef(
                                                                  ContextPtr context,
                                                                  const TokenList &inTokens,
                                                                  TypedefTypePtr &outCreatedTypedef
                                                                  ) throw (FailureWithLine)
        {
          TypePtr result;

          const char *what = "Type search";

          TokenList pretemplateTokens;

          TypeList templateTypes;

          {
            // search for template parameters
            pushTokens(inTokens);

            while (hasMoreTokens()) {
              auto token = extractNextToken(what);
              pretemplateTokens.push_back(token);

              if (TokenType_AngleBrace == token->mTokenType) {
                putBackToken(token);
                TokenList templateContents;
                extractToClosingBraceToken(what, templateContents);

                pushTokens(templateContents);

                while (hasMoreTokens()) {
                  parseComma(); // skip over a comma

                  TokenList templateTypeTokens;
                  extractToComma(what, templateTypeTokens);

                  TypedefTypePtr typedefObj;
                  auto foundType = findTypeOrCreateTypedef(context, templateTypeTokens, typedefObj);
                  templateTypes.push_back(foundType);
                }

                popTokens();
                break;
              }
            }

            popTokens();
          }

          try
          {
            pushTokens(pretemplateTokens);
            
            IDLCompilerHelper::FoundBasicTypeModifiers modifiers;

            while (hasMoreTokens()) {
              auto token = extractNextToken(what);
              switch (token->mTokenType) {
                case TokenType_Identifier: {
                  modifiers.insert(token->mToken);
                  continue;
                }
                case TokenType_ScopeOperator: {
                  modifiers.insertScope();
                  continue;
                }
                default:
                {
                  ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " has not legal type modifier");
                  break;
                }
              }
            }
            popTokens();

            result = modifiers.processType(context, outCreatedTypedef);

            {
              auto originalType = result;

              auto typedefObj = result->toTypedefType();
              if (typedefObj) {
                originalType = typedefObj->mOriginalType.lock();
              }

              auto structObj = originalType->toStruct();
              if (structObj) {
#define TODO 1
#define TODO 2
#if 0
                if (hasModifier(structObj->mModifiers, StructModifier_Generic)) {
                  auto newTemplateStruct = Struct::createFromTemplate(structObj, templateParams);
                  if (outCreatedTypedef) {
                    result = typedefObj = createTypedefToNewTemplateFromExistingTypedef(outCreatedTypedef, newTemplateStruct);
                  } if (!typedefObj) {
                    result = newTemplateStruct;
                  } else {
                    // A typedef to a generic template was found and this
                    // shouldn't be legal but perhaps a use case was missed...
                    ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " is a typedef to a template but typedef to generatic isn't allowed in this context");
                  }
                } else {
                  if (templateTypes.size() > 0) {
                    ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " has template parameters but type referenced isn't a generic template");
                  }
                }
#endif //0
              } else {
                if (templateTypes.size() > 0) {
                  ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " has template parameters but type referenced isn't a struct or generic template");
                }
              }
            }
            
          } catch (const InvalidContent &e) {
            ZS_THROW_CUSTOM_PROPERTIES_2(FailureWithLine, ZS_EVENTING_TOOL_INVALID_CONTENT, getLastLineNumber(), String(what) + " " + e.message());
          }

          return result->getTypeBypassingTypedefIfNoop();
        }

#if 0
        //---------------------------------------------------------------------
        IDLCompiler::TypedefTypePtr IDLCompiler::createTypedefToNewTemplateFromExistingTypedef(
                                                                                                       TypedefTypePtr existingTypedefObj,
                                                                                                       StructPtr newTemplate
                                                                                                       ) throw (InvalidContent)
        {
          auto newTypedefObj = TypedefType::create(existingTypedefObj);

          newTypedefObj->mModifiers = existingTypedefObj->mModifiers;
          newTypedefObj->mArraySize = existingTypedefObj->mArraySize;
          newTypedefObj->mName = existingTypedefObj->mName;
          if (existingTypedefObj->mDocumentation) {
            newTypedefObj->mDocumentation = existingTypedefObj->mDocumentation->clone()->toElement();
          }
          if (existingTypedefObj->mDirectives) {
            newTypedefObj->mDirectives = existingTypedefObj->mDirectives->clone()->toElement();
          }

          newTypedefObj->mOriginalType = newTemplate;
          return newTypedefObj;
        }
#endif //0
        
        //---------------------------------------------------------------------
        void IDLCompiler::writeXML(const String &outputName, const DocumentPtr &doc) const throw (Failure)
        {
          if (!doc) return;
          try {
            auto output = UseHelper::writeXML(*doc);
            UseHelper::saveFile(outputName, *output);
          } catch (const StdError &e) {
            ZS_THROW_CUSTOM_PROPERTIES_1(Failure, ZS_EVENTING_TOOL_SYSTEM_ERROR, "Failed to save XML file \"" + outputName + "\": " + " error=" + string(e.result()) + ", reason=" + e.message());
          }
        }

        //---------------------------------------------------------------------
        void IDLCompiler::writeJSON(const String &outputName, const DocumentPtr &doc) const throw (Failure)
        {
          if (!doc) return;
          try {
            auto output = UseHelper::writeJSON(*doc);
            UseHelper::saveFile(outputName, *output);
          } catch (const StdError &e) {
            ZS_THROW_CUSTOM_PROPERTIES_1(Failure, ZS_EVENTING_TOOL_SYSTEM_ERROR, "Failed to save JSON file \"" + outputName + "\": " + " error=" + string(e.result()) + ", reason=" + e.message());
          }
        }

        //---------------------------------------------------------------------
        void IDLCompiler::writeBinary(const String &outputName, const SecureByteBlockPtr &buffer) const throw (Failure)
        {
          if ((!buffer) ||
              (0 == buffer->SizeInBytes())) {
            ZS_THROW_CUSTOM_PROPERTIES_1(Failure, ZS_EVENTING_TOOL_SYSTEM_ERROR, "Failed to save file \"" + outputName + "\": file is empty");
          }
          try {
            UseHelper::saveFile(outputName, *buffer);
          } catch (const StdError &e) {
            ZS_THROW_CUSTOM_PROPERTIES_1(Failure, ZS_EVENTING_TOOL_SYSTEM_ERROR, "Failed to save file \"" + outputName + "\": " + " error=" + string(e.result()) + ", reason=" + e.message());
          }
        }

      } // namespace internal
    } // namespace tool
  } // namespace eventing
} // namespace zsLib