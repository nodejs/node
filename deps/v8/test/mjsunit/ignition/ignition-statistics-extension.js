// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition --turbo --trace-ignition-dispatches

assertEquals(typeof getIgnitionDispatchCounters, "function");

var old_dispatch_counters = getIgnitionDispatchCounters();

// Check that old_dispatch_counters is a non-empty object of objects, such that
// the value of each property in the inner objects is a number.

assertEquals(typeof old_dispatch_counters, "object");
assertTrue(Object.getOwnPropertyNames(old_dispatch_counters).length > 0);
for (var source_bytecode in old_dispatch_counters) {
  var counters_row = old_dispatch_counters[source_bytecode];
  assertEquals(typeof counters_row, "object");
  for (var counter in counters_row) {
    assertEquals(typeof counters_row[counter], "number");
  }
}

// Do something
function f(x) { return x*x; }
f(42);

var new_dispatch_counters = getIgnitionDispatchCounters();

var old_source_bytecodes = Object.getOwnPropertyNames(old_dispatch_counters);
var new_source_bytecodes = Object.getOwnPropertyNames(new_dispatch_counters);
var common_source_bytecodes = new_source_bytecodes.filter(function (name) {
  return old_source_bytecodes.indexOf(name) > -1;
});

// Check that the keys on the outer objects are the same
assertEquals(common_source_bytecodes, old_source_bytecodes);
assertEquals(common_source_bytecodes, new_source_bytecodes);

common_source_bytecodes.forEach(function (source_bytecode) {
  var new_counters_row = new_dispatch_counters[source_bytecode];
  var old_counters_row = old_dispatch_counters[source_bytecode];

  var old_destination_bytecodes = Object.getOwnPropertyNames(old_counters_row);
  var new_destination_bytecodes = Object.getOwnPropertyNames(new_counters_row);

  // Check that all the keys in old_ are in new_ too
  old_destination_bytecodes.forEach(function (name) {
    assertTrue(new_destination_bytecodes.indexOf(name) > -1);
  });

  // Check that for each source-destination pair, the counter has either
  // appeared (was undefined before calling f()), is unchanged, or incremented.
  new_destination_bytecodes.forEach(function (destination_bytecode) {
    var new_counter = new_counters_row[destination_bytecode];
    var old_counter = old_counters_row[destination_bytecode];
    assertTrue(typeof new_counter === "number");
    if (typeof old_counter === "number") {
      assertTrue(new_counter >= old_counter);
    }
  });
});
