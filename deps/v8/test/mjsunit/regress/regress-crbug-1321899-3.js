// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/regress/regress-crbug-1321899.js');

// Attached global should have access
const realm = Realm.createAllowCrossRealmAccess();
const globalProxy = Realm.global(realm);

assertThrows(() => B.setField(globalProxy), TypeError, /Cannot write private member #b to an object whose class did not declare it/);
assertThrows(() => B.getField(globalProxy), TypeError, /Cannot read private member #b from an object whose class did not declare it/);

new B(globalProxy);
assertEquals(B.getField(globalProxy), 1);
B.setField(globalProxy);
assertEquals(B.getField(globalProxy), 'b');  // Fast case
B.setField(globalProxy);  // Fast case
assertEquals(B.getField(globalProxy), 'b');  // Fast case
assertThrows(() => new B(globalProxy), TypeError, /Cannot initialize #b twice on the same object/);

assertThrows(() => C.setField(globalProxy), TypeError, /Cannot write private member #c to an object whose class did not declare it/);
assertThrows(() => C.getField(globalProxy), TypeError, /Cannot read private member #c from an object whose class did not declare it/);

new C(globalProxy);
assertEquals(C.getField(globalProxy), undefined);
C.setField(globalProxy);
assertEquals(C.getField(globalProxy), 'c');  // Fast case
C.setField(globalProxy);  // Fast case
assertEquals(C.getField(globalProxy), 'c');  // Fast case
assertThrows(() => new C(globalProxy), TypeError, /Cannot initialize #c twice on the same object/);

assertThrows(() => D.setAccessor(globalProxy), TypeError, /Receiver must be an instance of class D/);
assertThrows(() => D.getAccessor(globalProxy), TypeError, /Receiver must be an instance of class D/);

new D(globalProxy);
assertEquals(D.getAccessor(globalProxy), 0);
D.setAccessor(globalProxy);
assertEquals(D.getAccessor(globalProxy), 'd');  // Fast case
D.setAccessor(globalProxy);  // Fast case
assertEquals(D.getAccessor(globalProxy), 'd');  // Fast case
assertThrows(() => new D(globalProxy), TypeError, /Cannot initialize private methods of class D twice on the same object/);

assertThrows(() => E.setMethod(globalProxy), TypeError, /Receiver must be an instance of class E/);
assertThrows(() => E.getMethod(globalProxy), TypeError, /Receiver must be an instance of class E/);

new E(globalProxy);
assertEquals(E.getMethod(globalProxy)(), 0);
assertThrows(() => E.setMethod(globalProxy), TypeError, /Private method '#e' is not writable/);
assertEquals(E.getMethod(globalProxy)(), 0);  // Fast case
assertThrows(() => new E(globalProxy), TypeError, /Cannot initialize private methods of class E twice on the same object/);

// Access should fail after detaching
Realm.detachGlobal(realm);

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
