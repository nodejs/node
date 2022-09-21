// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --no-stress-flush-code --concurrent-recompilation
//
// Tests tracking of constness of properties stored in dictionary
// mode prototypes.


var unique_id = 0;
// Creates a function with unique SharedFunctionInfo to ensure the feedback
// vector is unique for each test case.
function MakeFunctionWithUniqueSFI(...args) {
  assertTrue(args.length > 0);
  var body = `/* Unique comment: ${unique_id++} */ ` + args.pop();
  return new Function(...args, body);
}

// Invalidation by store handler.
(function() {
  var proto = Object.create(null);
  proto.z = 1;
  assertFalse(%HasFastProperties(proto));

  var o = Object.create(proto);

  function read_z() {
    return o.z;
  }
  function update_z(new_value) {
    proto.z = new_value;
  }

  // Allocate feedback vector, but we don't want to optimize the function.
  %PrepareFunctionForOptimization(read_z);
  for (var i = 0; i < 4; i++) {
    read_z();
  }
  assertTrue(%HasOwnConstDataProperty(proto, "z"));

  // Allocate feedback vector, but we don't want to optimize the function.
  %PrepareFunctionForOptimization(update_z);
  for (var i = 0; i < 4; i++) {
    // Overwriting with same value maintains const-ness.
    update_z(1);
  }


  assertTrue(%HasOwnConstDataProperty(proto, "z"));

  update_z(2);

  assertFalse(%HasOwnConstDataProperty(proto, "z"));
  assertEquals(2, read_z());
})();

// Properties become const when dict mode object becomes prototype.
(function() {
  var proto = Object.create(null);
  var proto_shadow = Object.create(null);

  proto.z = 1;
  proto_shadow.z = 1;

  // Make sure that z is marked as mutable.
  proto.z = 2;
  proto_shadow.z = 2;

  assertFalse(%HasFastProperties(proto));
  assertTrue(%HaveSameMap(proto, proto_shadow));

  var o = Object.create(proto);

  assertFalse(%HasFastProperties(proto));
  // proto must have received new map.
  assertFalse(%HaveSameMap(proto, proto_shadow));
  assertEquals(%IsDictPropertyConstTrackingEnabled(),
               %HasOwnConstDataProperty(proto, "z"));
})();

// Properties become const when fast mode object becomes prototype.
(function() {
  var proto = {}
  var proto_shadow = {};

  proto.z = 1;
  proto_shadow.z = 1;

  // Make sure that z is marked as mutable.
  proto.z = 2;
  proto_shadow.z = 2;

  assertTrue(%HasFastProperties(proto));
  assertTrue(%HaveSameMap(proto, proto_shadow));

  var o = Object.create(proto);

  assertFalse(%HasFastProperties(proto));
  // proto must have received new map.
  assertFalse(%HaveSameMap(proto, proto_shadow));
  assertEquals(%IsDictPropertyConstTrackingEnabled(),
               %HasOwnConstDataProperty(proto, "z"));
})();

function testbench(o, proto, update_proto, check_constness) {
  var check_z = MakeFunctionWithUniqueSFI("obj", "return obj.z;");

  if (check_constness && %IsDictPropertyConstTrackingEnabled())
    assertTrue(%HasOwnConstDataProperty(proto, "z"));

  // Allocate feedback vector, but we don't want to optimize the function.
  %PrepareFunctionForOptimization(check_z);
  for (var i = 0; i < 4; i++) {
    check_z(o);
  }

  update_proto();

  if (%IsDictPropertyConstTrackingEnabled()) {
    if (check_constness)
      assertFalse(%HasOwnConstDataProperty(proto, "z"));
    assertFalse(%HasFastProperties(proto));
  }

  assertEquals("2", check_z(o));
}

// Simple update.
(function() {
  var proto = Object.create(null);
  proto.z = "1";
  assertFalse(%HasFastProperties(proto));

  var o = Object.create(proto);

  function update_z() {
    proto.z = "2";
  }

  testbench(o, proto, update_z, true);
})();

