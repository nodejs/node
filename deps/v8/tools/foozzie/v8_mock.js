// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is intended for permanent JS behavior changes for mocking out
// non-deterministic behavior. For temporary suppressions, please refer to
// v8_suppressions.js.
// This file is loaded before each correctness test cases and won't get
// minimized.


// This will be overridden in the test cases. The override can be minimized.
var __PrettyPrint = function __PrettyPrint(msg) { print(msg); };


// All calls to f.arguments are replaced by f.mock_arguments by an external
// script.
Object.prototype.mock_arguments = ['x', 'y']


// Mock Math.random.
var __magic_index_for_mocked_random = 0
Math.random = function(){
  __magic_index_for_mocked_random = (__magic_index_for_mocked_random + 1) % 10
  return __magic_index_for_mocked_random / 10.0;
}


// Mock Date.
var __magic_index_for_mocked_date = 0
var __magic_mocked_date = 1477662728696
__magic_mocked_date_now = function(){
  __magic_index_for_mocked_date = (__magic_index_for_mocked_date + 1) % 10
  __magic_mocked_date = __magic_mocked_date + __magic_index_for_mocked_date + 1
  return __magic_mocked_date
}

var __original_date = Date;
__magic_mock_date_handler = {
  construct: function(target, args, newTarget) {
    if (args.length > 0) {
      return new (Function.prototype.bind.apply(__original_date, [null].concat(args)));
    } else {
      return new __original_date(__magic_mocked_date_now());
    }
  },
  get: function(target, property, receiver) {
    if (property == "now") {
      return __magic_mocked_date_now;
    }
  },
}

Date = new Proxy(Date, __magic_mock_date_handler);

// Mock Worker.
var __magic_index_for_mocked_worker = 0
// TODO(machenbach): Randomize this for each test case, but keep stable during
// comparison. Also data and random above.
var __magic_mocked_worker_messages = [
  undefined, 0, -1, "", "foo", 42, [], {}, [0], {"x": 0}
]
Worker = function(code){
  try {
    __PrettyPrint(eval(code));
  } catch(e) {
    __PrettyPrint(e);
  }
  this.getMessage = function(){
    __magic_index_for_mocked_worker = (__magic_index_for_mocked_worker + 1) % 10
    return __magic_mocked_worker_messages[__magic_index_for_mocked_worker];
  }
  this.postMessage = function(msg){
    __PrettyPrint(msg);
  }
}
