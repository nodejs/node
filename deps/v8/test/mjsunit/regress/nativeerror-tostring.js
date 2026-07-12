// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[
  EvalError, RangeError, ReferenceError,
  SyntaxError, TypeError, URIError,
].forEach((NativeError) => {
  assertFalse(NativeError.prototype.hasOwnProperty('toString'));
  assertEquals(NativeError.prototype.toString, Error.prototype.toString);
});
