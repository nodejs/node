// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap --expose-gc

(function test_keyed_load() {
  var smi_arr = [0];
  smi_arr.load = 42;

  var double_arr = [0.5];
  double_arr.load = 42;

  var obj_arr = [{}];
  obj_arr.load = 42;

  var arrs = [smi_arr, double_arr, obj_arr];

  var tmp;
  function do_keyed_load(arrs) {
    for (var i = 0; i < arrs.length; i++) {
      var arr = arrs[i];
      tmp = arr[0];
    }
  }

  var obj = {};
  obj.load_boom = smi_arr;

  do_keyed_load(arrs);
  do_keyed_load(arrs);
  %OptimizeFunctionOnNextCall(do_keyed_load);
  do_keyed_load(arrs);

  gc();
})();

(function test_keyed_store() {
  var smi_arr = [0];
  smi_arr.store = 42;

  var double_arr = [0.5];
  double_arr.store = 42;

  var obj_arr = [{}];
  obj_arr.store = 42;

  var arrs = [smi_arr, double_arr, obj_arr];

  function do_keyed_store(arrs) {
    for (var i = 0; i < arrs.length; i++) {
      var arr = arrs[i];
      arr[0] = 0;
    }
  }

  var obj = {};
  obj.store_boom = smi_arr;

  do_keyed_store(arrs);
  do_keyed_store(arrs);
  %OptimizeFunctionOnNextCall(do_keyed_store);
  do_keyed_store(arrs);

  gc();
})();
