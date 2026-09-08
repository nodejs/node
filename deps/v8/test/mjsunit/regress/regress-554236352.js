// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --cache=after-execute

var R = Array.of(() => { 'use strict'; return eval('1'); }, "hello_world");
assertEquals("hello_world", R[1]);
assertEquals(1, R[0]());
