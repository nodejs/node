// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test the reindexer visiting classes, avoiding repeat visits of the same
// function.
//
// For each test, create function literals inside a class, where the functions
// have to be reindexed due to the whole thing being inside an arrow head scope.

((arg = (function wrapper() {
  // Class with field that has computed property name with a function in the
  // computation.
  class g {
    [{b: function in_computed_field_name() {}}]
  }
})) => {})();

((arg = (function wrapper() {
  // Class with initialized field that has computed property name with a
  // function in the computation.
  class g {
    [{b: function in_computed_field_name_with_init() {}}] = ""
  }
})) => {})();

((arg = (function wrapper() {
  // Class with initialized field that has literal property name with a function
  // in the initializer value.
  class g {
    b = (function in_init_value_of_field(){})()
  }
})) => {})();

((arg = (function wrapper() {
  // Class with initialized field that has private property name with a function
  // in the initializer value.
  class g {
    #b = (function in_init_value_of_private_field(){})()
  }
})) => {})();

((arg = (function wrapper() {
  // Class with initialized field that has computed property name with a
  // function in the initializer value.
  class g {
    ["b"] = (function in_init_value_of_computed_field_name(){})()
  }
})) => {})();

((arg = (function wrapper() {
  // Class with method that has computed property name with a function in the
  // computation.
  class g {
    [{b: function in_computed_method_name() {}}] () {}
  }
})) => {})();

((arg = (function wrapper() {
  // Class with method that has an argument with a default function init.
  class g {
    b(arg = function in_method_arg_default_init() {}) {}
  }
})) => {})();

((arg = (function wrapper() {
  // Class with method that has a computed property name and an argument with a
  // default function init.
  class g {
    ["b"] (arg = function in_computed_method_arg_default_init() {}) {}
  }
})) => {})();
