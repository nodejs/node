// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Sample V8 flag definitions for `test_d8_flags.py`.

#define SAMPLE_DEFAULT_ON true

#ifdef SOME_BUILD_FLAG
#define AMBIGUOUS_DEFAULT true
#else
#define AMBIGUOUS_DEFAULT false
#endif

#define DEFINE_BOOL(nam, def, cmt) FLAG(BOOL, bool, nam, def, cmt)
#define DEFINE_ALIAS_BOOL(alias, nam) FLAG_ALIAS(BOOL, bool, alias, nam)
#define DEFINE_IMPLICATION(cond, thenflag) \
  DEFINE_VALUE_IMPLICATION(cond, thenflag, true)

DEFINE_BOOL(maglev_inlining, true, "inline in maglev")
DEFINE_BOOL(always_osr, false, "always osr")
DEFINE_DEBUG_BOOL(wasm_opt, true, "optimize wasm")
DEFINE_BOOL(turbo_verify, DEBUG_BOOL, "verify turbofan")
DEFINE_BOOL(macro_default_on, SAMPLE_DEFAULT_ON, "enabled by macro")
DEFINE_BOOL(macro_default_off, !SAMPLE_DEFAULT_ON, "disabled by negated macro")
DEFINE_BOOL(ambiguous_flag, AMBIGUOUS_DEFAULT, "depends on build")
DEFINE_EXPERIMENTAL_FEATURE(maglev_future, "maglev future")
DEFINE_DEVELOPER_FLAG(trace_sample, "developer trace flag")
DEFINE_UNSAFE_FLAG(unsafe_sample, "unsafe test flag")
DEFINE_VERIFICATION_FLAG(verify_sample, "verification flag")

DEFINE_ALIAS_BOOL(shared_string_table, shared_strings)

DEFINE_IMPLICATION(maglev_future, maglev)
DEFINE_NEG_IMPLICATION(jitless, opt)
DEFINE_DISABLE_FLAG_IMPLICATION(fuzzing, trace_opt)
DEFINE_NEG_NEG_IMPLICATION(maglev, maglev_inlining)
DEFINE_VALUE_IMPLICATION(always_osr_from_maglev, osr_from_maglev, 4)
DEFINE_NEG_VALUE_IMPLICATION(turbofan, max_opt, 2)
DEFINE_NEG_VALUE_IMPLICATION(sparkplug, always_sparkplug, false)
