// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

let realms = [];
for (let i = 0; i < 4; i++) {
  realms.push(Realm.createAllowCrossRealmAccess());
}

for (let i = 0; i < 4; i++) {
  Realm.detachGlobal(realms[i]);
  gc();
}
