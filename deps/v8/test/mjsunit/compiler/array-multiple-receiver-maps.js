// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt
// Flags: --no-stress-background-compile

let id = 0;

function runTest(f, message, mkICTraining, deoptArg) {
  function test(f, message, ictraining, deoptArg) {
    // Train the call ic to the maps.
    let t = ictraining;

    // We put the training data into local variables
    // to ensure their maps are kepts alive. If the
    // maps die, gc *may* deoptimize {f}, which makes
    // the test flaky.
    let t1 = t();
    let t2 = t();
    let t3 = t();

    for (let a of t1) {
      f(a.arr, () => a.el);
    }
    for (let a of t2) {
      f(a.arr, () => a.el);
    }
    %OptimizeFunctionOnNextCall(f);
    message += " trained with" + JSON.stringify(t());
    if (deoptArg == undefined) {
      // Make sure the optimized function can handle
      // all trained maps without deopt.
      for (let a of t3) {
        message += " for args " + JSON.stringify(a) + " should have been optimized";
        f(a.arr, () => a.el);
        assertOptimized(f, undefined, message);
      }
    } else {
      // Trigger deopt, causing no-speculation bit to be set.
      let a1 = deoptArg;
      let a2 = deoptArg;
      message += " for args " + JSON.stringify(a1);
      message_unoptimized = message + " should have been unoptimized"
      message_optimized = message + " should have been unoptimized"
      f(a1.arr, () => a1.el);
      assertUnoptimized(f, undefined, message_unoptimized);
      %OptimizeFunctionOnNextCall(f);
      // No speculation should protect against further deopts.
      f(a2.arr, () => a2.el);
      assertOptimized(f, undefined,  message_optimized);
    }
  }

  // Get function as a string.
  var testString = test.toString();
  // Remove the function header..
  testString = testString.replace(new RegExp("[^\n]*"), "let f = " + f.toString() + ";");
  // ..and trailing '}'.
  testString = testString.replace(new RegExp("[^\n]*$"), "");
  // Substitute parameters.
  testString = testString.replace(new RegExp("ictraining", 'g'), mkICTraining.toString());
  testString = testString.replace(new RegExp("deoptArg", 'g'),
    deoptArg ? JSON.stringify(deoptArg).replace(/"/g,'') : "undefined");

  // Make field names unique to avoid learning of types.
  id = id + 1;
  testString = testString.replace(/[.]el/g, '.el' + id);
  testString = testString.replace(/el:/g, 'el' + id + ':');
  testString = testString.replace(/[.]arr/g, '.arr' + id);
  testString = testString.replace(/arr:/g, 'arr' + id + ':');

  var modTest = new Function("message", testString);
  //print(modTest);
  modTest(message);
}

let checks = {
  smiReceiver:
    { mkTrainingArguments : () => [{arr:[1], el:3}],
      deoptingArguments   : [{arr:[0.1], el:1}, {arr:[{}], el:1}]
    },
  objectReceiver:
    { mkTrainingArguments : () => [{arr:[{}], el:0.1}],
      deoptingArguments : []
    },
  multipleSmiReceivers:
    { mkTrainingArguments : () => { let b = [1]; b.x=3; return [{arr:[1], el:3}, {arr:b, el:3}] },
      deoptingArguments : [{arr:[0.1], el:1}, {arr:[{}], el:1}]
    },
  multipleSmiReceiversPackedUnpacked:
    { mkTrainingArguments : () => { let b = [1]; b[100] = 3; return [{arr:[1], el:3}, {arr:b, el:3}] },
      deoptingArguments : [{arr:[0.1], el:1}, {arr:[{}], el:1}]
    },
  multipleDoubleReceivers:
    { mkTrainingArguments : () => { let b = [0.1]; b.x=0.3; return [{arr:[0.1], el:0.3}, {arr:b, el:0.3}] },
      deoptingArguments : [{arr:[{}], el:true}, {arr:[1], el:true}]
    },
  multipleDoubleReceiversPackedUnpacked:
    { mkTrainingArguments : () => { let b = [0.1]; b[100] = 0.3; return [{arr:[0.1], el:0.3}, {arr:b, el:0.3}] },
      deoptingArguments : [{arr:[{}], el:true}, {arr:[1], el:true}]
    },
  multipleMixedReceivers:
    { mkTrainingArguments : () => { let b = [0.1]; b.x=0.3; return [{arr:[1], el:0.3}, {arr:[{}], el:true}, {arr:b, el:0.3}] },
      deoptingArguments : []
    },
  multipleMixedReceiversPackedUnpacked:
    { mkTrainingArguments : () => { let b = [0.1]; b[100] = 0.3; return [{arr:[1], el:0.3}, {arr:[{}], el:true}, {arr:b, el:0.3}] },
      deoptingArguments : []
    },
};

const functions = {
  push_reliable: (a,g) => { let b = g(); return a.push(2, b); },
  push_unreliable: (a,g) => { return a.push(2, g()); },
  pop_reliable: (a,g) => { let b = g(); return a.pop(2, b); },
  pop_unreliable: (a,g) => { return a.pop(2, g()); },
  shift_reliable: (a,g) => { let b = g(); return a.shift(2, b); },
  shift_unreliable: (a,g) => { return a.shift(2, g()); }
}

Object.keys(checks).forEach(
  key => {
    let check = checks[key];

    for (fnc in functions) {
      runTest(functions[fnc], "test-" + fnc + "-" + key, check.mkTrainingArguments);
      // Test each deopting arg separately.
      for (let deoptArg of check.deoptingArguments) {
        runTest(functions[fnc], "testDeopt-" + fnc + "-" + key, check.mkTrainingArguments, deoptArg);
      }
    }
  }
);
