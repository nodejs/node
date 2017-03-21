// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Debug = debug.Debug;
var LiveEdit = Debug.LiveEdit;

unique_id = 0;

var AsyncFunction = (async function(){}).constructor;

function assertPromiseValue(value, promise) {
  promise.then(resolve => {
    went = true;
    if (resolve !== value) {
      print(`expected ${value} found ${resolve}`);
      quit(1);
    }
  }, reject => {
    print(`rejected ${reject}`);
    quit(1);
  });
}

function MakeAsyncFunction() {
  // Prevents eval script caching.
  unique_id++;
  return AsyncFunction('callback',
      "/* " + unique_id + "*/\n" +
      "await callback();\n" +
      "return 'Cat';\n");
}

function MakeFunction() {
  // Prevents eval script caching.
  unique_id++;
  return Function('callback',
      "/* " + unique_id + "*/\n" +
      "callback();\n" +
      "return 'Cat';\n");
}

// First, try MakeGenerator with no perturbations.
(function(){
  var asyncfn = MakeAsyncFunction();
  function callback() {};
  var promise = asyncfn(callback);
  assertPromiseValue('Cat', promise);
})();

function patch(fun, from, to) {
  function debug() {
    var log = new Array();
    var script = Debug.findScript(fun);
    var pos = script.source.indexOf(from);
    print(`pos ${pos}`);
    try {
      LiveEdit.TestApi.ApplySingleChunkPatch(script, pos, from.length, to,
                                             log);
    } finally {
      print("Change log: " + JSON.stringify(log) + "\n");
    }
  }
  %ExecuteInDebugContext(debug);
}

// Try to edit a MakeAsyncFunction while it's running, then again while it's
// stopped.
(function(){
  var asyncfn = MakeAsyncFunction();

  var patch_attempted = false;
  function attempt_patch() {
    assertFalse(patch_attempted);
    patch_attempted = true;
    assertThrows(function() { patch(asyncfn, "'Cat'", "'Capybara'") },
                 LiveEdit.Failure);
  };
  var promise = asyncfn(attempt_patch);
  // Patch should not succeed because there is a live async function activation
  // on the stack.
  assertPromiseValue("Cat", promise);
  assertTrue(patch_attempted);

  %RunMicrotasks();

  // At this point one iterator is live, but closed, so the patch will succeed.
  patch(asyncfn, "'Cat'", "'Capybara'");
  promise = asyncfn(function(){});
  // Patch successful.
  assertPromiseValue("Capybara", promise);

  // Patching will fail however when an async function is suspended.
  var resolve;
  promise = asyncfn(function(){return new Promise(function(r){resolve = r})});
  assertThrows(function() { patch(asyncfn, "'Capybara'", "'Tapir'") },
               LiveEdit.Failure);
  resolve();
  assertPromiseValue("Capybara", promise);

  // Try to patch functions with activations inside and outside async
  // function activations.  We should succeed in the former case, but not in the
  // latter.
  var fun_outside = MakeFunction();
  var fun_inside = MakeFunction();
  var fun_patch_attempted = false;
  var fun_patch_restarted = false;
  function attempt_fun_patches() {
    if (fun_patch_attempted) {
      assertFalse(fun_patch_restarted);
      fun_patch_restarted = true;
      return;
    }
    fun_patch_attempted = true;
    // Patching outside an async function activation must fail.
    assertThrows(function() { patch(fun_outside, "'Cat'", "'Cobra'") },
                 LiveEdit.Failure);
    // Patching inside an async function activation may succeed.
    patch(fun_inside, "'Cat'", "'Koala'");
  }
  promise = asyncfn(function() { return fun_inside(attempt_fun_patches) });
  assertEquals('Cat',
               fun_outside(function () {
                 assertPromiseValue('Capybara', promise);
                 assertTrue(fun_patch_restarted);
                 assertTrue(fun_inside.toString().includes("'Koala'"));
               }));
})();

%RunMicrotasks();
