// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f1(a,) {}
function f2(a,b,) {}
function f3(a,b,c,) {}
assertEquals(1, f1.length);
assertEquals(2, f2.length);
assertEquals(3, f3.length);

function* g1(a,) {}
function* g2(a,b,) {}
function* g3(a,b,c,) {}
assertEquals(1, g1.length);
assertEquals(2, g2.length);
assertEquals(3, g3.length);

assertEquals(1, (function(a,) {}).length);
assertEquals(2, (function(a,b,) {}).length);
assertEquals(3, (function(a,b,c,) {}).length);

assertEquals(1, (function*(a,) {}).length);
assertEquals(2, (function*(a,b,) {}).length);
assertEquals(3, (function*(a,b,c,) {}).length);

assertEquals(1, ((a,) => {}).length);
assertEquals(2, ((a,b,) => {}).length);
assertEquals(3, ((a,b,c,) => {}).length);
