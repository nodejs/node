// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Exact boundary: "function " (9) + name + "() { [native code] }" (20) ==
// String::kMaxLength.
const exactFn =
    eval('(function ' + 'b'.repeat(%StringMaxLength() - 29) + '(){})');
assertEquals(%StringMaxLength(), exactFn.toString().length);

let r;
let s;
const hugeFn =
    eval('(function ' + 'a'.repeat(%StringMaxLength() - 28) + '(){r=eval(s)})');

// Function.prototype.toString exceeding String::kMaxLength via
// NativeCodeFunctionSourceString.
assertThrows(() => hugeFn.toString(), RangeError, /Invalid string length/);

// Object::NoSideEffectsToMaybeString on a JSFunction whose ToString overflows
// falls back to "[object Function]" without throwing a RangeError.
assertThrows(() => Promise.prototype.then.call(hugeFn), TypeError,
             /\[object Function\]/);

// Capture a CallSite inside direct eval called from hugeFn.
Error.prepareStackTrace = (err, frames) => frames;
s = 'new Error().stack';
hugeFn();
const frames = r;

// CallSite.prototype.getEvalOrigin and CallSite.prototype.toString exceeding
// String::kMaxLength.
assertThrows(() => frames[0].getEvalOrigin(), RangeError,
             /Invalid string length/);
assertThrows(() => frames[0].toString(), RangeError, /Invalid string length/);

// Default Error.prototype.stack formatting propagates RangeError when an eval
// origin or frame exceeds String::kMaxLength.
Error.prepareStackTrace = undefined;
s = 'new Error()';
hugeFn();
assertThrows(() => r.stack, RangeError, /Invalid string length/);

Error.stackTraceLimit = 1;
hugeFn();
assertThrows(() => r.stack, RangeError, /Invalid string length/);
