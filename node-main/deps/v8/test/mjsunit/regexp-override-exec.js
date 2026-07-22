// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var s = "baa";

assertEquals(1, s.search(/a/));
assertEquals(["aa"], s.match(/a./));
assertEquals(["b", "", ""], s.split(/a/));

let o = { index : 3, 0 : "x" };

RegExp.prototype.exec = () => { return o; }
assertEquals(3, s.search(/a/));
assertEquals(o, s.match(/a./));
assertEquals("baar", s.replace(/a./, "r"));

RegExp.prototype.exec = () => { return null; }
assertEquals(["baa"], s.split(/a/));
