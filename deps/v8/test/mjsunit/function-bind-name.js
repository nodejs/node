// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {}
var fb = f.bind({});
assertEquals('bound f', fb.name);
assertEquals('function bound f() { [native code] }', fb.toString());

Object.defineProperty(f, 'name', {value: 42});
var fb2 = f.bind({});
assertEquals('bound ', fb2.name);
assertEquals('function bound () { [native code] }', fb2.toString());
