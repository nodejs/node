// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(fnConstructor) {
    return Object.is(new fnConstructor(), undefined);
}

const realmIndex = Realm.createAllowCrossRealmAccess();
const otherFunction = Realm.global(realmIndex).Function;
Realm.detachGlobal(realmIndex);

%PrepareFunctionForOptimization(f);
assertFalse(f(Function));
assertThrows(_ => f(otherFunction));
%OptimizeFunctionOnNextCall(f);
assertThrows(_ => f(otherFunction));
