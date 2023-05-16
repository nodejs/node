// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PRESUBMIT_INTENTIONALLY_MISSING_INCLUDE_GUARD

// This file undefines Turboshaft's assembler macros. Include this file after
// your reducer and don't forget to include 'define-assembler-macros.inc' before.

#ifndef V8_COMPILER_TURBOSHAFT_ASSEMBLER_MACROS_DEFINED
#error \
    "Assembler macros not defined. Did you forget to #include \"define-assembler-macros.inc\" in this file?"
#endif

#undef __

#undef BIND
#undef ELSE
#undef ELSE_IF
#undef ELSE_IF_LIKELY
#undef ELSE_IF_UNLIKELY
#undef ELSE_IF_WITH_HINT
#undef END_IF
#undef GOTO
#undef GOTO_IF
#undef GOTO_IF_LIKELY
#undef GOTO_IF_NOT
#undef GOTO_IF_NOT_LIKELY
#undef GOTO_IF_NOT_UNLIKELY
#undef GOTO_IF_UNLIKELY
#undef IF
#undef IF_LIKELY
#undef IF_UNLIKELY
#undef IF_WITH_HINT
#undef IF_NOT
#undef IF_NOT_LIKELY
#undef IF_NOT_UNLIKELY
#undef LOOP

#undef V8_COMPILER_TURBOSHAFT_ASSEMBLER_MACROS_DEFINED
