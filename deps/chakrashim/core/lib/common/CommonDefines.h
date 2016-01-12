//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "TargetVer.h"
#include "Warnings.h"

//----------------------------------------------------------------------------------------------------
// Chakra Core version
//----------------------------------------------------------------------------------------------------
#define CHAKRA_CORE_MAJOR_VERSION 1
#define CHAKRA_CORE_MINOR_VERSION 0
#define CHAKRA_CORE_VERSION_RELEASE 0
#define CHAKRA_CORE_VERSION_PRERELEASE 1
#define CHAKRA_CORE_VERSION_RELEASE_QFE 0

#define CHAKRA_VERSION_RELEASE 0
#define CHAKRA_VERSION_PRERELEASE 1

// NOTE: need to update the GUID in ByteCodeCacheReleaseFileVersion.h as well

//----------------------------------------------------------------------------------------------------
// Default debug/fretest/release flags values
//  - Set the default values of debug/fretest/release flags if it is not set by the command line
//----------------------------------------------------------------------------------------------------
#ifndef DBG_DUMP
#define DBG_DUMP 0
#endif

#ifdef _DEBUG
#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG 1
#endif

// if test hook is enabled, debug config options are enabled too
#ifdef ENABLE_TEST_HOOKS
#ifndef ENABLE_DEBUG_CONFIG_OPTIONS
#define ENABLE_DEBUG_CONFIG_OPTIONS 1
#endif
#endif

// ENABLE_DEBUG_CONFIG_OPTIONS is enabled in debug build when DBG or DBG_DUMP is defined
// It is enabled in fretest build (jscript9test.dll and jc.exe) in the build script
#if DBG || DBG_DUMP
    #ifndef ENABLE_DEBUG_CONFIG_OPTIONS
        #define ENABLE_DEBUG_CONFIG_OPTIONS     1
    #endif

    // Flag to control availability of other flags to control regex debugging, tracing, profiling, etc. This is separate from
    // ENABLE_DEBUG_CONFIG_OPTIONS because enabling this flag may affect performance significantly, even with default values for
    // the regex flags this flag would make available.
    #ifndef ENABLE_REGEX_CONFIG_OPTIONS
        #define ENABLE_REGEX_CONFIG_OPTIONS     1
    #endif
#endif

// TODO: consider removing before RTM: keep for CHK/FRETEST but remove from FRE.
// This will cause terminate process on AV/Assert rather that letting PDM (F12/debugger scenarios) eat exceptions.
// At least for now, enable this even in FRE builds. See ReportError.h.
#define ENABLE_DEBUG_API_WRAPPER 1

//----------------------------------------------------------------------------------------------------
//  Define Architectures' aliases for Simplicity
//----------------------------------------------------------------------------------------------------
#if defined(_M_ARM) || defined(_M_ARM64)
#define _M_ARM32_OR_ARM64 1
#endif

#if defined(_M_IX86) || defined(_M_ARM)
#define _M_IX86_OR_ARM32 1
#define TARGET_32 1
#endif

#if defined(_M_X64) || defined(_M_ARM64)
#define _M_X64_OR_ARM64 1
#define TARGET_64 1
#endif

//----------------------------------------------------------------------------------------------------
// Enabled features
//----------------------------------------------------------------------------------------------------

// NOTE: Disabling these might not work and are not fully supported and maintained
// Even if it builds, it may not work properly. Disable at your own risk

// ByteCode
#define VARIABLE_INT_ENCODING 1                     // Byte code serialization variable size int field encoding
#define BYTECODE_BRANCH_ISLAND                      // Byte code short branch and branch island

// Language features
#define ENABLE_INTL_OBJECT                          // Intl support
#define ENABLE_ES6_CHAR_CLASSIFIER                  // ES6 Unicode character classifier support

// Type system features
#define PERSISTENT_INLINE_CACHES                    // *** TODO: Won't build if disabled currently
#define SUPPORT_FIXED_FIELDS_ON_PATH_TYPES          // *** TODO: Won't build if disabled currently

// GC features
#define CONCURRENT_GC_ENABLED 1                     // *** TODO: Won't build if disabled currently
#define PARTIAL_GC_ENABLED 1                        // *** TODO: Won't build if disabled currently
#define BUCKETIZE_MEDIUM_ALLOCATIONS 1              // *** TODO: Won't build if disabled currently
#define SMALLBLOCK_MEDIUM_ALLOC 1                   // *** TODO: Won't build if disabled currently
#define LARGEHEAPBLOCK_ENCODING 1                   // Large heap block metadata encoding
#define RECYCLER_WRITE_BARRIER                      // Write Barrier support
#define IDLE_DECOMMIT_ENABLED 1                     // Idle Decommit
#define RECYCLER_PAGE_HEAP                          // PageHeap support

