// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that the fallback path in RegExpExec executes the default exec
// implementation.
delete RegExp.prototype.exec;
assertEquals("b", "aba".replace(/a/g, ""));
