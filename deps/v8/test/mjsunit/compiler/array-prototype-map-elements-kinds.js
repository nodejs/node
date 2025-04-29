// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --no-optimize-on-next-call-optimizes-to-maglev --no-turbolev

// TODO(399393885): --no-optimize-on-next-call-optimizes-to-maglev and
// --no-turbolev can be removed once the bug is fixed, since then Turbofan and
// Maglev implementations will behave the same way.

load('test/mjsunit/elements-kinds-helpers.js');

function identity(x) {
  return x;
}
%PrepareFunctionForOptimization(identity);

function plusOne(x) {
  return x + 1;
}
%PrepareFunctionForOptimization(plusOne);

function plusOneAndMore(x) {
  return x + 1.1;
}
%PrepareFunctionForOptimization(plusOneAndMore);

function plusOneInObject(x) {
  return {a: x.a + 1};
}
%PrepareFunctionForOptimization(plusOneInObject);

function wrapInObject(x) {
  return {a: x};
}
%PrepareFunctionForOptimization(wrapInObject);

function unwrapObject(x) {
  return x.a;
}
%PrepareFunctionForOptimization(unwrapObject);

let ix = 0;
function transitionFromSmiToDouble(x) {
  if (ix++ == 0) {
    return 0;
  }
  return 1.1;
}
%PrepareFunctionForOptimization(transitionFromSmiToDouble);

function transitionFromSmiToObject(x) {
  if (ix++ == 0) {
    return 0;
  }
  return {a: 1};
}
%PrepareFunctionForOptimization(transitionFromSmiToObject);

function transitionFromDoubleToObject(x) {
  if (ix++ == 0) {
    return 1.1;
  }
  return {a: 1};
}
%PrepareFunctionForOptimization(transitionFromDoubleToObject);

function transitionFromObjectToDouble(x) {
  if (ix++ == 0) {
    return {a: 1};
  }
  return 1.1;
}
%PrepareFunctionForOptimization(transitionFromObjectToDouble);

function resetTransitionFunctions() {
  ix = 0;
}

(function testPackedSmiElementsWithIdentity() {
  function foo(a) {
    return a.map(identity);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, 3];
  const result = foo(array);
  assertTrue(HasPackedSmiElements(result));

  %OptimizeFunctionOnNextCall(foo);

  const array2 = [1, 2, 3];
  const result2 = foo(array2);
    // TODO(399393885): Make the optimized version use the same elements kind as
  // the unoptimized version.
  // assertTrue(HasPackedSmiElements(result2));
  assertTrue(HasHoleySmiElements(result2));
  assertOptimized(foo);
})();

(function testPackedSmiElements() {
  function foo(a) {
    return a.map(plusOne);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, 3];
  const result = foo(array);
  assertTrue(HasPackedSmiElements(result));

  %OptimizeFunctionOnNextCall(foo);

  const array2 = [1, 2, 3];
  const result2 = foo(array2);

  // TODO(399393885): Make the optimized version use the same elements kind as
  // the unoptimized version.
  // assertTrue(HasPackedSmiElements(result2));
  assertTrue(HasHoleyDoubleElements(result2) || HasHoleySmiElements(result2));

  assertOptimized(foo);
})();

(function testHoleySmiElements() {
  function foo(a) {
    return a.map(plusOne);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, , 3];
  const result = foo(array);
  assertTrue(HasHoleySmiElements(result));

  %OptimizeFunctionOnNextCall(foo);

  const array2 = [1, 2, , 3];
  const result2 = foo(array2);

  // TODO(399393885): Make the optimized version use the same elements kind as
  // the unoptimized version.
  // assertTrue(HasHoleySmiElements(result2));
  assertTrue(HasHoleyDoubleElements(result2) || HasHoleySmiElements(result2));

  assertOptimized(foo);
})();

(function testPackedDoubleElements1() {
  function foo(a) {
    return a.map(plusOne);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, 3.3];
  const result = foo(array);
  assertTrue(HasPackedDoubleElements(result));

  %OptimizeFunctionOnNextCall(foo);

  const array2 = [1, 2, 3.3];
  const result2 = foo(array2);

  // TODO(399393885): Make the optimized version use the same elements kind as
  // the unoptimized version.
  // assertTrue(HasPackedDoubleElements(result2));
  assertTrue(HasHoleyDoubleElements(result2));
  assertOptimized(foo);

})();

(function testPackedDoubleElements2() {
  function foo(a) {
    return a.map(plusOneAndMore);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, 3];
  const result = foo(array);
  assertTrue(HasPackedDoubleElements(result));

  %OptimizeFunctionOnNextCall(foo);

  const array2 = [1, 2, 3];
  const result2 = foo(array2);

  // TODO(399393885): Make the optimized version use the same elements kind as
  // the unoptimized version.
  // assertTrue(HasPackedDoubleElements(result2));
  assertTrue(HasHoleyDoubleElements(result2));

  assertOptimized(foo);
})();

