// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-turbofan

// This function will be optimized by Maglev.
function getTargetField(obj) {
  return obj.target_property;
}

// Helper to create an object in dictionary mode.
function createDictionaryObject() {
  const obj = {};
  // Adding a large number of properties forces the object into dictionary mode.
  for (let i = 0; i < 150; i++) {
    obj["p_" + i] = i;
  }
  obj.target_property = "safe_value";
  return obj;
}

const dictObj = createDictionaryObject();

// Warm up the IC system.
%PrepareFunctionForOptimization(getTargetField);
assertEquals("safe_value", getTargetField(dictObj));
assertEquals("safe_value", getTargetField(dictObj));

// Trigger Maglev optimization.
%OptimizeMaglevOnNextCall(getTargetField);
assertEquals("safe_value", getTargetField(dictObj));
assertTrue(isMaglevved(getTargetField));

// Test the fallback path: Value change.
dictObj.target_property = "updated_value";
assertEquals("updated_value", getTargetField(dictObj));

// Test the fallback path: Missing property.
// If the fast path reads the wrong offset for the key, this will fail
// to match and should safely fall back to returning undefined.
delete dictObj.target_property;
assertEquals(undefined, getTargetField(dictObj));

// Test the fallback path: Re-adding property.
dictObj.target_property = "revived_value";
assertEquals("revived_value", getTargetField(dictObj));
