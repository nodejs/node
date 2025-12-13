// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev-inline-js-wasm-wrappers --turboshaft-wasm-in-js-inlining
// Flags: --turbolev --no-maglev
// Flags: --trace-turbo-inlining --trace-deopt
// Flags: --allow-natives-syntax --no-stress-incremental-marking

// Test case for JS-to-Wasm wrapper inlining in Turbolev with tracing output.
// Note that there is a variant for 32 bit and 64 bit, since the Wasm-into-JS
// body inlining is not available on 32 bit platforms and thus there are
// differences in the tracing output.
// Please keep the flags above in sync and keep the code in the 64 bit variant.

d8.file.execute('test/message/js-wasm-wrapper-inlining-turbolev-64.js');
