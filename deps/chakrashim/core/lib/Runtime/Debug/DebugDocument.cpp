//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

namespace Js
{
    DebugDocument::DebugDocument(Utf8SourceInfo* utf8SourceInfo, Js::FunctionBody* functionBody) :
        utf8SourceInfo(utf8SourceInfo),
        m_breakpointList(nullptr)
    {
        Assert(utf8SourceInfo != nullptr);
        if (functionBody != nullptr)
        {
            this->functionBody.Root(functionBody, this->utf8SourceInfo->GetScriptContext()->GetRecycler());
        }
    }

    DebugDocument::~DebugDocument()
    {
        Assert(this->utf8SourceInfo == nullptr);
        Assert(this->m_breakpointList == nullptr);
    }

    void DebugDocument::CloseDocument()
    {
        if (this->m_breakpointList != nullptr)
        {
            this->ClearAllBreakPoints();
        }

        Assert(this->utf8SourceInfo != nullptr);

        if (functionBody)
        {
            functionBody.Unroot(this->utf8SourceInfo->GetScriptContext()->GetRecycler());
        }

        this->utf8SourceInfo = nullptr;
    }

    BreakpointProbeList* DebugDocument::GetBreakpointList()
    {
        if (m_breakpointList != nullptr)
        {
            return m_breakpointList;
        }

        ScriptContext * scriptContext = this->utf8SourceInfo->GetScriptContext();
        if (scriptContext == nullptr || scriptContext->IsClosed())
        {
            return nullptr;
        }

        ArenaAllocator* diagnosticArena = scriptContext->AllocatorForDiagnostics();
        AssertMem(diagnosticArena);

        m_breakpointList = this->NewBreakpointList(diagnosticArena);
        return m_breakpointList;
    }

    BreakpointProbeList* DebugDocument::NewBreakpointList(ArenaAllocator* arena)
    {
        return BreakpointProbeList::New(arena);
    }

    HRESULT DebugDocument::SetBreakPoint(long ibos, BREAKPOINT_STATE breakpointState)
    {
        ScriptContext* scriptContext = this->utf8SourceInfo->GetScriptContext();

        if (scriptContext == nullptr || scriptContext->IsClosed())
        {
            return E_UNEXPECTED;
        }

        HRESULT hr = NOERROR;

        switch (breakpointState)
        {
        default:
            AssertMsg(FALSE, "Bad breakpoint state");
            // fall-through
        case BREAKPOINT_DISABLED:
        case BREAKPOINT_DELETED:
        {
            BEGIN_TRANSLATE_OOM_TO_HRESULT
            {
                BreakpointProbeList* pBreakpointList = this->GetBreakpointList();
                if (pBreakpointList)
                {
                    ArenaAllocator arena(L"TemporaryBreakpointList", scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticPageAllocator(), Throw::OutOfMemory);
                    BreakpointProbeList* pDeleteList = this->NewBreakpointList(&arena);
                    StatementLocation statement;
                    if (!this->GetStatementLocation(ibos, &statement))
                    {
                        return E_FAIL;
                    }

                    pBreakpointList->Map([&statement, scriptContext, pDeleteList](int index, BreakpointProbe * breakpointProbe)
                    {
                        if (breakpointProbe->Matches(statement.function, statement.statement.begin))
                        {
                            scriptContext->GetDebugContext()->GetProbeContainer()->RemoveProbe(breakpointProbe);
                            pDeleteList->Add(breakpointProbe);
                        }
                    });

                    pDeleteList->Map([pBreakpointList](int index, BreakpointProbe * breakpointProbe)
                    {
                        pBreakpointList->Remove(breakpointProbe);
                    });
                    pDeleteList->Clear();
                }
            }
            END_TRANSLATE_OOM_TO_HRESULT(hr);

            break;
        }
        case BREAKPOINT_ENABLED:
        {
            StatementLocation statement;
            if (!this->GetStatementLocation(ibos, &statement))
            {
                return E_FAIL;
            }

            BEGIN_TRANSLATE_OOM_TO_HRESULT
            {
                BreakpointProbe* pProbe = Anew(scriptContext->AllocatorForDiagnostics(), BreakpointProbe, this, statement);
                scriptContext->GetDebugContext()->GetProbeContainer()->AddProbe(pProbe);
                BreakpointProbeList* pBreakpointList = this->GetBreakpointList();
                pBreakpointList->Add(pProbe);
            }
            END_TRANSLATE_OOM_TO_HRESULT(hr);

            break;
        }
        }
        return hr;
    }

    void DebugDocument::RemoveBreakpointProbe(BreakpointProbe *probe)
    {
        Assert(probe);
        if (m_breakpointList)
        {
            m_breakpointList->Remove(probe);
        }
    }

    void DebugDocument::ClearAllBreakPoints(void)
    {
        if (m_breakpointList != nullptr)
        {
            m_breakpointList->Clear();
            m_breakpointList = nullptr;
        }
    }