// JIT features

#if DISABLE_JIT
#define ENABLE_NATIVE_CODEGEN 0
#define ENABLE_PROFILE_INFO 0
#define ENABLE_BACKGROUND_PARSING 0                 // Disable background parsing in this mode
                                                    // We need to decouple the Jobs infrastructure out of 
                                                    // Backend to make background parsing work with JIT disabled
#define DYNAMIC_INTERPRETER_THUNK 0
#define DISABLE_DYNAMIC_PROFILE_DEFER_PARSE
#define ENABLE_COPYONACCESS_ARRAY 0

// Used to temporarily disable ASMjs related code to get nonative compiling
#define TEMP_DISABLE_ASMJS
#else
// By default, enable the JIT
#define ENABLE_NATIVE_CODEGEN 1
#define ENABLE_PROFILE_INFO 1
#define ENABLE_BACKGROUND_PARSING 1
#define ENABLE_COPYONACCESS_ARRAY 1
#ifndef DYNAMIC_INTERPRETER_THUNK
#if defined(_M_IX86_OR_ARM32) || defined(_M_X64_OR_ARM64)
#define DYNAMIC_INTERPRETER_THUNK 1
#else
#define DYNAMIC_INTERPRETER_THUNK 0
#endif
#endif
#endif

// Other features
// #define CHAKRA_CORE_DOWN_COMPAT 1

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS) || defined(CHAKRA_CORE_DOWN_COMPAT)
#define DELAYLOAD_SET_CFG_TARGET 1
#endif

#ifdef NTBUILD
#define ENABLE_PROJECTION
#define ENABLE_FOUNDATION_OBJECT
#define ENABLE_EXPERIMENTAL_FLAGS
#define ENABLE_WININET_PROFILE_DATA_CACHE
#define ENABLE_BASIC_TELEMETRY
#define ENABLE_DOM_FAST_PATH
#define ENABLE_JS_ETW                               // ETW support
#define EDIT_AND_CONTINUE
#endif

// Telemetry flags
#ifdef ENABLE_BASIC_TELEMETRY
#define ENABLE_DIRECTCALL_TELEMETRY
#endif

// Telemetry features (non-DEBUG related)
#ifdef ENABLE_BASIC_TELEMETRY

    // These defines can be "overridden" in other headers (e.g. ESBuiltInsTelemetryProvider.h) in case a specific telemetry provider wants to change an option for performance.
    #define TELEMETRY_OPCODE_OFFSET_ENABLED true              // If the BytecodeOffset and FunctionId are logged.
    #define TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId) true // Any filter to apply on a per propertyId basis in the opcode handler for GetProperty/TypeofProperty/GetMethodProperty/etc.
    #define TELEMETRY_OPCODE_GET_PROPERTY_VALUES true         // If no telemetry providers need the values of properties then this option skips getting the value in the TypeofProperty opcode handler.

//    #define TELEMETRY_PROFILED    // If telemetry should capture "Profiled*" operations
//    #define TELEMETRY_CACHEHIT    // If telemetry should capture data that was gotten with a Cache Hit
//    #define TELEMETRY_JSO         // If telemetry should capture JavascriptOperators (expensive, as it happens during JITed code too, not just interpreted mode)
    #define TELEMETRY_AddToCache    // If telemetry should capture property-gets only when the propertyId is added to the cache (generally this means only the first usage of any feature is logged)
//    #define TELEMETRY_INTERPRETER // If telemetry should capture more interpreter events compared to just TELEMETRY_AddToCache


    #define TELEMETRY_TRACELOGGING   // Telemetry output using TraceLogging
//    #define TELEMETRY_OUTPUTPRINT    // Telemetry output using Output::Print

    // Enable/disable specific telemetry providers:
    #define TELEMETRY_ESB  // Telemetry of ECMAScript Built-Ins usage or detection.
