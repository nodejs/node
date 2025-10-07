
// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --proto-assign-seq-opt
// Flags: --no-turbofan --no-maglev --no-sparkplug

function test_class_fast_path() {
  console.log("\n\ntest_class_is_slow_path");
  class test_class {
    constructor() { }
  }
  test_class.prototype.func = function () { return "test_class.prototype.func" };
  test_class.prototype.arrow_func = () => { return "test_class.prototype.arrow_func" };
  test_class.prototype.smi = 1
  test_class.prototype.str = "test_class.prototype.str"
  // TODO(rherouart): handle object and array literals
  // test_class.prototype.obj = {o:{smi:1, str:"str"}, smi:0};
  // test_class.prototype.arr = [0,1,2];

  test_instance = new test_class();
  assertEquals(test_instance.func(), "test_class.prototype.func");
  assertEquals(test_instance.arrow_func(), "test_class.prototype.arrow_func");
  assertEquals(test_instance.smi, 1);
  assertEquals(test_instance.str, "test_class.prototype.str");
  //   assertEquals(test_instance.obj.o.smi, 1);
  //   assertEquals(test_instance.obj.o.str, "str");
  //   assertEquals(test_instance.obj.smi, 0);
  //   assertEquals(test_instance.arr[0], 0);
  //   assertEquals(test_instance.arr[1], 1);
  //   assertEquals(test_instance.arr[2], 2);
}

function test_function_fast_paths() {
  console.log("\n\ntest_function_fast_paths");
  function test_function() { }


  test_function.prototype.func = function () { return "test_function.prototype.func" };
  test_function.prototype.arrow_func = () => { return "test_function.prototype.arrow_func" };
  test_function.prototype.smi = 1
  test_function.prototype.str = "test_function.prototype.str"

  test_instance = new test_function();
  assertEquals(test_instance.func(), "test_function.prototype.func");
  assertEquals(test_instance.arrow_func(), "test_function.prototype.arrow_func");
  assertEquals(test_instance.smi, 1);
  assertEquals(test_instance.str, "test_function.prototype.str");
}

function test_has_prototype_keys() {
  console.log("\n\ntest_has_prototype_keys");
  const test_object = {}
  Object.defineProperty(test_object, "prototype", {
    value: {}
  });

  console.log(JSON.stringify(test_object, null, 2));

  test_object.prototype.func = function () { return "test_object.prototype.func" };
  test_object.prototype.arrow_func = () => { return "test_object.prototype.arrow_func" };
  test_object.prototype.smi = 1
  test_object.prototype.str = "test_object.prototype.str"

  assertEquals(test_object.prototype.func(), "test_object.prototype.func");
  assertEquals(test_object.prototype.arrow_func(), "test_object.prototype.arrow_func");
  assertEquals(test_object.prototype.smi, 1);
  assertEquals(test_object.prototype.str, "test_object.prototype.str");
}

function test_arrow_function() {
  console.log("\n\ntest_arrow_function");
  var test_arrow_func = () => { };

  Object.defineProperty(test_arrow_func, "prototype", {
    value: {}
  });

  test_arrow_func.prototype.func = function () { return "test_function.prototype.func" };
  test_arrow_func.prototype.arrow_func = () => { return "test_function.prototype.arrow_func" };
  test_arrow_func.prototype.smi = 1
  test_arrow_func.prototype.str = "test_function.prototype.str"

  assertEquals(test_arrow_func.prototype.func(), "test_function.prototype.func");
  assertEquals(test_arrow_func.prototype.arrow_func(), "test_function.prototype.arrow_func");
  assertEquals(test_arrow_func.prototype.smi, 1);
  assertEquals(test_arrow_func.prototype.str, "test_function.prototype.str");
}

