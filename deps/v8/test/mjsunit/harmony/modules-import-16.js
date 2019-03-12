// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-dynamic-import

var ran = false;
var x;

var body = "import('modules-skip-1.js').then(ns => { x = ns.life();" +
    " ran = true;} ).catch(err => %AbortJS(err))"
var func = new Function(body);
func();

%PerformMicrotaskCheckpoint();
assertEquals(42, x);
assertTrue(ran);

var ran = false;
var body = "import('modules-skip-1.js').then(ns => { x = ns.life();" +
    " ran = true;} ).catch(err => %AbortJS(err))"
eval("var func = new Function(body); func();");

%PerformMicrotaskCheckpoint();
assertEquals(42, x);
assertTrue(ran);

var ran = false;
var body = "eval(import('modules-skip-1.js').then(ns => { x = ns.life();" +
    " ran = true;} ).catch(err => %AbortJS(err)))"
var func = new Function(body);
func();

%PerformMicrotaskCheckpoint();
assertEquals(42, x);
assertTrue(ran);
