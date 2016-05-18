// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Error.prepareStackTrace = (e,s) => s;
var constructor = Error().stack[0].constructor;

// Second argument needs to be a function.
assertThrows(()=>constructor({}, {}, 1, false), TypeError);

var receiver = {};
function f() {}

var site = constructor.call(null, receiver, f, {valueOf() { return 0 }}, false);
assertEquals(receiver, site.getThis());
assertEquals(1, site.getLineNumber());
assertEquals(1, site.getColumnNumber());
