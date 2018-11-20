// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Check that the receiver of Runtime_GetPropertyWithReceiver can be
// a plain JS value.

var values = [
  10,
  false,
  "test"
];

for (let val of values) {
  var proto = Object.getPrototypeOf(val);

  var proxy = new Proxy({}, {});
  Object.setPrototypeOf(proto, proxy);
}
