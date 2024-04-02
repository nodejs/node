// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function use(exports) {
  const result = Object.create(null);
  exports.forEach(x => result[x] = globalThis[x]);
  return result;
}

function takeAndUseWebSnapshot(createObjects, exports) {
  // Take a snapshot in Realm r1.
  const r1 = Realm.create();
  Realm.eval(r1, createObjects, { type: 'function' });
  const snapshot = Realm.takeWebSnapshot(r1, exports);
  // Use the snapshot in Realm r2.
  const r2 = Realm.create();
  const success = Realm.useWebSnapshot(r2, snapshot);
  assertTrue(success);
  const result =
      Realm.eval(r2, use, { type: 'function', arguments: [exports] });
  %HeapObjectVerify(result);
  return result;
}
