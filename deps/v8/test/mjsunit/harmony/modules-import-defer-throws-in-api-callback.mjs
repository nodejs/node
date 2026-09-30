// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

// Evaluating a deferred module from inside an embedder API callback must not
// leave the module's exception pending on the isolate while the top-level
// capability is rejected.
//
// `print` is a d8 API callback, so calling it leaves an API callback frame on
// the stack. Stringifying the namespace looks up `toString`, which evaluates
// the deferred module; the module throws, rejecting its top-level capability.
// That rejection has no handler, so V8 calls out to the host's rejection hook,
// and d8's hook builds an Error to get a stack trace -- which has to summarize
// the still-live API callback frame. Summarizing it instantiates the callback's
// FunctionTemplate, and ApiNatives' InvokeScope reports whatever exception is
// pending on the isolate. With the module's exception still pending there, that
// happens inside a DisallowExceptions scope and hits a DCHECK.

globalThis.eval_list = [];

import defer * as ns from './modules-skip-import-defer-throws-primitive.mjs';

assertEquals(0, globalThis.eval_list.length);

// Stringifying the namespace inside the API callback evaluates the module,
// which throws. `print` writes nothing, because ToString runs first.
assertThrowsEquals(() => print(ns), 123);

assertArrayEquals(['defer-throws-primitive'], globalThis.eval_list);

// The module is errored: further access rethrows the same value, and does not
// evaluate the module a second time.
assertThrowsEquals(() => ns.foo, 123);
assertArrayEquals(['defer-throws-primitive'], globalThis.eval_list);
