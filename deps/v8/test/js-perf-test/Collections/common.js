// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var N = 10;
var LargeN = 1e4;
var keys;
var keyValuePairs;

function SetupKeyValuePairsFromKeys() {
  keyValuePairs = keys.map((v) => [v, v]);
}

function SetupSmiKeys(count = 2 * N) {
  keys = Array.from({ length : count }, (v, i) => i);
}

function SetupSmiKeyValuePairs(count = 2 * N) {
  SetupSmiKeys(count);
  SetupKeyValuePairsFromKeys();
}

function SetupStringKeys(count = 2 * N) {
  keys = Array.from({ length : count }, (v, i) => 's' + i);
}

function SetupStringKeyValuePairs(count = 2 * N) {
  SetupStringKeys(count);
  SetupKeyValuePairsFromKeys();
}

function SetupObjectKeys(count = 2 * N) {
  keys = Array.from({ length : count }, (v, i) => ({}));
}

function SetupObjectKeyValuePairs(count = 2 * N) {
  SetupObjectKeys(count);
  SetupKeyValuePairsFromKeys();
}

function SetupDoubleKeys(count = 2 * N) {
  keys = Array.from({ length : count }, (v, i) => i + 0.234);
}

function SetupDoubleKeyValuePairs(count = 2 * N) {
  SetupDoubleKeys(count);
  SetupKeyValuePairsFromKeys();
}
