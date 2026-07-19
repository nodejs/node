// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const re = /$/;

// The runtime path (Runtime::kRegExpExec).
assertEquals(["a"], "a".split(re));
assertEquals("", RegExp.input);

// Runtime / compilation to generated code.
assertEquals(["a"], "a".split(re));
assertEquals("", RegExp.input);

// Generated code.
assertEquals(["a"], "a".split(re));
assertEquals("", RegExp.input);

// Once again just because we can.
assertEquals(["a"], "a".split(re));
assertEquals("", RegExp.input);