// Update using Object.assign.
(function() {
  var proto = Object.create(null);
  proto.z = "1";
  assertFalse(%HasFastProperties(proto));

  var o = Object.create(proto);

  function update_z() {
    Object.assign(proto, {z: "2"});
  }

  testbench(o, proto, update_z, true);
})();

// Update using Object.defineProperty
(function() {
  var proto = Object.create(null);
  proto.z = "1";
  assertFalse(%HasFastProperties(proto));

  var o = Object.create(proto);

  function update_z() {
    Object.defineProperty(proto, 'z', {
      value: "2",
      configurable: true,
      enumerable: true,
      writable: true
    });
  }

  testbench(o, proto, update_z, true);
})();


// Update using setter
(function() {
  var proto = Object.create(null);
  Object.defineProperty(proto, "z", {
    get : function () {return this.z_val;},
    set : function (new_z) {this.z_val = new_z;}
  });

  proto.z = "1";
  assertFalse(%HasFastProperties(proto));

  var o = Object.create(proto);

  function update_z() {
    proto.z = "2";
  }

  testbench(o, proto, update_z, false);
})();

// Proxy test 1: Update via proxy.
(function() {
  var proto = Object.create(null);

  var proxy = new Proxy(proto, {});

  proxy.z = "1";
  assertFalse(%HasFastProperties(proto));

  var o = Object.create(proxy);

  function update_z() {
    proxy.z = "2";
  }

  testbench(o, proto, update_z, false);
})();

// Proxy test 2: Update on proto.
(function() {
  var proto = Object.create(null);

  var proxy = new Proxy(proto, {});

  proto.z = "1";
  assertFalse(%HasFastProperties(proto));

  var o = Object.create(proxy);

  function update_z() {
    proto.z = "2";
  }

  testbench(o, proto, update_z, false);
})();

// Proxy test 3: Update intercepted.
(function() {
  var proto = Object.create(null);

  var handler = {
    get: function(target, prop) {
      return target.the_value;
    },
    set: function(target, prop, value) {
     return target.the_value = value;
    }
  };

  var proxy = new Proxy(proto, handler);

  proxy.z = "1";
  assertFalse(%HasFastProperties(proto));

  var o = Object.create(proxy);

  function update_z() {
    proxy.z = "2";
  }

  testbench(o, proto, update_z, false);

})();

//
// Below: Testing TF optimization of accessing constants in dictionary mode
// protoypes.
//

// Test inlining with fast mode receiver.
(function() {
  var proto = Object.create(null);
  proto.x = 1;
  var o = Object.create(proto);
  assertTrue(%HasFastProperties(o));
  assertFalse(%HasFastProperties(proto));

  function read_x(arg_o) {
    return arg_o.x;
  }

  %PrepareFunctionForOptimization(read_x);
  assertEquals(1, read_x(o));
  %OptimizeFunctionOnNextCall(read_x);
  assertEquals(1, read_x(o));
  assertOptimized(read_x);


  // Test that we inlined the access:
  var dummy = {x : 123};
  read_x(dummy);

  if (%IsDictPropertyConstTrackingEnabled()) {
    assertTrue(%HasFastProperties(o));
    assertFalse(%HasFastProperties(proto));
    assertUnoptimized(read_x);
  }

})();

// Test inlining with dictionary mode receiver that is a prototype.

(function() {

  var proto1 = Object.create(null);
  proto1.x = 1;
  var proto2 = Object.create(null);
  var o = Object.create(proto1);
  Object.setPrototypeOf(proto1, proto2);

  assertTrue(%HasFastProperties(o));
  assertFalse(%HasFastProperties(proto1));
  assertFalse(%HasFastProperties(proto2));

  function read_x(arg_o) {
    return arg_o.x;
  }

  %PrepareFunctionForOptimization(read_x);
  assertEquals(1, read_x(proto1));
  %OptimizeFunctionOnNextCall(read_x);
  assertEquals(1, read_x(proto1));
  assertOptimized(read_x);

  // Test that we inlined the access:
  var dummy = {x : 123};
  read_x(dummy);

  // TODO(v8:11457) This test doesn't work yet, see TODO in
  // AccessInfoFactory::TryLoadPropertyDetails. Currently, we can't inline
  // accesses with dictionary mode receivers.
  // if (%IsDictPropertyConstTrackingEnabled()) {
  //   assertTrue(%HasFastProperties(o));
  //   assertFalse(%HasFastProperties(proto1));
  //   assertFalse(%HasFastProperties(proto2));
  //   assertUnoptimized(read_x);
  // }
})();

