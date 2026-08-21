// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Object.defineProperty(Boolean.prototype, "v", {get:constructor});

function foo(b) { return b.v; }

foo(true);
foo(true);
foo(true);
