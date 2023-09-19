// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var proxy = new Proxy({}, {});

Object.assign(proxy, { b: "boom", a: "ah", o: "ouch" });
assertEquals(["b", "a", "o"], Object.getOwnPropertyNames(proxy));
assertEquals("boom", proxy.b);
assertEquals("ah", proxy.a);
assertEquals("ouch", proxy.o);
