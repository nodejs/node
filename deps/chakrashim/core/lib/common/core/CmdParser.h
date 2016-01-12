//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once



///----------------------------------------------------------------------------
///----------------------------------------------------------------------------
///
/// class CmdLineArgsParser
///
/// Parses the following grammar
///
///      range      = integer | integer - integer | range,range
///      parameter  = integer | string  | phase[:range]
///      flag       = string
///      phase      = string
///      [-|/]flag[:parameter]
///
///----------------------------------------------------------------------------
///----------------------------------------------------------------------------

class CmdLineArgsParser : private ICmdLineArgsParser
{
// Data
private:
    static const int  MaxTokenSize  = 512;

    Js::ConfigFlagsTable& flagTable;
    LPWSTR           pszCurrentArg;
    ICustomConfigFlags * pCustomConfigFlags;

// Methods
public:
    int                Parse(int argc, __in_ecount(argc) LPWSTR argv[]);
    int Parse(__in LPWSTR token) throw();
    CmdLineArgsParser(ICustomConfigFlags * pCustomConfigFlags = nullptr, Js::ConfigFlagsTable& flagTable = Js::Configuration::Global.flags);
    ~CmdLineArgsParser();

// Helper Classes
private:

    ///----------------------------------------------------------------------------
    ///
    /// class Exception
    ///
    ///----------------------------------------------------------------------------

    class Exception {
        LPCWSTR        pszMsg;
    public:
        Exception(LPCWSTR message):
            pszMsg(message)
        {}

        operator LPCWSTR () const
        {
            return this->pszMsg;
        }
    };

// Implementation
private:
            bool                       ParseBoolean();
            LPWSTR                     ParseString(__inout_ecount(ceBuffer) LPWSTR buffer, size_t ceBuffer = MaxTokenSize, bool fTreatColonAsSeperator = true);
            int                        ParseInteger();
            Js::SourceFunctionNode     ParseSourceFunctionIds();
            void                       ParsePhase(Js::Phases *pPhase);
            void                       ParseRange(Js::Range *range);
            void                       ParseNumberRange(Js::NumberRange *range);
            void                       ParseFlag();
            void                       ParseNumberSet(Js::NumberSet * numberSet);
            void                       ParseNumberPairSet(Js::NumberPairSet * numberPairSet);
            void                       PrintUsage();

            wchar_t CurChar()
            {
                return this->pszCurrentArg[0];
            }

            void NextChar()
            {
                this->pszCurrentArg++;
            }

            bool IsDigit()
            {
                return (CurChar() >='0' && CurChar() <= '9');
            }

            bool IsHexDigit()
            {
                return (CurChar() >= '0' && CurChar() <= '9') ||
                    (CurChar() >= 'A' && CurChar() <= 'F') ||
                    (CurChar() >= 'a' && CurChar() <= 'f');
            }

            // Implements ICmdLineArgsParser
            virtual BSTR GetCurrentString() override;
            virtual bool GetCurrentBoolean() override
            {
                return ParseBoolean();
            }
            virtual int GetCurrentInt() override
            {
                NextChar();
                return  ParseInteger();
            }
};
