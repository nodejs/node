//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"
#include "core\ICustomConfigFlags.h"
#include "core\CmdParser.h"

using namespace Js;


///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::ParseString
///
///     Parses an string token. There are 2 ways to specify it.
///         1. Quoted   - " " Any character within quotes is parsed as string.
///                       if the quotes are not closed, its an error.
///         2. UnQuoted - End of string is indicated by a space/end of stream.
///                       If fTreatColonAsSeperator is mentioned, then we break
///                       at colon also.
///
///
///     Empty string "" is treated as Exception()
///
///----------------------------------------------------------------------------

LPWSTR
CmdLineArgsParser::ParseString(__inout_ecount(ceBuffer) LPWSTR buffer, size_t ceBuffer, bool fTreatColonAsSeperator)
{

    wchar_t *out = buffer;
    size_t len = 0;

    if('"' == CurChar())
    {
        NextChar();

        while('"' != CurChar())
        {
            if(0 == CurChar())
            {
                throw Exception(L"Unmatched quote");
            }

            //
            // MaxTokenSize - 1 because we need 1 extra position for null termination
            //
            if (len >= ceBuffer - 1)
            {
                throw Exception(L"String token too large to parse");
            }

            out[len++] = CurChar();

            NextChar();
        }
        NextChar();
    }
    else
    {
        bool fDone = false;

        while(!fDone)
        {
            switch(CurChar())
            {
            case ' ':
            case ',':
            case 0:
                fDone = 1;
                break;
            case '-':
            case ':':
                if(fTreatColonAsSeperator)
                {
                    fDone = true;
                    break;
                }
                else
                {
                    // Fallthrough
                }
            default:
                if(len >= MaxTokenSize -1)
                {
                    throw Exception(L"String token too large to parse");
                }
                out[len++] = CurChar();
                NextChar();
            }
        }
    }

    if(0 == len)
    {
        throw Exception(L"String Token Expected");
    }

    out[len] = '\0';

    return buffer;
}

///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::ParseSourceFunctionIds
///
/// Parses for sourceContextId and FunctionId pairs
///----------------------------------------------------------------------------

Js::SourceFunctionNode
CmdLineArgsParser::ParseSourceFunctionIds()
{
    uint functionId, sourceId;

    if ('*' == CurChar())
    {
        sourceId = 1;
        functionId = (uint)-2;
        NextChar();
    }
    else if ('+' == CurChar())
    {
        sourceId = 1;
        functionId = (uint)-1;
        NextChar();
    }
    else
    {
        functionId = sourceId = ParseInteger();

        if ('.' == CurChar())
        {
            NextChar();
            if ('*' == CurChar())
            {
                functionId = (uint)-2;
                NextChar();
            }
            else if ('+' == CurChar())
            {
                functionId = (uint)-1;
                NextChar();
            }
            else
            {
                functionId = ParseInteger();
            }
        }
        else
        {
            sourceId = 1;
        }
    }
    return SourceFunctionNode(sourceId, functionId);
}

///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::ParseInteger
///
/// Parses signed integer. Checks for overflow and underflows.
///----------------------------------------------------------------------------

int
CmdLineArgsParser::ParseInteger()
{
    int result  = 0;
    int sign    = 1;

    if('-' == CurChar())
    {
        sign = -1;
        NextChar();
    }
    if(!IsDigit())
    {
        throw Exception(L"Integer Expected");
    }

    int base = 10;

    if ('0' == CurChar())
    {
        NextChar();
        if (CurChar() == 'x')
        {
            NextChar();
            base = 16;
        }
        // Should the else case be parse as octal?
    }

    while(IsDigit() || (base == 16 && IsHexDigit()))
    {
        int currentDigit = (int)(CurChar() - '0');
        if (currentDigit > 9)
        {
            Assert(base == 16);
            if (CurChar() < 'F')
            {
                currentDigit = 10 + (int)(CurChar() - 'A');
            }
            else
            {
                currentDigit = 10 + (int)(CurChar() - 'a');
            }

            Assert(currentDigit < 16);
        }

        result = result * base + (int)(CurChar() - '0');
        if(result < 0)
        {
            // overflow or underflow in case sign = -1
            throw Exception(L"Integer too large to parse");
        }

        NextChar();
    }

    return result * sign;
}


///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::ParseRange
///
/// Parses :-
/// range = int | int '-' int | range, range
///
///----------------------------------------------------------------------------

