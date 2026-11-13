// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval --bundle

// JS_BUNDLE_MODULE:mod_term.mjs
export var foo = 42;
d8.terminate();
(() => {})();

// JS_BUNDLE_MODULE_ENTRYPOINT
import defer * as ns from './mod_term.mjs';

// First task triggers synchronous evaluation of `mod_term.mjs`, which
// terminates during InnerModuleEvaluation and transitions `mod_term.mjs` to
// `kErrored`.
setTimeout(() => {
  ns.foo;
}, 0);

// Second task runs after d8 clears the termination exception and accesses
// `ns.foo` on the `kErrored` module. `foo` is a `var` so that the access can't
// throw because of TDZ.
setTimeout(() => {
  assertThrowsEquals(() => ns.foo, null);
  assertThrowsEquals(() => ns.foo, null);
}, 0);
