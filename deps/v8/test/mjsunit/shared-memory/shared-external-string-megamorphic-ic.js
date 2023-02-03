// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-externalize-string --shared-string-table
// Flags: --allow-natives-syntax

function set(o, ext_key) {
  o[ext_key] = "bar";
}
function get(o, ext_key) {
  o[ext_key];
}

%PrepareFunctionForOptimization(set);
%OptimizeFunctionOnNextCall(set);
%PrepareFunctionForOptimization(get);
%OptimizeFunctionOnNextCall(get);

(function test() {
  let ext_key = "AAAAAAAAAAAAAAAAAAAAAA";
  externalizeString(ext_key);

  set({a:1}, ext_key);
  set({b:2}, ext_key);
  set({c:3}, ext_key);
  set({d:4}, ext_key);
  set({e:5}, ext_key);
  set({f:6}, ext_key);

  get({a:1}, ext_key);
  get({b:2}, ext_key);
  get({c:3}, ext_key);
  get({d:4}, ext_key);
  get({e:5}, ext_key);
  get({f:6}, ext_key);
})();
