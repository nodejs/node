// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('../base.js');

const BENCHMARK_NAME = arguments[0];
const TEST_TYPE = arguments[1];
const optimize_param = arguments[2];
let optimize;
if (optimize_param == "opt") {
  optimize = true;
} else if (optimize_param == "noopt"){
  optimize = false;
} else {
  throw new Error("Test configuration error");
}

const DETERMINISTIC_RUNS = 1;
const LOCAL_ITERATIONS = 10000;
new BenchmarkSuite(BENCHMARK_NAME, [1000], [
  new Benchmark(BENCHMARK_NAME, false, false, DETERMINISTIC_RUNS, runBenchmark)
]);

// Classes for monomorphic super property access.
class A { };
A.prototype.super_prop_a0 = 10;
A.prototype.super_prop_a1 = 10;
A.prototype.super_prop_a2 = 10;
A.prototype.super_prop_a3 = 10;
A.prototype.super_prop_a4 = 10;
A.prototype.super_prop_a5 = 10;
A.prototype.super_prop_a6 = 10;
A.prototype.super_prop_a7 = 10;
A.prototype.super_prop_a8 = 10;
A.prototype.super_prop_a9 = 10;

class B extends A { }
B.prototype.super_prop_b0 = 10;
B.prototype.super_prop_b1 = 10;
B.prototype.super_prop_b2 = 10;
B.prototype.super_prop_b3 = 10;
B.prototype.super_prop_b4 = 10;
B.prototype.super_prop_b5 = 10;
B.prototype.super_prop_b6 = 10;
B.prototype.super_prop_b7 = 10;
B.prototype.super_prop_b8 = 10;
B.prototype.super_prop_b9 = 10;

class C extends B {
  test_super_a(unused) {
    // Do many property accesses to increase the time taken by the thing we want
    // to measure, in comparison to the overhead of calling this function.
    return (super.super_prop_a0 + super.super_prop_a1 + super.super_prop_a2 +
      super.super_prop_a3 + super.super_prop_a4 + super.super_prop_a5 +
      super.super_prop_a6 + super.super_prop_a7 + super.super_prop_a8 +
      super.super_prop_a9);
  }
  test_super_b(unused) {
    return (super.super_prop_b0 + super.super_prop_b1 + super.super_prop_b2 +
        super.super_prop_b3 + super.super_prop_b4 + super.super_prop_b5 +
        super.super_prop_b6 + super.super_prop_b7 + super.super_prop_b8 +
        super.super_prop_b9);
  }
};

// Classes for megamorphic super property access.
function createClasses(base) {
  class B extends base { };
  B.prototype.super_prop_b0 = 10;
  B.prototype.super_prop_b1 = 10;
  B.prototype.super_prop_b2 = 10;
  B.prototype.super_prop_b3 = 10;
  B.prototype.super_prop_b4 = 10;
  B.prototype.super_prop_b5 = 10;
  B.prototype.super_prop_b6 = 10;
  B.prototype.super_prop_b7 = 10;
  B.prototype.super_prop_b8 = 10;
  B.prototype.super_prop_b9 = 10;

  class C extends B {
    test_super_a(unused) {
      return (super.super_prop_a0 + super.super_prop_a1 + super.super_prop_a2 +
        super.super_prop_a3 + super.super_prop_a4 + super.super_prop_a5 +
        super.super_prop_a6 + super.super_prop_a7 + super.super_prop_a8 +
        super.super_prop_a9);
    }
    test_super_b(unused) {
      return (super.super_prop_b0 + super.super_prop_b1 + super.super_prop_b2 +
        super.super_prop_b3 + super.super_prop_b4 + super.super_prop_b5 +
        super.super_prop_b6 + super.super_prop_b7 + super.super_prop_b8 +
        super.super_prop_b9);
    }
  }
  return C;
}

function test_property_access(o) {
  return (o.prop0 + o.prop1 + o.prop2 + o.prop3 + o.prop4 + o.prop5 + o.prop6 +
     o.prop7 + o.prop8 + o.prop9);
}

// Set up "objects" and "tested_functions" based on which test we're running.
let objects;
let tested_functions;