//    #define TELEMETRY_ARRAY_USAGE // Telemetry of Array usage statistics
    #define TELEMETRY_DateParse // Telemetry of `Date.parse`

    #ifdef TELEMETRY_ESB
        // Because ESB telemetry is in-production and has major performance implications, this redefines some of the #defines above to disable non-critical functionality to get more performance.
        #undef TELEMETRY_OPCODE_OFFSET_ENABLED // Disable the FunctionId+Offset tracker.
        #define TELEMETRY_OPCODE_OFFSET_ENABLED false
        #undef TELEMETRY_PROPERTY_OPCODE_FILTER // Redefine the Property Opcode filter to ignore non-built-in properties.
        #define TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId) (propertyId < Js::PropertyIds::_countJSOnlyProperty)
        #undef TELEMETRY_OPCODE_GET_PROPERTY_VALUES
        #define TELEMETRY_OPCODE_GET_PROPERTY_VALUES false

        //#define TELEMETRY_ESB_STRINGS    // Telemetry that uses strings (slow), used for constructor detection for ECMAScript Built-Ins polyfills and Constructor-properties of Chakra-built-ins.
        //#define TELEMETRY_ESB_GetConstructorPropertyPolyfillDetection // Whether telemetry will inspect the `.constructor` property of every Object instance to determine if it's a polyfill of a known ES built-in.
    #endif

#else

    #define TELEMETRY_OPCODE_OFFSET_ENABLED false
    #define TELEMETRY_OPCODE_FILTER(propertyId) false

#endif

#if ENABLE_DEBUG_CONFIG_OPTIONS
#define ENABLE_DIRECTCALL_TELEMETRY_STATS
#endif

//----------------------------------------------------------------------------------------------------
// Debug and fretest features
//----------------------------------------------------------------------------------------------------
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS

#define BAILOUT_INJECTION
#if ENABLE_PROFILE_INFO
#define DYNAMIC_PROFILE_STORAGE
#define DYNAMIC_PROFILE_MUTATOR
#endif
#define RUNTIME_DATA_COLLECTION
#define SECURITY_TESTING
#define PROFILE_EXEC

#define BGJIT_STATS
#define REJIT_STATS
#define PERF_HINT
#define POLY_INLINE_CACHE_SIZE_STATS

#define JS_PROFILE_DATA_INTERFACE 1
#define EXCEPTION_RECOVERY 1
#define RECYCLER_TEST_SUPPORT
#define ARENA_ALLOCATOR_FREE_LIST_SIZE

// TODO (t-doilij) combine IR_VIEWER and ENABLE_IR_VIEWER
#ifdef _M_IX86
#if ENABLE_NATIVE_CODEGEN
#define IR_VIEWER
#define ENABLE_IR_VIEWER
#define ENABLE_IR_VIEWER_DBG_DUMP  // TODO (t-doilij) disable this before check-in
#endif
#endif

#ifdef ENABLE_JS_ETW
#define TEST_ETW_EVENTS

// VTUNE profiling requires ETW trace
#if defined(_M_IX86) || defined(_M_X64)
#define VTUNE_PROFILING
#endif
#endif

#ifdef NTBUILD
#define PERF_COUNTERS
#define ENABLE_MUTATION_BREAKPOINT
#endif

#ifdef _CONTROL_FLOW_GUARD
#define CONTROL_FLOW_GUARD_LOGGER
#endif

#ifndef ENABLE_TEST_HOOKS
#define ENABLE_TEST_HOOKS
#endif

#endif // ENABLE_DEBUG_CONFIG_OPTIONS


//----------------------------------------------------------------------------------------------------
// Debug only features
//----------------------------------------------------------------------------------------------------
#ifdef DEBUG
#define BYTECODE_TESTING
#define FAULT_INJECTION
#define RECYCLER_NO_PAGE_REUSE
#ifdef NTBUILD
#define INTERNAL_MEM_PROTECT_HEAP_ALLOC
#define INTERNAL_MEM_PROTECT_HEAP_CMDLINE
#endif
#endif

#ifdef DBG
#define VALIDATE_ARRAY
#define GENERATE_DUMP
#endif

#if DBG_DUMP
#undef DBG_EXTRAFIELD   // make sure we don't extra fields in free build.

