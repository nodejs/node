// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/deoptimizer.h"
#include "src/frames-inl.h"
#include "src/full-codegen/full-codegen.h"
#include "src/snapshot/natives.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_DeoptimizeFunction) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  if (!function->IsOptimized()) return isolate->heap()->undefined_value();

  // TODO(turbofan): Deoptimization is not supported yet.
  if (function->code()->is_turbofanned() &&
      function->shared()->asm_function() && !FLAG_turbo_asm_deoptimization) {
    return isolate->heap()->undefined_value();
  }

  Deoptimizer::DeoptimizeFunction(*function);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DeoptimizeNow) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);

  Handle<JSFunction> function;

  // If the argument is 'undefined', deoptimize the topmost
  // function.
  JavaScriptFrameIterator it(isolate);
  while (!it.done()) {
    if (it.frame()->is_java_script()) {
      function = Handle<JSFunction>(it.frame()->function());
      break;
    }
  }
  if (function.is_null()) return isolate->heap()->undefined_value();

  if (!function->IsOptimized()) return isolate->heap()->undefined_value();

  // TODO(turbofan): Deoptimization is not supported yet.
  if (function->code()->is_turbofanned() &&
      function->shared()->asm_function() && !FLAG_turbo_asm_deoptimization) {
    return isolate->heap()->undefined_value();
  }

  Deoptimizer::DeoptimizeFunction(*function);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_RunningInSimulator) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
#if defined(USE_SIMULATOR)
  return isolate->heap()->true_value();
#else
  return isolate->heap()->false_value();
#endif
}


RUNTIME_FUNCTION(Runtime_IsConcurrentRecompilationSupported) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  return isolate->heap()->ToBoolean(
      isolate->concurrent_recompilation_enabled());
}


RUNTIME_FUNCTION(Runtime_OptimizeFunctionOnNextCall) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 1 || args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  // The following assertion was lifted from the DCHECK inside
  // JSFunction::MarkForOptimization().
  RUNTIME_ASSERT(function->shared()->allows_lazy_compilation() ||
                 (function->code()->kind() == Code::FUNCTION &&
                  !function->shared()->optimization_disabled()));

  // If the function is already optimized, just return.
  if (function->IsOptimized()) return isolate->heap()->undefined_value();

  function->MarkForOptimization();

  Code* unoptimized = function->shared()->code();
  if (args.length() == 2 && unoptimized->kind() == Code::FUNCTION) {
    CONVERT_ARG_HANDLE_CHECKED(String, type, 1);
    if (type->IsOneByteEqualTo(STATIC_CHAR_VECTOR("concurrent")) &&
        isolate->concurrent_recompilation_enabled()) {
      function->AttemptConcurrentOptimization();
    }
  }

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_OptimizeOsr) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 0 || args.length() == 1);
  Handle<JSFunction> function = Handle<JSFunction>::null();

  if (args.length() == 0) {
    // Find the JavaScript function on the top of the stack.
    JavaScriptFrameIterator it(isolate);
    while (!it.done()) {
      if (it.frame()->is_java_script()) {
        function = Handle<JSFunction>(it.frame()->function());
        break;
      }
    }
    if (function.is_null()) return isolate->heap()->undefined_value();
  } else {
    // Function was passed as an argument.
    CONVERT_ARG_HANDLE_CHECKED(JSFunction, arg, 0);
    function = arg;
  }

  // The following assertion was lifted from the DCHECK inside
  // JSFunction::MarkForOptimization().
  RUNTIME_ASSERT(function->shared()->allows_lazy_compilation() ||
                 !function->shared()->optimization_disabled());

  // If the function is already optimized, just return.
  if (function->IsOptimized()) return isolate->heap()->undefined_value();

  Code* unoptimized = function->shared()->code();
  if (unoptimized->kind() == Code::FUNCTION) {
    DCHECK(BackEdgeTable::Verify(isolate, unoptimized));
    isolate->runtime_profiler()->AttemptOnStackReplacement(
        *function, Code::kMaxLoopNestingMarker);
  }

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_NeverOptimizeFunction) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  function->shared()->set_disable_optimization_reason(kOptimizationDisabled);
  function->shared()->set_optimization_disabled(true);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GetOptimizationStatus) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 1 || args.length() == 2);
  if (!isolate->use_crankshaft()) {
    return Smi::FromInt(4);  // 4 == "never".
  }
  bool sync_with_compiler_thread = true;
  if (args.length() == 2) {
    CONVERT_ARG_HANDLE_CHECKED(String, sync, 1);
    if (sync->IsOneByteEqualTo(STATIC_CHAR_VECTOR("no sync"))) {
      sync_with_compiler_thread = false;
    }
  }
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  if (isolate->concurrent_recompilation_enabled() &&
      sync_with_compiler_thread) {
    while (function->IsInOptimizationQueue()) {
      isolate->optimizing_compile_dispatcher()->InstallOptimizedFunctions();
      base::OS::Sleep(base::TimeDelta::FromMilliseconds(50));
    }
  }
  if (FLAG_always_opt || FLAG_prepare_always_opt) {
    // With --always-opt, optimization status expectations might not
    // match up, so just return a sentinel.
    return Smi::FromInt(3);  // 3 == "always".
  }
  if (FLAG_deopt_every_n_times) {
    return Smi::FromInt(6);  // 6 == "maybe deopted".
  }
  if (function->IsOptimized() && function->code()->is_turbofanned()) {
    return Smi::FromInt(7);  // 7 == "TurboFan compiler".
  }
  return function->IsOptimized() ? Smi::FromInt(1)   // 1 == "yes".
                                 : Smi::FromInt(2);  // 2 == "no".
}


