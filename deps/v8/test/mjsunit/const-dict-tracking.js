// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax
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
