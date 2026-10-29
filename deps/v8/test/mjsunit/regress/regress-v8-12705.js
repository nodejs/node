// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const r = Realm.create();
const otherPromise = Realm.eval(r, 'Promise.resolve()');

assertThrows(
  () => {
    Promise.prototype.then.call(otherPromise, () => { });
  }, TypeError, 'no access');
