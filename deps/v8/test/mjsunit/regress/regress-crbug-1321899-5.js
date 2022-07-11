// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/regress/regress-crbug-1321899.js');

const realm = Realm.create();
const globalProxy = Realm.global(realm);

assertThrows(() => new B(globalProxy), Error, /no access/);
assertThrows(() => new C(globalProxy), Error, /no access/);
assertThrows(() => new D(globalProxy), Error, /no access/);
assertThrows(() => new E(globalProxy), Error, /no access/);
assertThrows(() => B.setField(globalProxy), Error, /no access/);
assertThrows(() => C.setField(globalProxy), Error, /no access/);
assertThrows(() => D.setAccessor(globalProxy), Error, /no access/);
assertThrows(() => E.setMethod(globalProxy), Error, /no access/);
assertThrows(() => D.getAccessor(globalProxy), Error, /no access/);
assertThrows(() => E.getMethod(globalProxy), Error, /no access/);
