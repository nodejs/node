// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let p = Promise.resolve();
Object.defineProperty(p, 'then', {
  get: () => new Proxy(function() {}, p)
});

new Promise((r) => r(p));
