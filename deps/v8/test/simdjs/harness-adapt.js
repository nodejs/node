// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

"use strict";

var _oldLoad = load;

// Filter load paths in the ecmascript_simd tests that
// assume the test is run with a current working directory
// set to the directory containing the test.
load = function(filename) {
  // Decide if this is the compliance test or the benchmarks.
  if (filename === 'ecmascript_simd.js' ||
      filename === 'ecmascript_simd_tests.js') {
    _oldLoad('test/simdjs/data/src/' + filename);
  } else {
    _oldLoad('test/simdjs/data/src/benchmarks/' + filename);
  }
};

// TODO(bbudge): Drop when polyfill is not needed.
load('ecmascript_simd.js');

load('base.js');

})();
