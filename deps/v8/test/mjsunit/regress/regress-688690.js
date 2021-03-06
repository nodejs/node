// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var foo = "01234567";

foo += foo;
foo += foo;
foo += foo;
foo += foo;
foo += foo;  // foo.length = 256;

// Create an adaptor frame, and take the StringReplaceOneCharWithString runtime
// fast path. This crashed originally since TailCallRuntime could not handle
// adaptor frames.
var bar = foo.replace('x', 'y', 'z');
