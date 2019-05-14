// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --enable-slow-asserts

var o1 = {
  a00:0, a01:0, a02:0, a03:0, a04:0, a05:0, a06:0, a07:0, a08:0, a09:0, a0a:0, a0b:0, a0c:0, a0d:0, a0e:0, a0f:0,
  a10:0, a11:0, a12:0, a13:0, a14:0, a15:0, a16:0, a17:0, a18:0, a19:0, a1a:0, a1b:0, a1c:0, a1d:0, a1e:0, a1f:0,

  dbl: 0.1,

  some_double: 2.13,
};

var o2 = {
  a00:0, a01:0, a02:0, a03:0, a04:0, a05:0, a06:0, a07:0, a08:0, a09:0, a0a:0, a0b:0, a0c:0, a0d:0, a0e:0, a0f:0,
  a10:0, a11:0, a12:0, a13:0, a14:0, a15:0, a16:0, a17:0, a18:0, a19:0, a1a:0, a1b:0, a1c:0, a1d:0, a1e:0, a1f:0,

  dbl: 0.1,

  boom: [],
};

o2.boom.push(42);
assertEquals(42, o2.boom[0]);
