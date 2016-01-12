//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

//
// Flags for Fast F12 which are used at run time/by jitted code for conditional bailouts, etc.
//
// Summary on how these are used.
// Place               Scenario           Flag                                      BailOutKind                  Comment
// ---------------------------------------------------------------------------------------------------------------------------------------
// Begin Function      Async Break        DebuggingFlags::m_forceInterpreter        BailOutForceByFlag          'Async Break' is when the user hits Pause button.
//                     Step In            stepController::StepType                  BailOutStep
//                     F has BP           FunctionBody::SourceInfo::m_probeCount    BailOutBreakPointInFunction
//
// Return From F       Step any/out of F  stepController::frameAddrWhenSet > ebp    BailOutStackFrameBase
//                     F has BP           FunctionBody::m_hasBreakPoint             BailOutBreakPointInFunction  When we return to jitted F that has BP,
//                                                                                                               we need to bail out.
//                     Local val changed  Inplace stack addr check                  BailOutLocalValueChanged     Check 1 byte on stack specified by
//                                                                                                               Func::GetHasLocalVarChangedOffset().
// Return from helper  Continue after ex  DebuggingFlags::ContinueAfterException    BailOutIgnoreException       We wrap the call in jitted code with try-catch wrapper.
//        or lib Func  Continue after ex  DebuggingFlags::ContinueAfterException    BailOutIgnoreException       We wrap the call in jitted code with try-catch wrapper.
//                     Async Break        DebuggingFlags::m_forceInterpreter                                     Async Break is important to Hybrid Debugging.
//
// Loop back edge      Async Break        DebuggingFlags::m_forceInterpreter        BailOutForceByFlag          'Async Break' is when the user hits Pause button.
//                     F gets new BP      FunctionBody::SourceInfo::m_probeCount    BailOutBreakPointInFunction  For scenario when BP is defined inside loop while loop is running.
//
// 'debugger' stmt     'debugger' stmt    None (inplace explicit bailout)           BailOutExplicit              Insert explicit unconditional b/o.
//
// How it all works:
// - F12 Debugger controls the flags (set/clear)
// - JIT:
//   - When inserting a bailout, we use appropriate set of BailoutKind's (see BailoutKind.h).
//   - Then when lowering we do multiple condition checks (how many BailoutKind's are in the b/o instr)
//     and one bailout if any of conditions triggers.
// - Runtime: bailout happens, we break into debug interpreter thunk and F12 Debugger catches up,
//   now we can debug the frame that was originally jitted.
//

class DebuggingFlags
{
private:
    bool m_forceInterpreter;  // If true/non-zero, break into debug interpreter thunk (we check only in places where this flag is applicable).
    bool m_isIgnoringException; // If true/non-zero, we are processing ignore exception scenario. Redundant, as m_byteCodeOffsetAfterIgnoreException
    // would be != -1 but for lower it's faster to check both flags at once, that's the reason to have this flag.
    int m_byteCodeOffsetAfterIgnoreException;
    uint m_funcNumberAfterIgnoreException;  // Comes from FunctionBody::GetFunctionNumber(), 0 is default/invalid.

    // Whether try-catch wrapper for built-ins for "continue after exception scenarios" is present on current thread (below in call stack).
    // If one is registered, we don't wrap with try-catch all subsequent calls.
    // All built-ins have entryPoint = ProfileEntryThunk which does the try-catch.
    // The idea is that one built-in can call another, etc, but we want to try-catch on 1st built-in called from jitted code,
    // otherwise if we don't throw above, some other built-ins in the chain may continue doing something after exception in bad state.
    // What we want is that top-most built-in throws, but bottom-most right above jitted code catches the ex.
    bool m_isBuiltInWrapperPresent;

public:
    static const int InvalidByteCodeOffset = -1;
    static const uint InvalidFuncNumber = 0;

    DebuggingFlags();

    bool GetForceInterpreter() const;
    void SetForceInterpreter(bool value);
    static size_t GetForceInterpreterOffset();

    int GetByteCodeOffsetAfterIgnoreException() const;
    uint GetFuncNumberAfterIgnoreException() const;
    void SetByteCodeOffsetAfterIgnoreException(int offset);
    void SetByteCodeOffsetAndFuncAfterIgnoreException(int offset, uint functionNumber);
    void ResetByteCodeOffsetAndFuncAfterIgnoreException();
    size_t GetByteCodeOffsetAfterIgnoreExceptionOffset() const;

    bool IsBuiltInWrapperPresent() const;
    void SetIsBuiltInWrapperPresent(bool value = true);
};