function test_has_setters() {
  console.log("\n\ntest_has_setters");

  function test_function() { };

  Object.defineProperty(test_function.prototype, "key", {
    set(x) {
      test_function.prototype = {}
    },
  });

  test_function.prototype.func = function () { return "test_function.prototype.func" };
  test_function.prototype.arrow_func = () => { return "test_function.prototype.arrow_func" };
  test_function.prototype.smi = 1
  test_function.prototype.str = "test_function.prototype.str"

  assertEquals(test_function.prototype.func(), "test_function.prototype.func");
  assertEquals(test_function.prototype.arrow_func(), "test_function.prototype.arrow_func");
  assertEquals(test_function.prototype.smi, 1);
  assertEquals(test_function.prototype.str, "test_function.prototype.str");

  var test_instance = new test_function();

  test_function.prototype.func = function () { return "test_function.prototype.func" };
  test_function.prototype.arrow_func = () => { return "test_function.prototype.arrow_func" };
  test_function.prototype.smi = 1
  test_function.prototype.str = "test_function.prototype.str"
  test_function.prototype.key = 1
  assertEquals(Object.keys(test_function.prototype).length, 0);

  var test_instance2 = new test_function();
  assertEquals(Object.keys(test_instance2.__proto__).length, 0);

  assertEquals(test_instance.func(), "test_function.prototype.func");
  assertEquals(test_instance.arrow_func(), "test_function.prototype.arrow_func");
  assertEquals(test_instance.smi, 1);
  assertEquals(test_instance.str, "test_function.prototype.str");
}


function test_prototype_proto_keys() {
  console.log("\n\ntest_prototype_proto_keys");

  function test_function() { };
  var das_proto = {}
  Object.defineProperty(das_proto, "smi", {
    set(x) {
      test_function.prototype.str = "foo"
    },
    get() {
      return 0;
    }
  });

  test_function.prototype.func = function () { return "test_function.prototype.func" };
  test_function.prototype.arrow_func = () => { return "test_function.prototype.arrow_func" };
  test_function.prototype.__proto__ = das_proto
  test_function.prototype.str = "test_function.prototype.str"
  test_function.prototype.smi = 1

  assertEquals(test_function.prototype.func(), "test_function.prototype.func");
  assertEquals(test_function.prototype.arrow_func(), "test_function.prototype.arrow_func");
  assertEquals(test_function.prototype.smi, 0);
  assertEquals(test_function.prototype.str, "foo");

  var test_instance = new test_function();
  assertEquals(test_instance.func(), "test_function.prototype.func");
  assertEquals(test_instance.arrow_func(), "test_function.prototype.arrow_func");
  assertEquals(test_instance.smi, 0);
  assertEquals(test_instance.str, "foo");
}

function test_feedback_vector_side_effect() {
  console.log("\n\ntest_feedback_vector_side_effect");

  function outer() {
    function Class() { }
    Class.prototype.key_1 = function inner() {
      function tagged_template_literal(x) {
        return x;
      }
      return tagged_template_literal`abc`;
    }
    Class.prototype.key_2 = 1;
    Class.prototype.key_3 = 1;
    return new Class;
  }


  let inner_1 = outer();
  let inner_2 = outer();

  assertEquals(inner_1.key_1(), inner_2.key_1());
}

function test_assign_key_multiple_times() {
  console.log("\n\ntest_assign_key_multiple_times");

  function test_function() { };

  test_function.prototype.smi = function () { return "test_function.prototype.func" };
  test_function.prototype.smi = () => { return "test_function.prototype.arrow_func" };
  test_function.prototype.smi = 1

  assertEquals(test_function.prototype.smi, 1);

  var x = new test_function();
  assertEquals(x.smi, 1);
}

function test_not_proto_assign_seq() {
  console.log("\n\ntest_not_proto_assign_seq")
  function test_function() { };
  test_function.prototype = []

  test_function.prototype["0"] = function () { return "test_function.prototype.func" };
  test_function.prototype["1"] = () => { return "test_function.prototype.arrow_func" };
  test_function.prototype["2"] = 1

  assertEquals(test_function.prototype["0"](), "test_function.prototype.func");
  assertEquals(test_function.prototype["1"](), "test_function.prototype.arrow_func");
  assertEquals(test_function.prototype["2"], 1);
}