void
CmdLineArgsParser::ParseRange(Js::Range *pRange)
{
    SourceFunctionNode r1 = ParseSourceFunctionIds();
    SourceFunctionNode r2;
    switch(CurChar())
    {
    case '-':
        NextChar();
        r2 = ParseSourceFunctionIds();

        if (r1.sourceContextId > r2.sourceContextId)
        {
            throw Exception(L"Left source index must be smaller than the Right source Index");
        }
        if ((r1.sourceContextId == r2.sourceContextId) &&
            (r1.functionId > r2.functionId))
        {
            throw Exception(L"Left functionId must be smaller than the Right functionId when Source file is the same");
        }

        pRange->Add(r1, r2);
        switch(CurChar())
        {
        case ',':
            NextChar();
            ParseRange(pRange);
            break;

        case ' ':
        case 0:
            break;

        default:
            throw Exception(L"Unexpected character while parsing Range");
        }
        break;

    case ',':
        pRange->Add(r1);
        NextChar();
        ParseRange(pRange);
        break;

    case ' ':
    case 0:
        pRange->Add(r1);
        break;

    default:
        throw Exception(L"Unexpected character while parsing Range");
    }

}


void
CmdLineArgsParser::ParseNumberRange(Js::NumberRange *pRange)
{
    int start = ParseInteger();
    int end;

    switch (CurChar())
    {
    case '-':
        NextChar();
        end = ParseInteger();

        if (start > end)
        {
            throw Exception(L"Range start must be less than range end");
        }

        pRange->Add(start, end);
        switch (CurChar())
        {
        case ',':
            NextChar();
            ParseNumberRange(pRange);
            break;

        case ' ':
        case 0:
            break;

        default:
            throw Exception(L"Unexpected character while parsing Range");
        }
        break;

    case ',':
        pRange->Add(start);
        NextChar();
        ParseNumberRange(pRange);
        break;

    case ' ':
    case 0:
        pRange->Add(start);
        break;

    default:
        throw Exception(L"Unexpected character while parsing Range");
    }

}
///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::ParsePhase
///
/// Parses comma separated list of:
///     phase[:range]
/// phase is a string defined in Js:PhaseNames.
///
///----------------------------------------------------------------------------

void
CmdLineArgsParser::ParsePhase(Js::Phases *pPhaseList)
{
    wchar_t buffer[MaxTokenSize];
    ZeroMemory(buffer, sizeof(buffer));

    Phase phase = ConfigFlagsTable::GetPhase(ParseString(buffer));
    if(InvalidPhase == phase)
    {
        throw Exception(L"Invalid phase :");
    }

    pPhaseList->Enable(phase);
    switch(CurChar())
    {
    case ':':
        NextChar();
        ParseRange(pPhaseList->GetRange(phase));
        break;
    case ',':
        NextChar();
        ParsePhase(pPhaseList);
        break;
    default:
        break;
    }
}

void
CmdLineArgsParser::ParseNumberSet(Js::NumberSet * numberPairSet)
{
    while (true)
    {
        int x = ParseInteger();
        numberPairSet->Add(x);

        if (CurChar() != ';')
        {
            break;
        }
        NextChar();
    }
}

void
CmdLineArgsParser::ParseNumberPairSet(Js::NumberPairSet * numberPairSet)
{
    while (true)
    {
        int line = ParseInteger();
        int col = -1;
        if (CurChar() == ',')
        {
            NextChar();
            col = ParseInteger();
        }

        numberPairSet->Add(line, col);

        if (CurChar() != ';')
        {
            break;
        }
        NextChar();
    }
}

bool
CmdLineArgsParser::ParseBoolean()
{
    if (CurChar() == ':')
    {
        throw Exception(L"':' not expected with a boolean flag");
    }
    else if (CurChar() != '-' && CurChar() != ' ' && CurChar() != 0)
    {
        throw Exception(L"Invalid character after boolean flag");
    }
    else
    {
        return (CurChar() != '-');
    }
}

BSTR
CmdLineArgsParser::GetCurrentString()
{
    wchar_t buffer[MaxTokenSize];
    ZeroMemory(buffer, sizeof(buffer));

    switch (CurChar())
    {
    case ':':
        NextChar();
        return SysAllocString(ParseString(buffer, MaxTokenSize, false));
    case ' ':
    case 0:
        NextChar();
        return nullptr;
    default:
        throw Exception(L"Expected ':'");
    }
}

