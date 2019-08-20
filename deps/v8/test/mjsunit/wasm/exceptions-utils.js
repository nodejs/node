// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This file is intended to be loaded by other tests to provide utility methods
// requiring natives syntax (and hence not suited for the mjsunit.js file).

function assertWasmThrows(instance, runtime_id, values, code) {
  try {
    if (typeof code === 'function') {
      code();
    } else {
      eval(code);
    }
  } catch (e) {
    assertInstanceof(e, WebAssembly.RuntimeError);
    var e_runtime_id = %GetWasmExceptionId(e, instance);
    assertTrue(Number.isInteger(e_runtime_id));
    assertEquals(e_runtime_id, runtime_id);
    var e_values = %GetWasmExceptionValues(e);
    assertArrayEquals(values, e_values);
    return;  // Success.
  }
  throw new MjsUnitAssertionError('Did not throw expected <' + runtime_id +
                                  '> with values: ' + values);
}
