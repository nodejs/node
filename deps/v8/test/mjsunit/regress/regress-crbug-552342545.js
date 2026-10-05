// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const proxy = new Proxy([], {
  get(t, p) {
    if (p === 'length') throw new Error('stopped_early');
    return Reflect.get(t, p);
  }
});

const o = [];
o.length = 2000000000;
o[0] = proxy;

assertThrows(() => {
  o.flat({
    valueOf() {
      o.length = 8;
      o.fill(proxy);
      return 1;
    }
  });
}, Error, 'stopped_early');
