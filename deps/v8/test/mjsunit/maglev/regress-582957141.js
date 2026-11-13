// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function* gen() {}

let call_next = true;
function callNext(g) {
  if (!call_next) return;
  return g.next();
}
%PrepareFunctionForOptimization(callNext);
callNext(gen());
call_next = false;

function foo(x) {
  x.p5.length;
  callNext(x);
}
%PrepareFunctionForOptimization(foo);
foo({p0: 1, p1: 1, p2: 1, p3: 1, p4: 1, p5: 'a', a1: 1});
foo({p0: 1, p1: 1, p2: 1, p3: 1, p4: 1, p5: 'a', a2: 1});
foo({p0: 1, p1: 1, p2: 1, p3: 1, p4: 1, p5: 'a', a3: 1});
foo({p0: 1, p1: 1, p2: 1, p3: 1, p4: 1, p5: 'a', a4: 1});
foo({p0: 1, p1: 1, p2: 1, p3: 1, p4: 1, p5: 'a', a5: 1});
%OptimizeMaglevOnNextCall(foo);
foo({p0: 1, p1: 1, p2: 1, p3: 1, p4: 1, p5: 'a', a1: 1});
assertMaglevved(foo);