switch (TEST_TYPE) {
  case "super_1":
    // Monomorphic super property lookup, property is a constant, found 1 step
    // above the lookup start object in the prototype chain.
    objects = [new C()];
    tested_functions = [C.prototype.test_super_a];
    break;
  case "normal_1":
    // Monomorphic normal property lookup, property is a constant, found 1 step
    // above the lookup start object in the prototype chain.
    objects = [{__proto__: {"prop0": 10, "prop1": 10, "prop2": 10, "prop3": 10,
                            "prop4": 10, "prop5": 10, "prop6": 10, "prop7": 10,
                            "prop8": 10, "prop9": 10}}];
    tested_functions = [test_property_access];
    break;
  case "super_2":
    // Monomorphic super property lookup, property is a constant, found in the
    // lookup start object.
    objects = [new C()];
    tested_functions = [C.prototype.test_super_b];
    break;
  case "normal_2":
    // Monomorphic normal property lookup, property is a constant, found in the
    // lookup start object.
    objects = [{"prop0": 10, "prop1": 10, "prop2": 10, "prop3": 10, "prop4": 10,
                "prop5": 10, "prop6": 10, "prop7": 10, "prop8": 10,
                "prop9": 10}];
    tested_functions = [test_property_access];
    break;
  case "super_3":
    // Megamorphic super property lookup, property is a constant, found 1 step
    // above the lookup start object in the prototype chain. The holder is
    // the same in all cases though.
    objects = [];
    tested_functions = [];
    for (let i = 0; i < 5; ++i) {
      const c = createClasses(A);
      objects.push(new c());
      tested_functions.push(c.prototype.test_super_a);
    }
    break;
  case "normal_3":
    // Megamorphic normal property lookup, property is a constant, found 1 step
    // above the lookup start object in the prototype chain. The holder is
    // the same in all cases though.
    const proto = {"prop0": 10, "prop1": 10, "prop2": 10, "prop3": 10,
                   "prop4": 10, "prop5": 10, "prop6": 10, "prop7": 10,
                   "prop8": 10, "prop9": 10};
    objects = [{__proto__: proto}, {__proto__: proto, "a": 1},
               {__proto__: proto, "a": 1, "b": 1},
               {__proto__: proto, "a": 1, "b": 1, "c": 1},
               {__proto__: proto, "a": 1, "b": 1, "c": 1, "d": 1}];
    tested_functions = [test_property_access, test_property_access,
                        test_property_access, test_property_access,
                        test_property_access];
    break;
  case "super_4":
    // Megamorphic super property lookup, property is a constant, found in the
    // lookup start object. The holder is always a different object.
    objects = [];
    tested_functions = [];
    for (let i = 0; i < 5; ++i) {
      const c = createClasses(A);
      objects.push(new c());
      tested_functions.push(c.prototype.test_super_b);
    }
    break;
  case "normal_4":
    // Megamorphic normal property lookup, property is a constant, found in the
    // lookup start object. The holder is always a different object.
    objects = [{"prop0": 10, "prop1": 10, "prop2": 10, "prop3": 10, "prop4": 10,
                "prop5": 10, "prop6": 10, "prop7": 10, "prop8": 10,
                "prop9": 10},
               {"a": 0, "prop0": 10, "prop1": 10, "prop2": 10, "prop3": 10,
                "prop4": 10, "prop5": 10, "prop6": 10, "prop7": 10, "prop8": 10,
                "prop9": 10},
               {"a": 0, "b": 0, "prop0": 10, "prop1": 10, "prop2": 10,
                "prop3": 10, "prop4": 10, "prop5": 10, "prop6": 10, "prop7": 10,
                "prop8": 10, "prop9": 10},
               {"a": 0, "b": 0, "c": 0, "prop0": 10, "prop1": 10, "prop2": 10,
                "prop3": 10, "prop4": 10, "prop5": 10, "prop6": 10, "prop7": 10,
                "prop8": 10, "prop9": 10},
               {"a": 0, "b": 0, "c": 0, "d": 0, "prop0": 10, "prop1": 10,
                "prop2": 10, "prop3": 10, "prop4": 10, "prop5": 10, "prop6": 10,
                "prop7": 10, "prop8": 10, "prop9": 10}];

    tested_functions = [test_property_access, test_property_access,
                        test_property_access, test_property_access,
                        test_property_access];
    break;
  default:
    throw new Error("Test configuration error");
}

for (f of tested_functions) {
  if (optimize) {
    %PrepareFunctionForOptimization(f);
  } else {
    %NeverOptimizeFunction(f);
  }
}

function runBenchmark() {
  const expected_value = 10 * 10;
  let ix = 0;
  for (let i = 0; i < LOCAL_ITERATIONS; ++i) {
    const object = objects[ix];
    const r = tested_functions[ix].call(object, object);
    if (r != expected_value) {
      throw new Error("Test error");
    }
    if (++ix == objects.length) {
      ix = 0;
      if (optimize) {
        for (f of tested_functions) {
          %OptimizeFunctionOnNextCall(f);
        }
      }
    }
  }
}

var success = true;

function PrintResult(name, result) {
  print(name + '(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}

BenchmarkSuite.config.doWarmup = false;
BenchmarkSuite.config.doDeterministic = true;

BenchmarkSuite.RunSuites({NotifyResult: PrintResult, NotifyError: PrintError});
