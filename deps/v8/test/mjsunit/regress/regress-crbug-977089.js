// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// This function was carefully constructed by Clusterfuzz to execute a certain
// sequence of transitions. Thus, it may no longer test anything useful if
// the transition logic changes.
//
// The more stable unit test testing the same bug is:
//   test-field-type-tracking/NormalizeToMigrationTarget
var foo = function() {

  function f1(arg) {
    var ret = { x: arg };
    ret.__defineGetter__("y", function() { });
    return ret;
  }
  // Create v1 with a map with properties: {x:Smi, y:AccessorPair}
  let v1 = f1(10);
  // Create a map with properties: {x:Double, y:AccessorPair}, deprecating the
  // previous map.
  let v2 = f1(10.5);

  // Access x on v1 to a function that reads x, which triggers it to update its
  // map. This update transitions v1 to slow mode as there is already a "y"
  // transition with a different accessor.
  //
  // Note that the parent function `foo` can't be an IIFE, as then this callsite
  // would use the NoFeedback version of the LdaNamedProperty bytecode, and this
  // doesn't trigger the map update.
  v1.x;

  // Create v3 which overwrites a non-accessor with an accessor, triggering it
  // to normalize, and picking up the same cached normalized map as v1. However,
  // v3's map is not a migration target and v1's is (as it was migrated to when
  // updating v1), so the migration target bit doesn't match. This should be
  // fine and shouldn't trigger any DCHECKs.
  let v3 = { z:1 };
  v3.__defineGetter__("z", function() {});
};

%EnsureFeedbackVectorForFunction(foo);
foo();
