//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// (hintCode, isNotOptimized, hintlevel, description, consequences, suggestion)

PERFHINT_REASON(HasTryBlock,               true, PerfHintLevels::L1,      L"Function has try block",                 L"Un-optimized JIT code generated for this function", L"Move perf sensitive block inside of try to different function")
PERFHINT_REASON(HasTryBlock_Verbose,       true, PerfHintLevels::VERBOSE, L"Function has try block",                 L"Un-optimized JIT code generated for this function", L"Move perf sensitive block inside of try to different function")
PERFHINT_REASON(CallsEval,                 true, PerfHintLevels::L1,      L"Function calls eval statement",          L"Extra scopes, affect inlining, high overhead in the JIT code", L"Check usage of eval statement")
PERFHINT_REASON(CallsEval_Verbose,         true, PerfHintLevels::VERBOSE, L"Function calls eval statement",          L"Extra scopes, affect inlining, high overhead in the JIT code", L"Check usage of eval statement")
PERFHINT_REASON(ChildCallsEval,            true, PerfHintLevels::VERBOSE, L"Function's child calls eval statement",  L"Extra scopes, affect inlining, high overhead in the JIT code", L"Check usage of eval statement")
PERFHINT_REASON(HasWithBlock,              true, PerfHintLevels::L1,      L"Function has with statement",            L"Slower lookups, high overhead in the JIT code", L"Avoid using with statement")
PERFHINT_REASON(HeapArgumentsDueToFormals, true, PerfHintLevels::L1,      L"Arguments object not optimized due to formals", L"Slower lookups, affects inlining, high overhead in the JIT code", L"Check the usage of formals in the function")
PERFHINT_REASON(HeapArgumentsModification, true, PerfHintLevels::L1,      L"Modification to arguments",              L"Slower lookups, high overhead in the JIT code", L"Avoid modification to the arguments")
PERFHINT_REASON(HeapArgumentsCreated,      true, PerfHintLevels::L1,      L"Arguments object not optimized",         L"Slower lookups, high overhead in the JIT code", L"Check the usage of arguments in the function")
PERFHINT_REASON(PolymorphicInilineCap,     true, PerfHintLevels::L1,      L"Function has reached polymorphic-inline cap", L"This function will not inline more than 4 functions for this call-site.", L"Check the polymorphic usage of this function")