// The machinery we use for detecting the invalidation of constants held by
// dictionary mode objects (related to the prototype validity cell mechanism) is
// specific to prototypes. This means that for non-prototype dictionary mode
// objects, we have no way of detecting changes invalidating folded
// constants. Therefore, we must not fold constants held by non-prototype
// dictionary mode objects. This is tested here.
(function() {
  var proto = Object.create(null);
  proto.x = 1;
  var o = Object.create(null);
  Object.setPrototypeOf(o, proto);
  assertFalse(%HasFastProperties(o));
  assertFalse(%HasFastProperties(proto));

  function read_x(arg_o) {
    return arg_o.x;
  }

  %PrepareFunctionForOptimization(read_x);
  assertEquals(1, read_x(o));
  %OptimizeFunctionOnNextCall(read_x);
  assertEquals(1, read_x(o));
  assertOptimized(read_x);

  var dummy = {x : 123};
  read_x(dummy);

  if (%IsDictPropertyConstTrackingEnabled()) {
    assertFalse(%HasFastProperties(o));
    assertFalse(%HasFastProperties(proto));

    // We never inlined the acceess, so it's still optimized.
    assertOptimized(read_x);
  }
})();

// Test inlining of accessor.
(function() {
  var proto = Object.create(null);
  proto.x_val = 1;
  Object.defineProperty(proto, "x", {
    get : function () {return this.x_val;}
  });

  var o = Object.create(proto);
  assertFalse(%HasFastProperties(proto))

  function read_x(arg_o) {
    return arg_o.x;
  }

  %PrepareFunctionForOptimization(read_x);
  assertEquals(1, read_x(o));
  %OptimizeFunctionOnNextCall(read_x);
  assertEquals(1, read_x(o));
  assertOptimized(read_x);

  // Test that we inlined the access:
  var dummy = {x : 123};
  read_x(dummy);

  if (%IsDictPropertyConstTrackingEnabled()) {
    assertTrue(%HasFastProperties(o));
    assertFalse(%HasFastProperties(proto));
    assertUnoptimized(read_x);
  }
})();

// Invalidation by adding same property to receiver.
(function() {
  var proto = Object.create(null);
  proto.x = 1;
  var o = Object.create(proto);
  assertTrue(%HasFastProperties(o));
  assertFalse(%HasFastProperties(proto));

  function read_x(arg_o) {
    return arg_o.x;
  }

  %PrepareFunctionForOptimization(read_x);
  assertEquals(1, read_x(o));
  %OptimizeFunctionOnNextCall(read_x);
  assertEquals(1, read_x(o));
  assertOptimized(read_x);

  o.x = 2;

  assertEquals(2, read_x(o));

  if (%IsDictPropertyConstTrackingEnabled()) {
    assertTrue(%HasFastProperties(o));
    assertFalse(%HasFastProperties(proto));
    assertUnoptimized(read_x);
  }

})();

// Invalidation by adding property to intermediate prototype.
(function() {
  var proto = Object.create(null);
  proto.x = 1;
  var in_between = Object.create(null);
  Object.setPrototypeOf(in_between, proto);

  var o = Object.create(in_between);
  assertTrue(%HasFastProperties(o));
  assertFalse(%HasFastProperties(in_between));
  assertFalse(%HasFastProperties(proto));

  function read_x(arg_o) {
    return arg_o.x;
  }

  %PrepareFunctionForOptimization(read_x);
  assertEquals(1, read_x(o));
  %OptimizeFunctionOnNextCall(read_x);
  assertEquals(1, read_x(o));
  assertOptimized(read_x);

  in_between.x = 2;

  if (%IsDictPropertyConstTrackingEnabled()) {
    assertFalse(%HasFastProperties(in_between));
    assertFalse(%HasFastProperties(proto));
    assertUnoptimized(read_x);
  }

  assertEquals(2, read_x(o));
})();

