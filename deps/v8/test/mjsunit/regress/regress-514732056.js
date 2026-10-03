// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

const r = Realm.create();
const options = new Proxy({}, {
  get: function(target, prop) {
    if (prop === 'type') {
      // Disposing the realm during ReadSource should trigger the
      // "Invalid realm index" check in Realm.eval.
      Realm.dispose(r);
      return 'string';
    }
  }
});

assertThrows(() => Realm.eval(r, '1+1', options), Error, 'Invalid realm index');
