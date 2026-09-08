// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Realm.createAllowCrossRealmAccess();

function f() {
  Temporal.Instant;
}
let f_1 = Realm.eval(1, f + " f");
Realm.navigate(1);
f_1();