///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::ParseFlag
///
/// Parses:
///      flag[:parameter]
/// Flag is a string defined in Js:FlagNames.
/// The type of expected parameter depends upon the flag. It can be
///     1. String
///     2. Number
///     3. Boolean
///     4. Phase
///
/// In case of boolean the presence no parameter is expected. the value of the
/// boolean flag is set to 'true'
///
///----------------------------------------------------------------------------

void
CmdLineArgsParser::ParseFlag()
{
    wchar_t buffer[MaxTokenSize];
    ZeroMemory(buffer, sizeof(buffer));

    LPWSTR flagString = ParseString(buffer);
    Flag flag = ConfigFlagsTable::GetFlag(flagString);
    if(InvalidFlag == flag)
    {
        if (pCustomConfigFlags != nullptr)
        {
            if (pCustomConfigFlags->ParseFlag(flagString, this))
            {
                return;
            }
        }
        throw Exception(L"Invalid Flag");
    }


    FlagTypes flagType = ConfigFlagsTable::GetFlagType(flag);
    AssertMsg(InvalidFlagType != flagType, "Invalid flag type");

    this->flagTable.Enable(flag);

    if(FlagBoolean == flagType)
    {
        Boolean boolValue = ParseBoolean();

        this->flagTable.SetAsBoolean(flag, boolValue);
    }
    else
    {
        switch(CurChar())
        {
        case ':':
            NextChar();
            switch(flagType)
            {
            case FlagPhases:
                ParsePhase(this->flagTable.GetAsPhase(flag));
                break;

            case FlagString:
                *this->flagTable.GetAsString(flag) = ParseString(buffer, MaxTokenSize, false);
                break;

            case FlagNumber:
                *this->flagTable.GetAsNumber(flag) = ParseInteger();
                break;

            case FlagNumberSet:
                ParseNumberSet(this->flagTable.GetAsNumberSet(flag));
                break;

            case FlagNumberPairSet:
                ParseNumberPairSet(this->flagTable.GetAsNumberPairSet(flag));
                break;

            case FlagNumberRange:
                ParseNumberRange(this->flagTable.GetAsNumberRange(flag));
                break;

            default:
                AssertMsg(0, "Flag not Handled");
            }
            break;
        case ' ':
        case 0:
            break;
        default:
            throw Exception(L"Expected ':'");
        }
    }
}

///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::Parse
///
/// The main loop which parses 1 flag at a time
///
///----------------------------------------------------------------------------

int
CmdLineArgsParser::Parse(int argc, __in_ecount(argc) LPWSTR argv[])
{
    int err = 0;

    for(int i = 1; i < argc; i++)
    {
        if ((err = Parse(argv[i])) != 0)
        {
            break;
        }
    }
    return err;
}

int CmdLineArgsParser::Parse(__in LPWSTR oneArg) throw()
{
    int err = 0;
    wchar_t buffer[MaxTokenSize];
    ZeroMemory(buffer, sizeof(buffer));

    this->pszCurrentArg = oneArg;
    AssertMsg(NULL != this->pszCurrentArg, "How can command line give NULL argv's");
    try
    {
        switch(CurChar())
        {
        case '-' :
        case '/':
            NextChar();
            if('?' == CurChar())
            {
                PrintUsage();
                return -1;
            }
            else
            {
                ParseFlag();
            }
            break;
        default:
            if(NULL != this->flagTable.Filename)
            {
                throw Exception(L"Duplicate filename entry");
            }

            this->flagTable.Filename = ParseString(buffer, MaxTokenSize, false);
            break;
        }
    }
    catch(Exception &exp)
    {
        wprintf(L"%s : %s\n", (LPCWSTR)exp, oneArg);
        err = -1;
    }
    return err;
}


///----------------------------------------------------------------------------
///
/// CmdLineArgsParser::CmdLineArgsParser
///
/// Constructor
///
///----------------------------------------------------------------------------

CmdLineArgsParser::CmdLineArgsParser(ICustomConfigFlags * pCustomConfigFlags, Js::ConfigFlagsTable& flagTable) :
    flagTable(flagTable), pCustomConfigFlags(pCustomConfigFlags)
{
    this->pszCurrentArg = NULL;
}

CmdLineArgsParser::~CmdLineArgsParser()
{
    flagTable.FinalizeConfiguration();
}

void CmdLineArgsParser::PrintUsage()
{
    if (pCustomConfigFlags)
    {
        pCustomConfigFlags->PrintUsage();
        return;
    }
    Js::ConfigFlagsTable::PrintUsageString();
}
