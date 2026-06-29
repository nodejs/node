// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

d8.file.execute('test/mjsunit/mjsunit.js');

function assertLessThanOrEqual(a, b) {
  assertTrue(a <= b, `Expected ${a} <= ${b}`);
}

function createRealmAndAllocate(bytes) {
  let realm = Realm.createAllowCrossRealmAccess();
  Realm.eval(realm, `
    this.numbers = [.1];
    for (let i = 0; i < ${bytes} / 8; ++i) {
      this.numbers.push(0.1);
    }
  `);
}

if (this.performance && performance.measureMemory) {
  let number_of_realms = 3;
  let realms = [];
  let bytes_to_allocate = 1024 * 1024;
  for (let i = 0; i < number_of_realms; i++) {
    realms.push(createRealmAndAllocate(bytes_to_allocate));
  }
  assertPromiseResult((async () => {
    let result = await performance.measureMemory({detailed: true});
    print(JSON.stringify(result));
    assertEquals(number_of_realms, result.other.length);
    for (let other of result.other) {
      // TODO(ulan): compare against bytes_to_allocate once
      // we have more accurate native context inference.
      assertLessThanOrEqual(0, other.jsMemoryEstimate);
    }
  })());
  // Force a GC to complete memory measurement.
  gc();
}
