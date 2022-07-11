// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/regress/regress-crbug-1321899.js');

// Detached global should not have access
const realm = Realm.createAllowCrossRealmAccess();
const detached = Realm.global(realm);
Realm.detachGlobal(realm);

assertThrows(() => new B(detached), Error, /no access/);
assertThrows(() => new C(detached), Error, /no access/);
assertThrows(() => new D(detached), Error, /no access/);
assertThrows(() => new E(detached), Error, /no access/);
assertThrows(() => B.setField(detached), Error, /no access/);
assertThrows(() => C.setField(detached), Error, /no access/);
assertThrows(() => D.setAccessor(detached), Error, /no access/);
assertThrows(() => E.setMethod(detached), Error, /no access/);
assertThrows(() => D.getAccessor(detached), Error, /no access/);
assertThrows(() => E.getMethod(detached), Error, /no access/);