(function testHoleyDoubleElements1() {
  function foo(a) {
    return a.map(plusOne);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, , 3.3];
  const result = foo(array);
  assertTrue(HasHoleyDoubleElements(result));

  %OptimizeFunctionOnNextCall(foo);

  const array2 = [1, 2, , 3.3];
  const result2 = foo(array2);
  assertTrue(HasHoleyDoubleElements(result2));
  assertOptimized(foo);
})();

(function testHoleyDoubleElements2() {
  function foo(a) {
    return a.map(plusOneAndMore);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, , 3];
  const result = foo(array);
  assertTrue(HasHoleyDoubleElements(result));

  %OptimizeFunctionOnNextCall(foo);

  const array2 = [1, 2, , 3];
  const result2 = foo(array2);
  assertTrue(HasHoleyDoubleElements(result2));
  assertOptimized(foo);
})();

(function testPackedElements1() {
  function foo(a) {
    return a.map(plusOneInObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [{a: 1}, {a: 2}, {a: 3}];
  const result = foo(array);
  assertTrue(HasPackedObjectElements(result));

  %OptimizeFunctionOnNextCall(foo);

  const array2 = [{a: 1}, {a: 2}, {a: 3}];
  const result2 = foo(array2);

  // TODO(399393885): Make the optimized version use the same elements kind as
  // the unoptimized version.
  // assertTrue(HasPackedObjectElements(result2));
  assertTrue(HasHoleyObjectElements(result2));

  assertOptimized(foo);
})();

(function testPackedElements2() {
  function foo(a) {
    return a.map(wrapInObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, 3];
  const result = foo(array);
  assertTrue(HasPackedObjectElements(result));

  %OptimizeFunctionOnNextCall(foo);

  const array2 = [1, 2, 3];
  const result2 = foo(array2);

  // TODO(399393885): Make the optimized version use the same elements kind as
  // the unoptimized version.
  // assertTrue(HasPackedObjectElements(result2));
  assertTrue(HasHoleyObjectElements(result2));

  assertOptimized(foo);
})();

(function testHoleyObjectElements1() {
  function foo(a) {
    return a.map(plusOneInObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [{a: 1}, {a: 2}, , {a: 3}];
  const result = foo(array);
  assertTrue(HasHoleyObjectElements(result));

  %OptimizeFunctionOnNextCall(foo);

  const array2 = [{a: 1}, {a: 2}, , {a: 3}];
  const result2 = foo(array2);
  assertTrue(HasHoleyObjectElements(result2));
  assertOptimized(foo);
})();

(function testHoleyObjectElements2() {
  function foo(a) {
    return a.map(wrapInObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, , 3];
  const result = foo(array);
  assertTrue(HasHoleyObjectElements(result));

  %OptimizeFunctionOnNextCall(foo);

  const array2 = [1, 2, , 3];
  const result2 = foo(array2);
  assertTrue(HasHoleyObjectElements(result2));
  assertOptimized(foo);
})();


(function testFromObjectToPackedSmi() {
  function foo(a) {
    return a.map(unwrapObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [{a: 1}, {a: 2}, {a: 3}];
  const array2 = [{a: 1}, {a: 2}, {a: 3}];
  const result = foo(array);
  assertTrue(HasPackedSmiElements(result));

  %OptimizeFunctionOnNextCall(foo);

  const result2 = foo(array2);

  // TODO(399393885): Make the optimized version use the same elements kind as
  // the unoptimized version.
  // assertTrue(HasPackedSmiElements(result2));
  assertTrue(HasHoleySmiElements(result2));

  assertOptimized(foo);
})();

(function testFromObjectToHoleySmi() {
  function foo(a) {
    return a.map(unwrapObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [{a: 1}, {a: 2}, , {a: 3}];
  const array2 = [{a: 1}, {a: 2}, , {a: 3}];
  const result = foo(array);
  assertTrue(HasHoleySmiElements(result));

  %OptimizeFunctionOnNextCall(foo);

  const result2 = foo(array2);
  assertTrue(HasHoleySmiElements(result2));
  assertOptimized(foo);
})();

(function testTransitionFromPackedSmiToDoubleWhileMapping() {
  resetTransitionFunctions();

  function foo(a) {
    return a.map(transitionFromSmiToDouble);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [0, 0, 0];
  const array2 = [0, 0, 0];
  const result = foo(array);
  assertTrue(HasPackedDoubleElements(result));

  %OptimizeFunctionOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);

  // TODO(399393885): Make the optimized version use the same elements kind as
  // the unoptimized version.
  // assertTrue(HasPackedDoubleElements(result2));
  assertTrue(HasHoleyDoubleElements(result2));

  assertOptimized(foo);
})();

(function testTransitionFromHoleySmiToDoubleWhileMapping() {
  resetTransitionFunctions();

  function foo(a) {
    return a.map(transitionFromSmiToDouble);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [0, 0, , 0];
  const array2 = [0, 0, , 0];
  const result = foo(array);
  assertTrue(HasHoleyDoubleElements(result));

  %OptimizeFunctionOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);
  assertTrue(HasHoleyDoubleElements(result2));
  assertOptimized(foo);
})();

(function testTransitionFromPackedSmiToObjectWhileMapping() {
  resetTransitionFunctions();

  function foo(a) {
    return a.map(transitionFromSmiToObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [0, 0, 0];
  const array2 = [0, 0, 0];
  const result = foo(array);
  assertTrue(HasPackedObjectElements(result));

  %OptimizeFunctionOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);

  // TODO(399393885): Make the optimized version use the same elements kind as
  // the unoptimized version.
  // assertTrue(HasPackedObjectElements(result2));
  assertTrue(HasHoleyObjectElements(result2));

  assertOptimized(foo);
})();

(function testTransitionFromHoleySmiToObjectWhileMapping() {
  resetTransitionFunctions();

  function foo(a) {
    return a.map(transitionFromSmiToObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [0, 0, , 0];
  const array2 = [0, 0, , 0];
  const result = foo(array);
  assertTrue(HasHoleyObjectElements(result));

  %OptimizeFunctionOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);
  assertTrue(HasHoleyObjectElements(result2));
  assertOptimized(foo);
})();

(function testTransitionFromPackedSmiToDoubleToObjectWhileMapping() {
  resetTransitionFunctions();

  function foo(a) {
    return a.map(transitionFromDoubleToObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [0, 0, 0];
  const array2 = [0, 0, 0];
  const result = foo(array);
  assertTrue(HasPackedObjectElements(result));

  %OptimizeFunctionOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);

  // TODO(399393885): Make the optimized version use the same elements kind as
  // the unoptimized version.
  // assertTrue(HasPackedObjectElements(result2));
  assertTrue(HasHoleyObjectElements(result2));

  assertOptimized(foo);
})();

(function testTransitionFromHoleySmiToDoubleToObjectWhileMapping() {
  resetTransitionFunctions();

  function foo(a) {
    return a.map(transitionFromDoubleToObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [0, 0, , 0];
  const array2 = [0, 0, , 0];
  const result = foo(array);
  assertTrue(HasHoleyObjectElements(result));

  %OptimizeFunctionOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);
  assertTrue(HasHoleyObjectElements(result2));
  assertOptimized(foo);
})();

(function testTransitionFromPackedSmiToObjectToDoubleWhileMapping() {
  resetTransitionFunctions();

  function foo(a) {
    return a.map(transitionFromObjectToDouble);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [0, 0, 0];
  const array2 = [0, 0, 0];
  const result = foo(array);
  assertTrue(HasPackedObjectElements(result));

  %OptimizeFunctionOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);

  // TODO(399393885): Make the optimized version use the same elements kind as
  // the unoptimized version.
  // assertTrue(HasPackedObjectElements(result2));
  assertTrue(HasHoleyObjectElements(result2));

  assertOptimized(foo);
})();

(function testTransitionFromHoleySmiToObjectToDoubleWhileMapping() {
  resetTransitionFunctions();

  function foo(a) {
    return a.map(transitionFromObjectToDouble);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [0, 0, , 0];
  const array2 = [0, 0, , 0];
  const result = foo(array);
  assertTrue(HasHoleyObjectElements(result));

  %OptimizeFunctionOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);
  assertTrue(HasHoleyObjectElements(result2));
  assertOptimized(foo);
})();

(function testMapperReducesLength() {
  let array = [0, 1, 2];
  function evil(x) {
    if (x == 1) {
      array.length = 1;
    }
    return x;
  }
  %PrepareFunctionForOptimization(evil);
  function foo(a) {
    return a.map(evil);
  }
  %PrepareFunctionForOptimization(foo);

  const result = foo(array);
  assertTrue(HasHoleySmiElements(result));
  assertEquals(undefined, result[2]);

  array = [0, 1, 2];
  %OptimizeFunctionOnNextCall(foo);

  const result2 = foo(array);
  assertTrue(HasHoleySmiElements(result2));
  assertEquals(undefined, result2[2]);
  // Deopted since we iterated the array out of bounds.
  assertUnoptimized(foo);
})();
