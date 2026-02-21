// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var p01;
var p02;
var p03;
var p04;
var p05;
var p06;
var p07;
var p08;
var p09;
var p10;
var p11;
var p12;
var p13;
var p14;
var p15;
var p16;
var p17;
var p18;
var p19;
var p20;
var p21;
var p22;
var p23;
var p24;
var p25;
var p26;
var p27;
var p28;
var p29;
var p30;
var p31;
var p32;
var p33;
var p34;
var p35;
var p36;
var p37;
var p38;
var p39;
var p40;
var p41;
var p42;
var p43;
var p44;

p = { get b() {} };
for (x in p) {}
p = this;

function foo() {
  p.bla = p[42];
  p.__defineGetter__('bla', function() {});
}
foo();
try { var q = {}(); } catch(_) {}
