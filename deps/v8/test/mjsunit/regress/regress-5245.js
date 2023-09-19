// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

// After captureStackTrace.

var a = {};
Error.captureStackTrace(a, Error);
a.stack = 1;  // Should not throw, stack should be writable.

// After the standard Error constructor.

var b = new Error();
b.stack = 1;  // Should not throw, stack should be writable.
b.stack = 1;  // Still writable.

// After read access to stack.

var c = new Error();
var old_stack = c.stack;
c.stack = 1;  // Should not throw, stack should be writable.
c.stack = 1;  // Still writable.
