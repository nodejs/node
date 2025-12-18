// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct

// Private names as used by V8-specific APIs like Error.captureStackTrace should
// not bypass shared objects' extensibility check.

assertThrows(() => Error.captureStackTrace(new Atomics.Condition()));
assertThrows(() => Error.captureStackTrace(new Atomics.Mutex()));
assertThrows(() => Error.captureStackTrace(new (new SharedStructType(['p']))));
assertThrows(() => Error.captureStackTrace(new SharedArray(1)));
