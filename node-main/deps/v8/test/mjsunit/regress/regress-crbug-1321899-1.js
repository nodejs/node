// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/regress/regress-crbug-1321899.js');

// Detached global should not have access
const realm = Realm.createAllowCrossRealmAccess();
const detached = Realm.global(realm);
Realm.detachGlobal(realm);

checkNoAccess(detached, /no access/);
