// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-inline-new

function keyed_store(obj, key, value) {
  obj[key] = value;
}

function foo() {
  obj = {};
  obj.smi = 1;
  obj.dbl = 1.5;
  obj.obj = {a:1};

  // Transition keyed store IC to polymorphic.
  keyed_store(obj, "smi", 100);
  keyed_store(obj, "dbl", 100);
  keyed_store(obj, "obj", 100);

  // Now call with a FAST_SMI_ELEMENTS object.
  var smi_array = [5, 1, 1];
  keyed_store(smi_array, 1, 6);
  // Transition from FAST_SMI_ELEMENTS to FAST_DOUBLE_ELEMENTS.
  keyed_store(smi_array, 2, 1.2);
}

foo();
