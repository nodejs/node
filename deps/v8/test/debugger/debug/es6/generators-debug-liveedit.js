// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var Debug = debug.Debug;
var LiveEdit = Debug.LiveEdit;

unique_id = 0;

var Generator = (function*(){}).constructor;

function assertIteratorResult(value, done, result) {
  assertEquals({value: value, done: done}, result);
}

function MakeGenerator() {
  // Prevents eval script caching.
  unique_id++;
  return Generator('callback',
      "/* " + unique_id + "*/\n" +
      "yield callback();\n" +
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
  var generator = MakeGenerator();
  function callback() {};
  var iter = generator(callback);
  assertIteratorResult(undefined, false, iter.next());
  assertIteratorResult("Cat", true, iter.next());
})();

function ExecuteInDebugContext(f) {
  var result;
  var exception = null;
  Debug.setListener(function(event) {
    if (event == Debug.DebugEvent.Break) {
      try {
        result = f();
      } catch (e) {
        // Rethrow this exception later.
        exception = e;
      }
    }
  });
  debugger;
  Debug.setListener(null);
  if (exception !== null) throw exception;
  return result;
}

function patch(fun, from, to) {
  function debug() {
    var log = new Array();
    var script = Debug.findScript(fun);
    var pos = script.source.indexOf(from);
    try {
      LiveEdit.TestApi.ApplySingleChunkPatch(script, pos, from.length, to,
                                             log);
    } finally {
      print("Change log: " + JSON.stringify(log) + "\n");
    }
  }
  ExecuteInDebugContext(debug);
}

// Try to edit a MakeGenerator while it's running, then again while it's
// stopped.
(function(){
  var generator = MakeGenerator();

  var gen_patch_attempted = false;
  function attempt_gen_patch() {
    assertFalse(gen_patch_attempted);
    gen_patch_attempted = true;
    assertThrows(function() { patch(generator, "'Cat'", "'Capybara'") },
                 LiveEdit.Failure);
  };
  var iter = generator(attempt_gen_patch);
  assertIteratorResult(undefined, false, iter.next());
  // Patch should not succeed because there is a live generator activation on
  // the stack.
  assertIteratorResult("Cat", true, iter.next());
  assertTrue(gen_patch_attempted);

  // At this point one iterator is live, but closed, so the patch will succeed.
  patch(generator, "'Cat'", "'Capybara'");
  iter = generator(function(){});
  assertIteratorResult(undefined, false, iter.next());
  // Patch successful.
  assertIteratorResult("Capybara", true, iter.next());

  // Patching will fail however when a live iterator is suspended.
  iter = generator(function(){});
  assertIteratorResult(undefined, false, iter.next());
  assertThrows(function() { patch(generator, "'Capybara'", "'Tapir'") },
               LiveEdit.Failure);
  assertIteratorResult("Capybara", true, iter.next());

  // Try to patch functions with activations inside and outside generator
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
    // Patching outside a generator activation must fail.
    assertThrows(function() { patch(fun_outside, "'Cat'", "'Cobra'") },
                 LiveEdit.Failure);
    // Patching inside a generator activation may succeed.
    patch(fun_inside, "'Cat'", "'Koala'");
  }
  iter = generator(function() { return fun_inside(attempt_fun_patches) });
  assertEquals('Cat',
               fun_outside(function () {
                 assertIteratorResult('Koala', false, iter.next());
                 assertTrue(fun_patch_restarted);
               }));
})();