RUNTIME_FUNCTION(Runtime_UnblockConcurrentRecompilation) {
  DCHECK(args.length() == 0);
  RUNTIME_ASSERT(FLAG_block_concurrent_recompilation);
  RUNTIME_ASSERT(isolate->concurrent_recompilation_enabled());
  isolate->optimizing_compile_dispatcher()->Unblock();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GetOptimizationCount) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  return Smi::FromInt(function->shared()->opt_count());
}


RUNTIME_FUNCTION(Runtime_GetUndetectable) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);

  Local<v8::ObjectTemplate> desc = v8::ObjectTemplate::New(v8_isolate);
  desc->MarkAsUndetectable();
  Local<v8::Object> obj;
  if (!desc->NewInstance(v8_isolate->GetCurrentContext()).ToLocal(&obj)) {
    return nullptr;
  }
  return *Utils::OpenHandle(*obj);
}


RUNTIME_FUNCTION(Runtime_ClearFunctionTypeFeedback) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  function->shared()->ClearTypeFeedbackInfo();
  Code* unoptimized = function->shared()->code();
  if (unoptimized->kind() == Code::FUNCTION) {
    unoptimized->ClearInlineCaches();
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_NotifyContextDisposed) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  isolate->heap()->NotifyContextDisposed(true);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetAllocationTimeout) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2 || args.length() == 3);
#ifdef DEBUG
  CONVERT_SMI_ARG_CHECKED(interval, 0);
  CONVERT_SMI_ARG_CHECKED(timeout, 1);
  isolate->heap()->set_allocation_timeout(timeout);
  FLAG_gc_interval = interval;
  if (args.length() == 3) {
    // Enable/disable inline allocation if requested.
    CONVERT_BOOLEAN_ARG_CHECKED(inline_allocation, 2);
    if (inline_allocation) {
      isolate->heap()->EnableInlineAllocation();
    } else {
      isolate->heap()->DisableInlineAllocation();
    }
  }
#endif
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugPrint) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);

  OFStream os(stdout);
#ifdef DEBUG
  if (args[0]->IsString()) {
    // If we have a string, assume it's a code "marker"
    // and print some interesting cpu debugging info.
    JavaScriptFrameIterator it(isolate);
    JavaScriptFrame* frame = it.frame();
    os << "fp = " << static_cast<void*>(frame->fp())
       << ", sp = " << static_cast<void*>(frame->sp())
       << ", caller_sp = " << static_cast<void*>(frame->caller_sp()) << ": ";
  } else {
    os << "DebugPrint: ";
  }
  args[0]->Print(os);
  if (args[0]->IsHeapObject()) {
    os << "\n";
    HeapObject::cast(args[0])->map()->Print(os);
  }
#else
  // ShortPrint is available in release mode. Print is not.
  os << Brief(args[0]);
#endif
  os << std::endl;

  return args[0];  // return TOS
}


RUNTIME_FUNCTION(Runtime_DebugTrace) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  isolate->PrintStack(stdout);
  return isolate->heap()->undefined_value();
}


// This will not allocate (flatten the string), but it may run
// very slowly for very deeply nested ConsStrings.  For debugging use only.
RUNTIME_FUNCTION(Runtime_GlobalPrint) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_CHECKED(String, string, 0);
  StringCharacterStream stream(string);
  while (stream.HasMore()) {
    uint16_t character = stream.GetNext();
    PrintF("%c", character);
  }
  return string;
}