#define TRACK_DISPATCH
#define BGJIT_STATS
#define REJIT_STATS
#define POLY_INLINE_CACHE_SIZE_STATS
#define INLINE_CACHE_STATS
#define FIELD_ACCESS_STATS
#define MISSING_PROPERTY_STATS
#define EXCEPTION_RECOVERY 1
#define EXCEPTION_CHECK                     // Check exception handling.
#define PROFILE_EXEC
#define PROFILE_MEM
#define PROFILE_TYPES
#define PROFILE_EVALMAP
#define PROFILE_OBJECT_LITERALS
#define PROFILE_BAILOUT_RECORD_MEMORY
#define MEMSPECT_TRACKING
#define PROFILE_RECYCLER_ALLOC
#define PROFILE_STRINGS
#define PROFILE_DICTIONARY 1
#define RECYCLER_SLOW_CHECK_ENABLED          // This can be disabled to speed up the debug build's GC
#define RECYCLER_STRESS
#define RECYCLER_STATS
#define RECYCLER_FINALIZE_CHECK
#define RECYCLER_FREE_MEM_FILL
#define RECYCLER_DUMP_OBJECT_GRAPH
#define RECYCLER_MEMORY_VERIFY
#define RECYCLER_ZERO_MEM_CHECK
#define RECYCLER_TRACE
#define RECYCLER_VERIFY_MARK

#ifdef PERF_COUNTERS
#define RECYCLER_PERF_COUNTERS
#define HEAP_PERF_COUNTERS
#endif // PERF_COUNTERS

#define PAGEALLOCATOR_PROTECT_FREEPAGE
#define ARENA_MEMORY_VERIFY
#define SEPARATE_ARENA
#define HEAP_TRACK_ALLOC
#define CHECK_MEMORY_LEAK
#define LEAK_REPORT


#define PROJECTION_METADATA_TRACE
#define ERROR_TRACE
#define DEBUGGER_TRACE

#define PROPERTY_RECORD_TRACE

#define ARENA_ALLOCATOR_FREE_LIST_SIZE

#ifdef DBG_EXTRAFIELD
#define HEAP_ENUMERATION_VALIDATION
#endif
#endif // DBG_DUMP

//----------------------------------------------------------------------------------------------------
// Special build features
//  - features that can be enabled on private builds for debugging
//----------------------------------------------------------------------------------------------------
#ifdef ENABLE_JS_ETW
// #define ETW_MEMORY_TRACKING          // ETW events for internal allocations
#endif
// #define OLD_ITRACKER                 // Switch to the old IE8 ITracker GUID
// #define LOG_BYTECODE_AST_RATIO       // log the ratio between AST size and bytecode generated.
// #define DUMP_FRAGMENTATION_STATS        // Display HeapBucket fragmentation stats after sweep

// ----- Fretest or free build special build features (already enabled in debug builds) -----
// #define TRACK_DISPATCH

// #define BGJIT_STATS

// Profile defines that can be enabled in release build
// #define PROFILE_EXEC
// #define PROFILE_MEM
// #define PROFILE_STRINGS
// #define PROFILE_TYPES
// #define PROFILE_OBJECT_LITERALS
// #define PROFILE_RECYCLER_ALLOC
// #define MEMSPECT_TRACKING

// #define HEAP_TRACK_ALLOC

// Recycler defines that can be enabled in release build
// #define RECYCLER_STRESS
// #define RECYCLER_STATS
// #define RECYCLER_FINALIZE_CHECK
// #define RECYCLER_FREE_MEM_FILL
// #define RECYCLER_DUMP_OBJECT_GRAPH
// #define RECYCLER_MEMORY_VERIFY
// #define RECYCLER_TRACE
// #define RECYCLER_VERIFY_MARK

// #ifdef PERF_COUNTERS
// #define RECYCLER_PERF_COUNTERS
// #define HEAP_PERF_COUNTERS
// #endif //PERF_COUNTERS

// Other defines that can be enabled in release build
// #define PAGEALLOCATOR_PROTECT_FREEPAGE
// #define ARENA_MEMORY_VERIFY
// #define SEPARATE_ARENA
// #define LEAK_REPORT
// #define CHECK_MEMORY_LEAK
// #define RECYCLER_MARK_TRACK
// #define INTERNAL_MEM_PROTECT_HEAP_ALLOC

//----------------------------------------------------------------------------------------------------
// Disabled features
//----------------------------------------------------------------------------------------------------
//Enable/disable dom properties
#define DOMEnabled 0

//----------------------------------------------------------------------------------------------------
// Platform dependent flags
//----------------------------------------------------------------------------------------------------
#ifndef INT32VAR
#if defined(_M_X64_OR_ARM64)
#define INT32VAR 1
#else
#define INT32VAR 0
#endif
#endif

#ifndef FLOATVAR
#if defined(_M_X64)
#define FLOATVAR 1
#else
#define FLOATVAR 0
#endif
#endif

#if defined(_M_IX86) || defined(_M_X64)
#ifndef TEMP_DISABLE_ASMJS
#define ASMJS_PLAT
#endif
#endif

