// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var accessor_to_data_case = (function() {
  var v = {};
  Object.defineProperty(v, "foo", { get: function() { return 42; }, configurable: true});

  var obj = {};
  obj["boom"] = v;

  Object.defineProperty(v, "foo", { value: 0, writable: true, configurable: true });
  return obj;
})();


var data_to_accessor_case = (function() {
  var v = {};
  Object.defineProperty(v, "bar", { value: 0, writable: true, configurable: true });

  var obj = {};
  obj["bam"] = v;

  Object.defineProperty(v, "bar", { get: function() { return 42; }, configurable: true});
  return obj;
})();
