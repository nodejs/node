// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo() { return 1; }
var x = {toJSON: foo.bind()};
assertEquals("1", JSON.stringify(x));
