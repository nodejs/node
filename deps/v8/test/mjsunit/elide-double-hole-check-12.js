// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f1(a, i) {
  return a[i] + 0.5;
}

var other_realm = Realm.create();
var arr = [,0.0,2.5];
assertEquals(0.5, f1(arr, 1));
assertEquals(0.5, f1(arr, 1));
%OptimizeFunctionOnNextCall(f1);
assertEquals(0.5, f1(arr, 1));

Realm.shared = arr.__proto__;

// The call syntax is useful to make sure the native context is that of the
// other realm.
Realm.eval(other_realm, "Array.prototype.push.call(Realm.shared, 1);");
assertEquals(1.5, f1(arr, 0));
