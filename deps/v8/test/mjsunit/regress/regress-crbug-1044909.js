// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function main() {
  const v2 = Object.prototype;
  v2[4294967296] = {};
  const v12 = {get: function() {}};
  Object.defineProperty(v2, 4294967296, v12);
  const v15 = {...v2};
}
%PrepareFunctionForOptimization(main);
main();
