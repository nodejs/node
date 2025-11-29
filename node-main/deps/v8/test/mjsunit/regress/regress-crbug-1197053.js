// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

Realm.createAllowCrossRealmAccess();
Realm.detachGlobal(1);
const global_var = Realm.global(1);

function f() {
    return global_var.__proto__;
};

%EnsureFeedbackVectorForFunction(f);
assertThrows(f);
assertThrows(f);