#if _WIN32 || _WIN64
#if _M_IX86
#define I386_ASM 1
#endif //_M_IX86
#endif // _WIN32 || _WIN64

#ifndef PDATA_ENABLED
#if defined(_M_ARM32_OR_ARM64) || defined(_M_X64)
#define PDATA_ENABLED 1
#else
#define PDATA_ENABLED 0
#endif
#endif

//----------------------------------------------------------------------------------------------------
// Dependent flags
//  - flags values that are dependent on other flags
//----------------------------------------------------------------------------------------------------

#ifndef CONCURRENT_GC_ENABLED
#undef IDLE_DECOMMIT_ENABLED   // Currently idle decommit can only be enabled if concurrent gc is enabled
#endif

#ifdef BAILOUT_INJECTION
#define ENABLE_PREJIT
#endif

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS)
// Enable Output::Trace
#define ENABLE_TRACE
#endif

#if DBG || defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT) || defined(TRACK_DISPATCH) || defined(ENABLE_TRACE) || defined(RECYCLER_PAGE_HEAP)
#define STACK_BACK_TRACE
#endif

#if defined(STACK_BACK_TRACE) || defined(CONTROL_FLOW_GUARD_LOGGER)
#define DBGHELP_SYMBOL_MANAGER
#endif

#if defined(TRACK_DISPATCH) || defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
#define TRACK_JS_DISPATCH
#endif

// LEAK_REPORT and CHECK_MEMORY_LEAK requires RECYCLER_DUMP_OBJECT_GRAPH
// HEAP_TRACK_ALLOC and RECYCLER_STATS
#if defined(LEAK_REPORT) || defined(CHECK_MEMORY_LEAK)
#define RECYCLER_DUMP_OBJECT_GRAPH
#define HEAP_TRACK_ALLOC
#define RECYCLER_STATS
#endif

// PROFILE_RECYCLER_ALLOC requires PROFILE_MEM
#if defined(PROFILE_RECYCLER_ALLOC) && !defined(PROFILE_MEM)
#define PROFILE_MEM
#endif

// RECYCLER_DUMP_OBJECT_GRAPH is needed when using PROFILE_RECYCLER_ALLOC
#if defined(PROFILE_RECYCLER_ALLOC) && !defined(RECYCLER_DUMP_OBJECT_GRAPH)
#define RECYCLER_DUMP_OBJECT_GRAPH
#endif


#if defined(HEAP_TRACK_ALLOC) || defined(PROFILE_RECYCLER_ALLOC)
#define TRACK_ALLOC
#define TRACE_OBJECT_LIFETIME           // track a particular object's lifetime
#endif

#if defined(USED_IN_STATIC_LIB)
#undef FAULT_INJECTION
#undef RECYCLER_DUMP_OBJECT_GRAPH
#undef HEAP_TRACK_ALLOC
#undef RECYCLER_STATS
#undef PERF_COUNTERS
#endif

// Not having the config options enabled trumps all the above logic for these switches
#ifndef ENABLE_DEBUG_CONFIG_OPTIONS
#undef ARENA_MEMORY_VERIFY
#undef RECYCLER_MEMORY_VERIFY
#undef PROFILE_MEM
#undef PROFILE_DICTIONARY
#undef PROFILE_RECYCLER_ALLOC
#undef PROFILE_EXEC
#undef PROFILE_EVALMAP
#undef FAULT_INJECTION
#undef RECYCLER_STRESS
#undef RECYCLER_SLOW_VERIFY
#undef RECYCLER_VERIFY_MARK
#undef RECYCLER_STATS
#undef RECYCLER_FINALIZE_CHECK
#undef RECYCLER_DUMP_OBJECT_GRAPH
#undef DBG_DUMP
#undef BGJIT_STATS
#undef EXCEPTION_RECOVERY
#undef PROFILE_STRINGS
#undef PROFILE_TYPES
#undef PROFILE_OBJECT_LITERALS
#undef SECURITY_TESTING
#undef LEAK_REPORT
#endif

//----------------------------------------------------------------------------------------------------
// Default flags values
//  - Set the default values of flags if it is not set by the command line or above
//----------------------------------------------------------------------------------------------------
#ifndef JS_PROFILE_DATA_INTERFACE
#define JS_PROFILE_DATA_INTERFACE 0
#endif

#ifndef PROFILE_DICTIONARY
#define PROFILE_DICTIONARY 0
#endif
