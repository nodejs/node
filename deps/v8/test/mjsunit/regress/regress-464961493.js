// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Realm.createAllowCrossRealmAccess();
Realm.detachGlobal(1);
function f() {
  Temporal.Instant;
}
Realm.eval(1, f + " f()");