// Invalidation by changing prototype of receiver.
(function() {
  var proto = Object.create(null);
  proto.x = 1;
  var other_proto = Object.create(null);
  other_proto.x = 2;

  var o = Object.create(proto);
  assertTrue(%HasFastProperties(o));
  assertFalse(%HasFastProperties(proto));

  function read_x(arg_o) {
    return arg_o.x;
  }

  %PrepareFunctionForOptimization(read_x);
  assertEquals(1, read_x(o));
  %OptimizeFunctionOnNextCall(read_x);
  assertEquals(1, read_x(o));

  Object.setPrototypeOf(o, other_proto);
  assertEquals(2, read_x(o));

  if (%IsDictPropertyConstTrackingEnabled()) {
    assertFalse(%HasFastProperties(proto));
    assertFalse(%HasFastProperties(other_proto));
    assertUnoptimized(read_x);
  }
})();

// Invalidation by changing [[Prototype]] of a prototype on the chain from the
// receiver to the holder.
(function() {
  var proto = Object.create(null);
  proto.x = 1;
  var other_proto = Object.create(null);
  other_proto.x = 2;
  var in_between = Object.create(null);
  Object.setPrototypeOf(in_between, proto);

  var o = Object.create(in_between);
  assertTrue(%HasFastProperties(o));
  assertFalse(%HasFastProperties(in_between));
  assertFalse(%HasFastProperties(proto));

  function read_x(arg_o) {
    return arg_o.x;
  }

  %PrepareFunctionForOptimization(read_x);
  assertEquals(1, read_x(o));
  %OptimizeFunctionOnNextCall(read_x);
  assertEquals(1, read_x(o));
  assertOptimized(read_x);

  Object.setPrototypeOf(in_between, other_proto);

  if (%IsDictPropertyConstTrackingEnabled()) {
    assertFalse(%HasFastProperties(in_between));
    assertFalse(%HasFastProperties(proto));
    assertFalse(%HasFastProperties(other_proto));
    assertUnoptimized(read_x);
  }

  assertEquals(2, read_x(o));
})();

// Invalidation by changing property on prototype itself.
(function() {
  var proto = Object.create(null);
  proto.x = 1;
  var o = Object.create(proto);
  assertTrue(%HasFastProperties(o));
  assertFalse(%HasFastProperties(proto));

  function read_x(arg_o) {
    return arg_o.x;
  }

  %PrepareFunctionForOptimization(read_x);
  assertEquals(1, read_x(o));
  %OptimizeFunctionOnNextCall(read_x);
  assertEquals(1, read_x(o));
  assertOptimized(read_x);

  proto.x = 2;

  if (%IsDictPropertyConstTrackingEnabled()) {
    assertFalse(%HasFastProperties(proto));
    assertUnoptimized(read_x);
  }

  assertEquals(2, read_x(o));
})();

// Invalidation by deleting property on prototype.
(function() {
  var proto = Object.create(null);
  proto.x = 1;
  var o = Object.create(proto);
  assertTrue(%HasFastProperties(o));
  assertFalse(%HasFastProperties(proto));

  function read_x(arg_o) {
    return arg_o.x;
  }

  %PrepareFunctionForOptimization(read_x);
  read_x(o);
  %OptimizeFunctionOnNextCall(read_x);
  read_x(o);

  delete proto.x;

  if (%IsDictPropertyConstTrackingEnabled()) {
    assertFalse(%HasFastProperties(proto));
    assertUnoptimized(read_x);
  }

  assertEquals(undefined, read_x(o));
})();

