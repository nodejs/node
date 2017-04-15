// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("base.js");

var str;
var re;

function SimpleSearch() {
  str.search(re);
}

function Search1Setup() {
  re = /[Cz]/;
  str = createHaystack();
}

function Search2Setup() {
  re = /[Cz]/;
  re.lastIndex = 42;  // Force lastIndex restoration.
  str = createHaystack();
}

function Search3Setup() {
  re = /[cZ]/;  // Not found.
  str = createHaystack();
}

var benchmarks = [ [SimpleSearch, Search1Setup],
                   [SimpleSearch, Search2Setup],
                   [SimpleSearch, Search3Setup],
                 ];
