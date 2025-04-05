// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-turbofan
// Flags: --no-optimize-maglev-optimizes-to-turbofan

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

  %OptimizeMaglevOnNextCall(foo);

  const array2 = [1, 2, 3];
  const result2 = foo(array2);
  assertTrue(HasPackedSmiElements(result2));
  assertTrue(isMaglevved(foo));
})();

(function testPackedSmiElements() {
  function foo(a) {
    return a.map(plusOne);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, 3];
  const result = foo(array);
  assertTrue(HasPackedSmiElements(result));

  %OptimizeMaglevOnNextCall(foo);

  const array2 = [1, 2, 3];
  const result2 = foo(array2);
  assertTrue(HasPackedSmiElements(result2));
  assertTrue(isMaglevved(foo));
})();

(function testHoleySmiElements() {
  function foo(a) {
    return a.map(plusOne);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, , 3];
  const result = foo(array);
  assertTrue(HasHoleySmiElements(result));

  %OptimizeMaglevOnNextCall(foo);

  const array2 = [1, 2, , 3];
  const result2 = foo(array2);
  assertTrue(HasHoleySmiElements(result2));
  assertTrue(isMaglevved(foo));
})();

(function testPackedDoubleElements1() {
  function foo(a) {
    return a.map(plusOne);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, 3.3];
  const result = foo(array);
  assertTrue(HasPackedDoubleElements(result));

  %OptimizeMaglevOnNextCall(foo);

  const array2 = [1, 2, 3.3];
  const result2 = foo(array2);
  assertTrue(HasPackedDoubleElements(result2));
  assertTrue(isMaglevved(foo));
})();

(function testPackedDoubleElements2() {
  function foo(a) {
    return a.map(plusOneAndMore);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, 3];
  const result = foo(array);
  assertTrue(HasPackedDoubleElements(result));

  %OptimizeMaglevOnNextCall(foo);

  const array2 = [1, 2, 3];
  const result2 = foo(array2);
  assertTrue(HasPackedDoubleElements(result2));
  assertTrue(isMaglevved(foo));
})();

(function testHoleyDoubleElements1() {
  function foo(a) {
    return a.map(plusOne);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, , 3.3];
  const result = foo(array);
  assertTrue(HasHoleyDoubleElements(result));

  %OptimizeMaglevOnNextCall(foo);

  const array2 = [1, 2, , 3.3];
  const result2 = foo(array2);
  assertTrue(HasHoleyDoubleElements(result2));
  assertTrue(isMaglevved(foo));
})();

(function testHoleyDoubleElements2() {
  function foo(a) {
    return a.map(plusOneAndMore);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, , 3];
  const result = foo(array);
  assertTrue(HasHoleyDoubleElements(result));

  %OptimizeMaglevOnNextCall(foo);

  const array2 = [1, 2, , 3];
  const result2 = foo(array2);
  assertTrue(HasHoleyDoubleElements(result2));
  assertTrue(isMaglevved(foo));
})();

(function testPackedElements1() {
  function foo(a) {
    return a.map(plusOneInObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [{a: 1}, {a: 2}, {a: 3}];
  const result = foo(array);
  assertTrue(HasPackedObjectElements(result));

  %OptimizeMaglevOnNextCall(foo);

  const array2 = [{a: 1}, {a: 2}, {a: 3}];
  const result2 = foo(array2);
  assertTrue(HasPackedObjectElements(result2));
  assertTrue(isMaglevved(foo));
})();

(function testPackedElements2() {
  function foo(a) {
    return a.map(wrapInObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, 3];
  const result = foo(array);
  assertTrue(HasPackedObjectElements(result));

  %OptimizeMaglevOnNextCall(foo);

  const array2 = [1, 2, 3];
  const result2 = foo(array2);
  assertTrue(HasPackedObjectElements(result2));
  assertTrue(isMaglevved(foo));
})();

(function testHoleyObjectElements1() {
  function foo(a) {
    return a.map(plusOneInObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [{a: 1}, {a: 2}, , {a: 3}];
  const result = foo(array);
  assertTrue(HasHoleyObjectElements(result));

  %OptimizeMaglevOnNextCall(foo);

  const array2 = [{a: 1}, {a: 2}, , {a: 3}];
  const result2 = foo(array2);
  assertTrue(HasHoleyObjectElements(result2));
  assertTrue(isMaglevved(foo));
})();

(function testHoleyObjectElements2() {
  function foo(a) {
    return a.map(wrapInObject);
  }
  %PrepareFunctionForOptimization(foo);

  const array = [1, 2, , 3];
  const result = foo(array);
  assertTrue(HasHoleyObjectElements(result));

  %OptimizeMaglevOnNextCall(foo);

  const array2 = [1, 2, , 3];
  const result2 = foo(array2);
  assertTrue(HasHoleyObjectElements(result2));
  assertTrue(isMaglevved(foo));
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

  %OptimizeMaglevOnNextCall(foo);

  const result2 = foo(array2);
  assertTrue(HasPackedSmiElements(result2));
  assertTrue(isMaglevved(foo));
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

  %OptimizeMaglevOnNextCall(foo);

  const result2 = foo(array2);
  assertTrue(HasHoleySmiElements(result2));
  assertTrue(isMaglevved(foo));
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

  %OptimizeMaglevOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);
  assertTrue(HasPackedDoubleElements(result2));
  assertTrue(isMaglevved(foo));
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

  %OptimizeMaglevOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);
  assertTrue(HasHoleyDoubleElements(result2));
  assertTrue(isMaglevved(foo));
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

  %OptimizeMaglevOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);
  assertTrue(HasPackedObjectElements(result2));
  assertTrue(isMaglevved(foo));
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

  %OptimizeMaglevOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);
  assertTrue(HasHoleyObjectElements(result2));
  assertTrue(isMaglevved(foo));
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

  %OptimizeMaglevOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);
  assertTrue(HasPackedObjectElements(result2));
  assertTrue(isMaglevved(foo));
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

  %OptimizeMaglevOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);
  assertTrue(HasHoleyObjectElements(result2));
  assertTrue(isMaglevved(foo));
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

  %OptimizeMaglevOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);
  assertTrue(HasPackedObjectElements(result2));
  assertTrue(isMaglevved(foo));
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

  %OptimizeMaglevOnNextCall(foo);
  resetTransitionFunctions();

  const result2 = foo(array2);
  assertTrue(HasHoleyObjectElements(result2));
  assertTrue(isMaglevved(foo));
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
  %OptimizeMaglevOnNextCall(foo);

  const result2 = foo(array);
  assertTrue(HasHoleySmiElements(result2));
  assertEquals(undefined, result2[2]);
  assertTrue(isMaglevved(foo));
})();
