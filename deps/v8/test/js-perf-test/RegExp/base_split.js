// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("base.js");

var str;
var re;

function SimpleSplit() {
  str.split(re);
}

function Split1Setup() {
  re = /[Cz]/;
  str = createHaystack();
}

function Split2Setup() {
  re = /[Cz]/u;
  str = createHaystack();
}

function Split3Setup() {
  re = /[Cz]/y;
  str = createHaystack();
}

function Split4Setup() {
  re = /[Cz]/uy;
  str = createHaystack();
}

function Split5Setup() {
  re = /[Cz]/;
  str = "";
}

function Split6Setup() {
  re = /[cZ]/;  // No match.
  str = createHaystack();
}

function Split7Setup() {
  re = /[A-Za-z\u0080-\u00FF ]/;
  str = "hipopótamo maçã pólen ñ poção água língüa";
}

var benchmarks = [ [SimpleSplit, Split1Setup],
                   [SimpleSplit, Split2Setup],
                   [SimpleSplit, Split3Setup],
                   [SimpleSplit, Split4Setup],
                   [SimpleSplit, Split5Setup],
                   [SimpleSplit, Split6Setup],
                   [SimpleSplit, Split7Setup],
                 ];
