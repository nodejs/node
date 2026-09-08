// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval --bundle

// Regression test for crbug.com/529641841: forcing synchronous evaluation of a
// deferred module from deep within a recursion can overflow the stack at the
// InnerModuleEvaluation entry STACK_CHECK, before the module is pushed onto the
// evaluation stack. The module's [[EvaluationError]] must still be recorded so
// the top-level capability rejects with the actual RangeError instead of the
// EMPTY (TheHole) sentinel, which used to crash in JSPromise::Reject.

// JS_BUNDLE_MODULE:deferred.mjs
// Intentionally empty: the entrypoint overflows the stack at the
// InnerModuleEvaluation entry STACK_CHECK, before this module's body would
// ever run.

// JS_BUNDLE_MODULE_ENTRYPOINT
import defer * as ns from './deferred.mjs';

function recurse() {
  // Recurse first to drive the native stack close to exhaustion, then touch
  // the deferred namespace via Object.defineProperty (same path as the
  // original testcase: DefineOwnProperty -> GetOwnPropertyDescriptor ->
  // GetPropertyAttributes -> EvaluateModuleSync).
  try { recurse(); } catch(e) { };
  Object.defineProperty(ns, 'x', { value: 1 });
}

// Must throw a catchable RangeError rather than fatally crashing.
assertThrows(recurse, RangeError);
