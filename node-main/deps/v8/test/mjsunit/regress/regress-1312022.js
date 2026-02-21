// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function __getProperties(obj) {
  let properties = [];
  for (let name of Object.getOwnPropertyNames(obj)) {
    properties.push(name);
  }
  return properties;
}
function* __getObjects(root = this, level = 0) {
  if (level > 4) return;
  let obj_names = __getProperties(root);
  for (let obj_name of obj_names) {
    let obj = root[obj_name];
    yield* __getObjects(obj, level + 1);
  }
}
function __getRandomObject() {
  for (let obj of __getObjects()) {}
}
%PrepareFunctionForOptimization(__f_23);
%OptimizeFunctionOnNextCall(__f_23);
try {
  __getRandomObject(), {};
} catch (e) {}
function __f_23(__v_93) {
  var __v_95 = "x";
  return __v_93[__v_95] + __v_94[__v_95];
}
%PrepareFunctionForOptimization(__f_23);
try {
   __f_23();
} catch (e) {}
try {
  %OptimizeFunctionOnNextCall(__f_23);
  __f_23();
} catch (e) {}
%DisableOptimizationFinalization();
