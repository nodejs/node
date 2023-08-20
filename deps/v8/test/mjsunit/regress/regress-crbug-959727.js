// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';
let r = Realm.createAllowCrossRealmAccess();
Realm.detachGlobal(r);
try {
  Realm.global(r)[1] = 0;
} catch (e) {
}
