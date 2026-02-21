// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("base.js");

var str;
var re;

function SimpleMatch() {
  str.match(re);
}

function Match1Setup() {
  re = /[Cz]/;
  str = createHaystack();
}

function Match2Setup() {
  re = /[Cz]/g;
  str = createHaystack();
}

function Match3Setup() {
  re = /[cZ]/;  // Not found.
  str = createHaystack();
}

var benchmarks = [ [SimpleMatch, Match1Setup],
                   [SimpleMatch, Match2Setup],
                   [SimpleMatch, Match3Setup],
                 ];
