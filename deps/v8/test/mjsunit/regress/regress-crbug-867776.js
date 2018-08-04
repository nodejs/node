// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

for (var i = 0; i < 3; i++) {
  var array = new BigInt64Array(200);

  function evil_callback() {
    %ArrayBufferNeuter(array.buffer);
    gc();
    return 1094795585n;
  }

  var evil_object = {valueOf: evil_callback};
  var root;
  try {
    root = BigInt64Array.of.call(function() { return array }, evil_object);
  } catch(e) {}
  gc();
}
