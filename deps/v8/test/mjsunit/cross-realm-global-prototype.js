// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Make sure we don't accidentally leak information about other objects.

var realm = Realm.create();
var test = Realm.eval(realm,
    "() => { return Realm.global(0) instanceof Object }");
%PrepareFunctionForOptimization(test);

assertFalse(test());

// Set the prototype of the current global object to the global object of the
// other realm.
__proto__ = Realm.eval(realm, "this");
assertFalse(test());

test();
test();
%OptimizeFunctionOnNextCall(test);
assertEquals(false, test());
