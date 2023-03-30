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

#define BIND(label, ...)                                         \
  auto [CONCAT(is_bound_, __LINE__), ##__VA_ARGS__] =            \
    Asm().ControlFlowHelper_Bind(label);                         \
    (detail::SuppressUnusedWarning(CONCAT(is_bound_, __LINE__)))
#define LOOP(loop_label, ...) \
  for(auto [CONCAT(run_loop_, __LINE__), ##__VA_ARGS__] =                      \
    Asm().ControlFlowHelper_BindLoop(loop_label); CONCAT(run_loop_, __LINE__); \
        Asm().ControlFlowHelper_EndLoop(loop_label),                           \
        CONCAT(run_loop_, __LINE__) = false)
#define GOTO(label, ...)                             \
  Asm().ControlFlowHelper_Goto(label, {__VA_ARGS__})
#define GOTO_IF(cond, label, ...)                            \
  Asm().ControlFlowHelper_GotoIf(cond, label, {__VA_ARGS__}, \
                                 BranchHint::kNone)
#define GOTO_IF_LIKELY(cond, label, ...)                     \
  Asm().ControlFlowHelper_GotoIf(cond, label, {__VA_ARGS__}, \
                                 BranchHint::kTrue)
#define GOTO_IF_UNLIKELY(cond, label, ...)                   \
  Asm().ControlFlowHelper_GotoIf(cond, label, {__VA_ARGS__}, \
                                 BranchHint::kFalse)
#define GOTO_IF_NOT(cond, label, ...)                           \
  Asm().ControlFlowHelper_GotoIfNot(cond, label, {__VA_ARGS__}, \
                                    BranchHint::kNone)
#define GOTO_IF_NOT_LIKELY(cond, label, ...)                    \
  Asm().ControlFlowHelper_GotoIfNot(cond, label, {__VA_ARGS__}, \
                                    BranchHint::kFalse)
#define GOTO_IF_NOT_UNLIKELY(cond, label, ...)                  \
  Asm().ControlFlowHelper_GotoIfNot(cond, label, {__VA_ARGS__}, \
                                    BranchHint::kTrue)

#define IF_WITH_HINT(cond, if_not, hint)                                   \
  for (bool bound = Asm().ControlFlowHelper_If(cond, if_not, hint); bound; \
       (bound = false), Asm().ControlFlowHelper_GotoEnd())
#define IF(cond) IF_WITH_HINT(cond, false, BranchHint::kNone)
#define IF_LIKELY(cond) IF_WITH_HINT(cond, false, BranchHint::kTrue)
#define IF_UNLIKELY(cond) IF_WITH_HINT(cond, false, BranchHint::kFalse)
#define IF_NOT(cond) IF_WITH_HINT(cond, true, BranchHint::kNone)
#define IF_NOT_LIKELY(cond) IF_WITH_HINT(cond, true, BranchHint::kFalse)
#define IF_NOT_UNLIKELY(cond) IF_WITH_HINT(cond, true, BranchHint::kTrue)
#define ELSE_IF_WITH_HINT(cond, hint)                             \
  for (bool bound = Asm().ControlFlowHelper_ElseIf(               \
          [&]() { return cond; }, hint);                          \
       bound; (bound = false), Asm().ControlFlowHelper_GotoEnd())
#define ELSE_IF(cond) ELSE_IF_WITH_HINT(cond, BranchHint::kNone)
#define ELSE_IF_LIKELY(cond) ELSE_IF_WITH_HINT(cond, BranchHint::kTrue)
#define ELSE_IF_UNLIKELY(cond) ELSE_IF_WITH_HINT(cond, BranchHint::kFalse)
#define ELSE                                               \
  for (bool bound = Asm().ControlFlowHelper_Else(); bound; \
       (bound = false), Asm().ControlFlowHelper_GotoEnd())
#define END_IF Asm().ControlFlowHelper_EndIf();

#define __ Asm().
