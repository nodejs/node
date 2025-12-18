// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function assert(cond) {
  if (!cond) throw 'Assert';
}

function Constructor() {
  this.padding1 = null;
  this.padding2 = null;
  this.padding3 = null;
  this.padding4 = null;
  this.padding5 = null;
  this.padding6 = null;
  this.padding7 = null;
  this.padding8 = null;
  this.padding9 = null;
  this.padding10 = null;
  this.padding11 = null;
  this.padding12 = null;
  this.padding13 = null;
  this.padding14 = null;
  this.padding15 = null;
  this.padding16 = null;
  this.padding17 = null;
  this.padding18 = null;
  this.padding19 = null;
  this.padding20 = null;
  this.padding21 = null;
  this.padding22 = null;
  this.padding23 = null;
  this.padding24 = null;
  this.padding25 = null;
  this.padding26 = null;
  this.padding27 = null;
  this.padding28 = null;
  this.padding29 = null;
  this.array = null;
  this.accumulator = 0;
}

function f(k) {
  var c = k.accumulator | 0;
  k.accumulator = k.array[k.accumulator + 1 | 0] | 0;
  k.array[c + 1 | 0] = -1;
  var head = k.accumulator;
  assert(head + c & 1);
  while (head >= 0) {
    head = k.array[head + 1 | 0];
  }
  return;
};
%PrepareFunctionForOptimization(f);
const tmp = new Constructor();
tmp.array = new Int32Array(5);
for (var i = 1; i < 5; i++) tmp.array[i] = i | 0;
tmp.accumulator = 0;

f(tmp);
f(tmp);
%OptimizeFunctionOnNextCall(f);
f(tmp);  // This must not trigger the {assert}.