    BOOL DebugDocument::GetStatementSpan(long ibos, StatementSpan* pStatement)
    {
        StatementLocation statement;
        if (GetStatementLocation(ibos, &statement))
        {
            pStatement->ich = statement.statement.begin;
            pStatement->cch = statement.statement.end - statement.statement.begin;
            return TRUE;
        }
        return FALSE;
    }

    FunctionBody * DebugDocument::GetFunctionBodyAt(long ibos)
    {
        StatementLocation location = {};
        if (GetStatementLocation(ibos, &location))
        {
            return location.function;
        }

        return nullptr;
    }

    BOOL DebugDocument::HasLineBreak(long _start, long _end)
    {
        return this->functionBody->HasLineBreak(_start, _end);
    }

    BOOL DebugDocument::GetStatementLocation(long ibos, StatementLocation* plocation)
    {
        if (ibos < 0)
        {
            return FALSE;
        }

        ScriptContext* scriptContext = this->utf8SourceInfo->GetScriptContext();
        if (scriptContext == nullptr || scriptContext->IsClosed())
        {
            return FALSE;
        }

        ulong ubos = static_cast<ulong>(ibos);

        // Getting the appropriate statement on the asked position works on the heuristic which requires two
        // probable candidates. These candidates will be closest to the ibos where first.range.start < ibos and
        // second.range.start >= ibos. They will be fetched out by going into each FunctionBody.

        StatementLocation candidateMatch1 = {};
        StatementLocation candidateMatch2 = {};

        this->utf8SourceInfo->MapFunction([&](FunctionBody* pFuncBody)
        {
            ulong functionStart = pFuncBody->StartInDocument();
            ulong functionEnd = functionStart + pFuncBody->LengthInBytes();

            // For the first candidate, we should allow the current function to participate if its range
            // (instead of just start offset) is closer to the ubos compared to already found candidate1.

            if (candidateMatch1.function == nullptr ||
                ((candidateMatch1.statement.begin <= static_cast<int>(functionStart) ||
                candidateMatch1.statement.end <= static_cast<int>(functionEnd)) &&
                ubos > functionStart) ||
                candidateMatch2.function == nullptr ||
                (candidateMatch2.statement.begin > static_cast<int>(functionStart) &&
                ubos <= functionStart) ||
                (functionStart <= ubos &&
                ubos < functionEnd))
            {
                // We need to find out two possible candidate from the current FunctionBody.
                pFuncBody->FindClosestStatements(ibos, &candidateMatch1, &candidateMatch2);
            }
        });

        if (candidateMatch1.function == nullptr && candidateMatch2.function == nullptr)
        {
            return FALSE; // No Match found
        }

        if (candidateMatch1.function == nullptr || candidateMatch2.function == nullptr)
        {
            *plocation = (candidateMatch1.function == nullptr) ? candidateMatch2 : candidateMatch1;

            return TRUE;
        }

        // If one of the func is inner to another one, and ibos is in the inner one, disregard the outer one/let the inner one win.
        // See WinBlue 575634. Scenario is like this: var foo = function () {this;} -- and BP is set to 'this;' 'function'.
        if (candidateMatch1.function != candidateMatch2.function)
        {
            Assert(candidateMatch1.function && candidateMatch2.function);

            regex::Interval func1Range(candidateMatch1.function->StartInDocument());
            func1Range.End(func1Range.Begin() + candidateMatch1.function->LengthInBytes());
            regex::Interval func2Range(candidateMatch2.function->StartInDocument());
            func2Range.End(func2Range.Begin() + candidateMatch2.function->LengthInBytes());

            if (func1Range.Includes(func2Range) && func2Range.Includes(ibos))
            {
                *plocation = candidateMatch2;
                return TRUE;
            }
            else if (func2Range.Includes(func1Range) && func1Range.Includes(ibos))
            {
                *plocation = candidateMatch1;
                return TRUE;
            }
        }

        // At this point we have both candidate to consider.

        Assert(candidateMatch1.statement.begin < candidateMatch2.statement.begin);
        Assert(candidateMatch1.statement.begin < ibos);
        Assert(candidateMatch2.statement.begin >= ibos);

        // Default selection
        *plocation = candidateMatch1;

        // If the second candidate start at ibos or
        // if the first candidate has line break between ibos and the second candidate is on the same line as ibos
        // then consider the second one.

        BOOL fNextHasLineBreak = this->HasLineBreak(ibos, candidateMatch2.statement.begin);

        if ((candidateMatch2.statement.begin == ibos)
            || (this->HasLineBreak(candidateMatch1.statement.begin, ibos) && !fNextHasLineBreak))
        {
            *plocation = candidateMatch2;
        }
        // If ibos is out of the range of first candidate, choose second candidate if  ibos is on the same line as second candidate
        // or ibos is not on the same line of the end of the first candidate.
        else if (candidateMatch1.statement.end < ibos && (!fNextHasLineBreak || this->HasLineBreak(candidateMatch1.statement.end, ibos)))
        {
            *plocation = candidateMatch2;
        }

        return TRUE;
    }
}
