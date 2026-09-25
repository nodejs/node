// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var gaga = 42;

function* foo(x, b) {
 if (b) return;
 x.p;
 while (true) {
   gaga;
   yield;
 }
}
%PrepareFunctionForOptimization(foo);
foo({p:42}, true);
foo({p:42}, true);
%OptimizeFunctionOnNextCall(foo);
const g = foo({p:42}, false);