function test_prototype_read_only() {
  console.log("\n\ntest_prototype_read_only");
  function test_function() { }

  Object.defineProperty(test_function.prototype, "key", { value: 0, writable: false })

  test_function.prototype.func = function () { return "test_function.prototype.func" };
  test_function.prototype.arrow_func = () => { return "test_function.prototype.arrow_func" };
  test_function.prototype.smi = 1
  test_function.prototype.str = "test_function.prototype.str"
  test_function.prototype.key = 1

  test_instance = new test_function();
  assertEquals(test_instance.func(), "test_function.prototype.func");
  assertEquals(test_instance.arrow_func(), "test_function.prototype.arrow_func");
  assertEquals(test_instance.smi, 1);
  assertEquals(test_instance.str, "test_function.prototype.str");
  assertEquals(test_instance.key, 0);
}

function test_eval_return_last_set_property() {
  console.log("\n\ntest_eval_return_last_set_property");

  let result = eval(`
    function Foo(){}
    Foo.prototype.k1 = 1;
    Foo.prototype.k2 = 2;
    Foo.prototype.k3 = 3;
  `)
  assertEquals(3, result);
}


function test_null_prototype() {
  console.log("\n\ntest_null_prototype");
  function test_function() { }

  test_function.prototype = null;
  try {
    test_function.prototype.func = function () { return "test_function.prototype.func" };
    test_function.prototype.arrow_func = () => { return "test_function.prototype.arrow_func" };
    test_function.prototype.smi = 1
    test_function.prototype.str = "test_function.prototype.str"
    test_function.prototype.key = 1

    test_instance = new test_function();
    assertEquals(test_instance.func(), "test_function.prototype.func");
    assertEquals(test_instance.arrow_func(), "test_function.prototype.arrow_func");
    assertEquals(test_instance.smi, 1);
    assertEquals(test_instance.str, "test_function.prototype.str");
    assertEquals(test_instance.key, 0);
  }
  catch {
    assertEquals(true,true);
    return;
  }
  assertEquals(false,true);
}

function test_variable_proxy() {
  console.log("\n\ntest_variable_proxy");
  var calls = 0;
  let foo = {
    get prototype() {
      console.log("inside");
      calls += 1;
      foo = {};
      return { prototype: {} };
    }
  };
  try {
    foo.prototype.k1 = 1;
    foo.prototype.k2 = 2;
  }
  catch {
    console.log("catch");
  }
  assertEquals(calls, 1);
  assertEquals(Object.keys(foo).length, 0);
}


function test_variable_proxy_eval() {
  console.log("\n\ntest_variable_proxy_eval");

  var foo = function () { };
  (function inner_test() {

    eval("var foo = { prototype: { set k1(x) { console.log('inside');calls += 1;foo = {}} }}");
    var calls = 0;
    try {
      foo.prototype.k1 = 1;
      foo.prototype.k2 = 2;
    }
    catch {
      console.log("foo");
    }
    assertEquals(calls, 1);
    assertEquals(Object.keys(foo).length, 0);
  })();
}

function test_different_left_most_var() {
  console.log("\n\ntest_different_left_most_var");
  // Note: This code should not generate a SetPrototypeProperties Instruction
  function foo() { }
  function bar() { }

  foo.prototype.k1 = 1;
  bar.prototype.k1 = 2;
  foo.prototype.k2 = 3;
  bar.prototype.k2 = 4;

  assertEquals(foo.prototype.k1, 1);
  assertEquals(bar.prototype.k1, 2);
  assertEquals(foo.prototype.k2, 3);
  assertEquals(bar.prototype.k2, 4);
}

test_null_prototype();
test_class_fast_path();
test_function_fast_paths();
test_has_prototype_keys();
test_arrow_function();
test_has_setters();
test_prototype_proto_keys();
test_feedback_vector_side_effect();
test_assign_key_multiple_times();
test_not_proto_assign_seq();
test_prototype_read_only();
test_eval_return_last_set_property();
test_variable_proxy_eval();
test_variable_proxy();
test_different_left_most_var();
