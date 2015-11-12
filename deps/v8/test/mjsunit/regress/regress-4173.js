// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function Migrator(o) {
  return o.foo;
}
function Loader(o) {
  return o[0];
}

var first_smi_array = [1];
var second_smi_array = [2];
var first_object_array = ["first"];
var second_object_array = ["string"];

assertTrue(%HasFastSmiElements(first_smi_array));
assertTrue(%HasFastSmiElements(second_smi_array));
assertTrue(%HasFastObjectElements(first_object_array));
assertTrue(%HasFastObjectElements(second_object_array));

// Prepare identical transition chains for smi and object arrays.
first_smi_array.foo = 0;
second_smi_array.foo = 0;
first_object_array.foo = 0;
second_object_array.foo = 0;

// Collect type feedback for not-yet-deprecated original object array map.
for (var i = 0; i < 3; i++) Migrator(second_object_array);

// Blaze a migration trail for smi array maps.
// This marks the migrated smi array map as a migration target.
first_smi_array.foo = 0.5;
print(second_smi_array.foo);

// Deprecate original object array map.
// Use TryMigrate from deferred optimized code to migrate second object array.
first_object_array.foo = 0.5;
%OptimizeFunctionOnNextCall(Migrator);
Migrator(second_object_array);

// |second_object_array| now erroneously has a smi map.
// Optimized code assuming smi elements will expose this.

for (var i = 0; i < 3; i++) Loader(second_smi_array);
%OptimizeFunctionOnNextCall(Loader);
assertEquals("string", Loader(second_object_array));

// Any of the following checks will also fail:
assertTrue(%HasFastObjectElements(second_object_array));
assertFalse(%HasFastSmiElements(second_object_array));
assertTrue(%HaveSameMap(first_object_array, second_object_array));
assertFalse(%HaveSameMap(first_smi_array, second_object_array));

%ClearFunctionTypeFeedback(Loader);
%ClearFunctionTypeFeedback(Migrator);