// Storing the same value does not invalidate const-ness. Store done from
// runtime/without feedback.
(function() {
  var proto = Object.create(null);
  var some_object = {bla: 123};
  proto.x = 1;
  proto.y = some_object

  var o = Object.create(proto);
  assertTrue(%HasFastProperties(o));
  assertFalse(%HasFastProperties(proto));

  function read_xy(arg_o) {
    return [arg_o.x, arg_o.y];
  }

  %PrepareFunctionForOptimization(read_xy);
  assertEquals([1, some_object], read_xy(o));
  %OptimizeFunctionOnNextCall(read_xy);
  assertEquals([1, some_object], read_xy(o));
  assertOptimized(read_xy);

  // Build value 1 without re-using proto.x.
  var x2 = 0;
  for(var i = 0; i < 5; ++i) {
    x2 += 0.2;
  }

  // Storing the same values for x and y again:
  proto.x = x2;
  proto.y = some_object;
  assertEquals(x2, proto.x);

  if (%IsDictPropertyConstTrackingEnabled()) {
    assertFalse(%HasFastProperties(proto));
    assertTrue(%HasOwnConstDataProperty(proto, "x"));
    assertOptimized(read_xy);
  }

  proto.x = 2;
  if (%IsDictPropertyConstTrackingEnabled()) {
    assertFalse(%HasFastProperties(proto));
    assertFalse(%HasOwnConstDataProperty(proto, "x"));
    assertUnoptimized(read_xy);
  }

  assertEquals(2, read_xy(o)[0]);
})();

// Storing the same value does not invalidate const-ness. Store done by IC
// handler.
(function() {
  var proto = Object.create(null);
  var some_object = {bla: 123};
  proto.x = 1;
  proto.y = some_object

  var o = Object.create(proto);
  assertTrue(%HasFastProperties(o));
  assertFalse(%HasFastProperties(proto));

  function read_xy(arg_o) {
    return [arg_o.x, arg_o.y];
  }

  %PrepareFunctionForOptimization(read_xy);
  assertEquals([1, some_object], read_xy(o));
  %OptimizeFunctionOnNextCall(read_xy);
  assertEquals([1, some_object], read_xy(o));
  assertOptimized(read_xy);

  // Build value 1 without re-using proto.x.
  var x2 = 0;
  for(var i = 0; i < 5; ++i) {
    x2 += 0.2;
  }

  function change_xy(obj, x, y) {
    obj.x = x;
    obj.y = y;
  }

  %PrepareFunctionForOptimization(change_xy);
  // Storing the same values for x and y again:
  change_xy(proto, 1, some_object);
  change_xy(proto, 1, some_object);

  if (%IsDictPropertyConstTrackingEnabled()) {
    assertFalse(%HasFastProperties(proto));
    assertTrue(%HasOwnConstDataProperty(proto, "x"));
    assertOptimized(read_xy);
  }

  change_xy(proto, 2, some_object);

  if (%IsDictPropertyConstTrackingEnabled()) {
    assertFalse(%HasFastProperties(proto));
    assertFalse(%HasOwnConstDataProperty(proto, "x"));
    assertUnoptimized(read_xy);
  }

  assertEquals(2, read_xy(o)[0]);
})();

// Invalidation by replacing a prototype. Just like the old prototype, the new
// prototype owns the property as an accessor, but in the form of an
// AccessorInfo rather than an AccessorPair.
(function() {
  var proto1 = Object.create(null);
  Object.defineProperty(proto1, 'length', {get() {return 1}});
  var proto2 = Object.create(proto1);
  var o = Object.create(proto2);
  assertTrue(%HasFastProperties(o));
  assertFalse(%HasFastProperties(proto1));
  assertFalse(%HasFastProperties(proto2));

  function read_length(arg_o) {
    return arg_o.length;
  }

  %PrepareFunctionForOptimization(read_length);
  assertEquals(1, read_length(o));
  %DisableOptimizationFinalization();
  %OptimizeFunctionOnNextCall(read_length, "concurrent");
  assertEquals(1, read_length(o));
  assertUnoptimized(read_length);

  %WaitForBackgroundOptimization();
  var other_proto1 = [];
  Object.setPrototypeOf(proto2, other_proto1);
  %FinalizeOptimization();

  assertUnoptimized(read_length);
  assertEquals(0, read_length(o));

  if (%IsDictPropertyConstTrackingEnabled()) {
    assertFalse(%HasFastProperties(proto1));
    assertFalse(%HasFastProperties(proto2));
    assertFalse(%HasFastProperties(other_proto1));
    assertUnoptimized(read_length);
  }
})();
