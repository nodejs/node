// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for hitting a DCHECK in StoreProxy.


for (var i = 0; i < 10; i++) {
  __proto__ = new Proxy({}, { getPrototypeOf() { } });
}
