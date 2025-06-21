// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var re = /[bc]/;
var str = "baba";

assertEquals(["", "a", "a"], str.split(re));

// Force slow path.
re.exec = (string) => RegExp.prototype.exec.call(re, string);
assertEquals(["", "a", "a"], str.split(re));
