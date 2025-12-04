// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

proto = Object.getPrototypeOf(0);

function foo(v) {
  properties = Object.getOwnPropertyNames(proto);
  if (properties.includes("constructor") &&
      v.constructor.hasOwnProperty()) {
  }
}

function bar(n) {
  if (n > 5000) return;
  foo(0) ;
  bar(n+1);
}
bar(1);
