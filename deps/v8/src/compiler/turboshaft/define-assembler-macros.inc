// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PRESUBMIT_INTENTIONALLY_MISSING_INCLUDE_GUARD

// This file defines Turboshaft's assembler macros. Include this file before
// your reducers and don't forget to include 'undef-assembler-macros.inc'
// afterwards.

#ifdef V8_COMPILER_TURBOSHAFT_ASSEMBLER_MACROS_DEFINED
#error \
    "Assembler macros already defined. Did you forget to #include \"undef-assembler-macros.inc\" in a previous file?"
#endif

#define V8_COMPILER_TURBOSHAFT_ASSEMBLER_MACROS_DEFINED 1

#define LIKELY(...) ConditionWithHint(__VA_ARGS__, BranchHint::kTrue)
#define UNLIKELY(...) ConditionWithHint(__VA_ARGS__, BranchHint::kFalse)

#define BIND(label, ...)                                                  \
  auto [CONCAT(is_bound_, __LINE__), ##__VA_ARGS__] =                     \
    Asm().ControlFlowHelper_Bind(label);                                  \
    (::v8::internal::compiler::turboshaft::detail::SuppressUnusedWarning( \
        CONCAT(is_bound_, __LINE__)))
#define LOOP(loop_label, ...) \
  for(auto [CONCAT(run_loop_, __LINE__), ##__VA_ARGS__] =                      \
    Asm().ControlFlowHelper_BindLoop(loop_label); CONCAT(run_loop_, __LINE__); \
        Asm().ControlFlowHelper_EndLoop(loop_label),                           \
        CONCAT(run_loop_, __LINE__) = false)
#define GOTO(label, ...)                             \
  Asm().ControlFlowHelper_Goto(label, {__VA_ARGS__})
#define GOTO_IF(cond, label, ...)                            \
  Asm().ControlFlowHelper_GotoIf(cond, label, {__VA_ARGS__})
#define GOTO_IF_NOT(cond, label, ...)                           \
  Asm().ControlFlowHelper_GotoIfNot(cond, label, {__VA_ARGS__})

#define IF(...)                                                            \
  for (bool bound = Asm().ControlFlowHelper_If(__VA_ARGS__, false); bound; \
       (bound = false), Asm().ControlFlowHelper_GotoEnd())
#define IF_NOT(...)                                                       \
  for (bool bound = Asm().ControlFlowHelper_If(__VA_ARGS__, true); bound; \
       (bound = false), Asm().ControlFlowHelper_GotoEnd())
#define ELSE_IF(...)                                               \
   for (bool bound = Asm().ControlFlowHelper_ElseIf(               \
                                   [&]() { return __VA_ARGS__; }); \
        bound; (bound = false), Asm().ControlFlowHelper_GotoEnd())
#define ELSE                                               \
  for (bool bound = Asm().ControlFlowHelper_Else(); bound; \
       (bound = false), Asm().ControlFlowHelper_GotoEnd())
#define END_IF Asm().ControlFlowHelper_EndIf();

#ifdef DEBUG
// In debug builds, `Reduce(operation)` makes sure that `operation##Op` exists by using this name in an expression. This will detect typos in the name which would otherwise stay unnoticed potentially.
#define REDUCE(operation) \
  CONCAT(CHECK_Reduce, operation)                                             \
      = (::v8::internal::compiler::turboshaft::detail::SuppressUnusedWarning( \
            std::is_same_v<operation##Op, operation##Op>),                    \
         OpIndex{});                                                          \
  decltype(CONCAT(CHECK_Reduce, operation)) Reduce##operation
#define REDUCE_INPUT_GRAPH(operation)                                         \
  CONCAT(CHECK_ReduceInputGraph, operation)                                   \
      = (::v8::internal::compiler::turboshaft::detail::SuppressUnusedWarning( \
            std::is_same_v<operation##Op, operation##Op>),                    \
            OpIndex{});                                                       \
  decltype(CONCAT(CHECK_ReduceInputGraph, operation))                         \
          ReduceInputGraph##operation
#else
#define REDUCE(operation) Reduce##operation
#define REDUCE_INPUT_GRAPH(operation) ReduceInputGraph##operation
#endif  // DEBUG

#define __ Asm().