RUNTIME_FUNCTION(Runtime_SystemBreak) {
  // The code below doesn't create handles, but when breaking here in GDB
  // having a handle scope might be useful.
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  base::OS::DebugBreak();
  return isolate->heap()->undefined_value();
}


// Sets a v8 flag.
RUNTIME_FUNCTION(Runtime_SetFlags) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(String, arg, 0);
  base::SmartArrayPointer<char> flags =
      arg->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  FlagList::SetFlagsFromString(flags.get(), StrLength(flags.get()));
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_Abort) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_SMI_ARG_CHECKED(message_id, 0);
  const char* message =
      GetBailoutReason(static_cast<BailoutReason>(message_id));
  base::OS::PrintError("abort: %s\n", message);
  isolate->PrintStack(stderr);
  base::OS::Abort();
  UNREACHABLE();
  return NULL;
}


RUNTIME_FUNCTION(Runtime_AbortJS) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, message, 0);
  base::OS::PrintError("abort: %s\n", message->ToCString().get());
  isolate->PrintStack(stderr);
  base::OS::Abort();
  UNREACHABLE();
  return NULL;
}


RUNTIME_FUNCTION(Runtime_NativeScriptsCount) {
  DCHECK(args.length() == 0);
  return Smi::FromInt(Natives::GetBuiltinsCount());
}


// Returns V8 version as a string.
RUNTIME_FUNCTION(Runtime_GetV8Version) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);

  const char* version_string = v8::V8::GetVersion();

  return *isolate->factory()->NewStringFromAsciiChecked(version_string);
}


RUNTIME_FUNCTION(Runtime_DisassembleFunction) {
  HandleScope scope(isolate);
#ifdef DEBUG
  DCHECK(args.length() == 1);
  // Get the function and make sure it is compiled.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, func, 0);
  if (!Compiler::Compile(func, KEEP_EXCEPTION)) {
    return isolate->heap()->exception();
  }
  OFStream os(stdout);
  func->code()->Print(os);
  os << std::endl;
#endif  // DEBUG
  return isolate->heap()->undefined_value();
}

namespace {

int StackSize(Isolate* isolate) {
  int n = 0;
  for (JavaScriptFrameIterator it(isolate); !it.done(); it.Advance()) n++;
  return n;
}

void PrintIndentation(Isolate* isolate) {
  const int nmax = 80;
  int n = StackSize(isolate);
  if (n <= nmax) {
    PrintF("%4d:%*s", n, n, "");
  } else {
    PrintF("%4d:%*s", n, nmax, "...");
  }
}

}  // namespace

RUNTIME_FUNCTION(Runtime_TraceEnter) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  PrintIndentation(isolate);
  JavaScriptFrame::PrintTop(isolate, stdout, true, false);
  PrintF(" {\n");
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_TraceExit) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  PrintIndentation(isolate);
  PrintF("} -> ");
  obj->ShortPrint();
  PrintF("\n");
  return obj;  // return TOS
}

RUNTIME_FUNCTION(Runtime_TraceTailCall) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  PrintIndentation(isolate);
  PrintF("} -> tail call ->\n");
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_HaveSameMap) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(JSObject, obj1, 0);
  CONVERT_ARG_CHECKED(JSObject, obj2, 1);
  return isolate->heap()->ToBoolean(obj1->map() == obj2->map());
}


RUNTIME_FUNCTION(Runtime_InNewSpace) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(isolate->heap()->InNewSpace(obj));
}


#define ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(Name)       \
  RUNTIME_FUNCTION(Runtime_Has##Name) {                  \
    CONVERT_ARG_CHECKED(JSObject, obj, 0);               \
    return isolate->heap()->ToBoolean(obj->Has##Name()); \
  }

ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastSmiElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastObjectElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastSmiOrObjectElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastDoubleElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastHoleyElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(DictionaryElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(SloppyArgumentsElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FixedTypedArrayElements)
// Properties test sitting with elements tests - not fooling anyone.
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastProperties)

#undef ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION


#define FIXED_TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION(Type, type, TYPE, ctype, s) \
  RUNTIME_FUNCTION(Runtime_HasFixed##Type##Elements) {                        \
    CONVERT_ARG_CHECKED(JSObject, obj, 0);                                    \
    return isolate->heap()->ToBoolean(obj->HasFixed##Type##Elements());       \
  }

TYPED_ARRAYS(FIXED_TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION)

#undef FIXED_TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION
}  // namespace internal
}  // namespace v8
