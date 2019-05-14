// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function push_wrapper(array, value) {
  array.push(value);
}
function pop_wrapper(array) {
  return array.pop();
}

// Test the frzon arrays throw an exception if you try to push to them, both in
// optimized and non-optimized code.
var array = [2, 2];
Object.freeze(array);

try { push_wrapper(array, 1); } catch (e) {}
assertEquals(2, array.length);
try { push_wrapper(array, 1); } catch (e) {}
assertEquals(2, array.length);
%OptimizeFunctionOnNextCall(push_wrapper);
try { push_wrapper(array, 1); } catch (e) {}
assertEquals(2, array.length);
try { push_wrapper(array, 1); } catch (e) {}
assertEquals(2, array.length);

try { pop_wrapper(array); } catch (e) {}
assertEquals(2, array.length);
try { pop_wrapper(array); } catch (e) {}
assertEquals(2, array.length);
%OptimizeFunctionOnNextCall(pop_wrapper);
try { pop_wrapper(array); } catch (e) {}
assertEquals(2, array.length);
try { pop_wrapper(array); } catch (e) {}
assertEquals(2, array.length);
