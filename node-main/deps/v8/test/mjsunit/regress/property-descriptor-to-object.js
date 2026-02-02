// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var o = { prop: 1 };
Object.prototype.value = 0;
var d = Object.getOwnPropertyDescriptor(o, "prop");
assertEquals(1, d.value);
