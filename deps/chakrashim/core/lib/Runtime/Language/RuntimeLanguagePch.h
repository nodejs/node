//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "Parser.h"

#include "Runtime.h"

#include "Language\AsmJsTypes.h"
#include "Language\AsmJsUtils.h"
#include "Language\AsmJsLink.h"
#include "Language\AsmJsModule.h"
#include "Language\AsmJs.h"
#ifdef ASMJS_PLAT
#include "Language\AsmJSJitTemplate.h"
#include "Language\AsmJSEncoder.h"
#include "Language\AsmJSCodeGenerator.h"
#endif
#include "Language\FunctionCodeGenJitTimeData.h"

#include "Language\ProfilingHelpers.h"
#include "Language\CacheOperators.h"

#include "Language\JavascriptMathOperators.h"
#include "Language\JavascriptStackWalker.h"
#ifdef DYNAMIC_PROFILE_STORAGE
#include "Language\DynamicProfileStorage.h"
#endif
#include "Language\SourceDynamicProfileManager.h"

#include "Base\EtwTrace.h"

#include "Library\ArgumentsObject.h"

#include "Types\TypePropertyCache.h"
#include "Library\JavascriptVariantDate.h"
#include "Library\JavascriptProxy.h"
#include "Library\JavascriptSymbol.h"
#include "Library\JavascriptSymbolObject.h"
#include "Library\JavascriptGenerator.h"
#include "Library\StackScriptFunction.h"
#include "Library\HostObjectBase.h"

#ifdef ENABLE_MUTATION_BREAKPOINT
// REVIEW: ChakraCore Dependency
#include "activdbg_private.h"
#include "Debug\MutationBreakpoint.h"
#endif

// SIMD_JS
// SIMD types
#include "Library\JavascriptSIMDFloat32x4.h"
#include "Library\JavascriptSIMDFloat64x2.h"
#include "Library\JavascriptSIMDInt32x4.h"
#include "Library\JavascriptSIMDInt8x16.h"
// SIMD operations
#include "Language\SIMDFloat32x4Operation.h"
#include "Language\SIMDFloat64x2Operation.h"
#include "Language\SIMDInt32x4Operation.h"
#include "Language\SIMDInt8x16Operation.h"
#include "Language\SIMDUtils.h"
// SIMD libs
#include "Library\SIMDFloat32x4Lib.h"
#include "Library\SIMDFloat64x2Lib.h"
#include "Library\SIMDInt32x4Lib.h"
#include "Library\SIMDInt8x16Lib.h"


#include "Debug\DebuggingFlags.h"
#include "Debug\DiagProbe.h"
#include "Debug\DebugManager.h"
#include "Debug\ProbeContainer.h"
#include "Debug\DebugContext.h"

#ifdef ENABLE_BASIC_TELEMETRY
#include "ScriptContextTelemetry.h"
#endif

// .inl files
#include "Language\CacheOperators.inl"
#include "Language\JavascriptMathOperators.inl"
