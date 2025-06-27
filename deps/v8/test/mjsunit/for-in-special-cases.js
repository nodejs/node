// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --expose-gc

function for_in_null() {
  try {
    for (var x in null) {
      return false;
    }
  } catch(e) {
    return false;
  }
  return true;
}

function for_in_undefined() {
  try {
    for (var x in undefined) {
      return false;
    }
  } catch(e) {
    return false;
  }
  return true;
}

for (var i = 0; i < 10; ++i) {
  assertTrue(for_in_null());
  gc();
}

for (var j = 0; j < 10; ++j) {
  assertTrue(for_in_undefined());
  gc();
}

assertEquals(10, i);
assertEquals(10, j);


function Accumulate(x) {
  var accumulator = [];
  for (var i in x) {
    accumulator.push(i);
  }
  return accumulator;
}

for (var i = 0; i < 3; ++i) {
  assertEquals(Accumulate("abcd"), ['0', '1', '2', '3']);
}

function for_in_string_prototype() {

  var x = new String("abc");
  x.foo = 19;
  function B() {
    this.bar = 5;
    this[7] = 4;
  }
  B.prototype = x;

  var y = new B();
  y.gub = 13;

  var elements = Accumulate(y);
  var elements1 = Accumulate(y);
  // If for-in returns elements in a different order on multiple calls, this
  // assert will fail.  If that happens, consider if that behavior is OK.
  assertEquals(elements, elements1, "For-in elements not the same both times.");
  assertEquals(["7","bar","gub","0","1","2","foo"], elements)

  assertEquals(['0', '1', '2', 'foo'], Accumulate(x))
}

for_in_string_prototype();
for_in_string_prototype();


(function for_in_dictionary_prototype_1() {
  let prototype1 = {prop: 0, prop1: 1};
  let derived1 = Object.create(null, {
    prop: {enumerable: false, configurable: true, value: 0},
  });
  Object.setPrototypeOf(derived1, prototype1);

  let prototype2 = {prop: 0, prop1: 1};
  let derived2 = Object.create(prototype2, {
    prop: {enumerable: false, configurable: true, value: 0},
  });

  for (let i = 0; i < 3; i++) {
    assertEquals(['prop1'], Accumulate(derived1));
    assertEquals(['prop1'], Accumulate(derived2));
  }
})();

(function for_in_dictionary_prototype_2() {
  let prototype1 = {prop: 0, prop1: 1};
  let derived1 = Object.create(null, {
    prop: {enumerable: false, configurable: true, value: 1},
    prop2: {enumerable: true, configurable: true, value: 2},
    prop3: {enumerable: false, configurable: true, value: 3},
  });
  Object.setPrototypeOf(derived1, prototype1);

  let prototype2 = {prop: 0, prop1: 1};
  let derived2 = Object.create(prototype2, {
    prop: {enumerable: false, configurable: true, value: 0},
    prop2: {enumerable: true, configurable: true, value: 2},
    prop3: {enumerable: false, configurable: true, value: 3},
  });

  for (let i = 0; i < 3; i++) {
    assertEquals(['prop2', 'prop1'], Accumulate(derived1));
    assertEquals(['prop2', 'prop1'], Accumulate(derived2));
  }
})();

(function for_in_prototype_itself_change() {
  let prototype1 = {prop: 0, prop1: 1};
  let derived1 = {prop2: 2, prop3: 3};

  Object.setPrototypeOf(derived1, prototype1);
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop2', 'prop3', 'prop', 'prop1'], Accumulate(derived1));
  }

  prototype1.prop3 = 3;
  let derived2 = {prop4: 4, prop5: 5};
  Object.setPrototypeOf(derived2, prototype1);
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop4', 'prop5', 'prop', 'prop1', 'prop3'], Accumulate(derived2));
  }
})();

(function for_in_prototype_change_property() {
  let prototype1 = {prop: 0, prop1: 1};
  let derived1 = {prop2: 2, prop3: 3};

  Object.setPrototypeOf(derived1, prototype1);
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop2', 'prop3', 'prop', 'prop1'], Accumulate(derived1));
  }

  prototype1.__proto__ = {prop4: 4, prop5: 5};
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop2', 'prop3', 'prop', 'prop1', 'prop4', 'prop5'], Accumulate(derived1));
  }

  derived1.__proto__ = {prop6: 6, prop7: 7};
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop2', 'prop3', 'prop6', 'prop7'], Accumulate(derived1));
  }
})();

(function for_in_prototype_change_element1() {
  let prototype1 = {prop: 0, prop1: 1};
  let derived1 = {prop2: 2, prop3: 3};

  Object.setPrototypeOf(derived1, prototype1);
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop2', 'prop3', 'prop', 'prop1'], Accumulate(derived1));
  }

  prototype1[0] = 4;
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop2', 'prop3', '0', 'prop', 'prop1'], Accumulate(derived1));
  }

  derived1.__proto__ = {1: 1, 3: 3};
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop2', 'prop3', '1', '3'], Accumulate(derived1));
  }
})();

(function for_in_prototype_change_element2() {
  Array.prototype.__proto__ = {'A': 1};
  let array = ['a', 'b', 'c', 'd', 'e'];
  for (let i = 0; i < 3; i++) {
    assertEquals(['0', '1', '2', '3', '4', 'A'], Accumulate(array));
  }
  Array.prototype[10] = 'b';
  for (let i = 0; i < 3; i++) {
    assertEquals(['0', '1', '2', '3', '4', '10', 'A'], Accumulate(array));
  }
})();

(function for_in_prototype_change_element3() {
  let prototype = {prop: 0};
  let holey_array = {
    1: 'a',
    get 3() {
      delete this[5];
      return 'b';
    },
    5: 'c'
  };
  Object.setPrototypeOf(holey_array, prototype);
  for (let i = 0; i < 3; i++) {
    assertEquals(['1', '3', '5', 'prop'], Accumulate(holey_array));
  }
  prototype[10] = 'b';
  for (let i = 0; i < 3; i++) {
    assertEquals(['1', '3', '5', '10', 'prop'], Accumulate(holey_array));
  }
  for (let i = 0; i < 3; i++) {
    var accumulator = [];
    for (var j in holey_array) {
      accumulator.push(j);
      holey_array[j];
    }
    assertEquals(['1', '3', '10', 'prop'], accumulator);
  }
})();

(function for_in_prototype_change_element4() {
  let prototype = {
    1: 'a',
    get 3() {
      delete this[5];
      return 'b';
    },
    5: 'c',
  };
  let holey_array = {7: 'd', 9: 'e'};
  Object.setPrototypeOf(holey_array, prototype);
  for (let i = 0; i < 3; i++) {
    assertEquals(['7', '9', '1', '3', '5'], Accumulate(holey_array));
  }
  prototype.prop = 0;
  for (let i = 0; i < 3; i++) {
    assertEquals(['7', '9', '1', '3', '5', 'prop'], Accumulate(holey_array));
  }
  for (let i = 0; i < 3; i++) {
    var accumulator = [];
    for (var j in holey_array) {
      accumulator.push(j);
      prototype[j];
    }
    assertEquals(['7', '9', '1', '3', 'prop'], accumulator);
  }
})();

(function for_in_non_enumerable1() {
  let prototype1 = {prop: 0};
  let derived1 = Object.create(prototype1, {
    prop1: {enumerable: false, configurable: true, value: 1},
  });
  Object.setPrototypeOf(derived1, prototype1);
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop'], Accumulate(derived1));
  }

  let derived2 = {prop2: 2};
  Object.setPrototypeOf(derived2, prototype1);
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop2', 'prop'], Accumulate(derived2));
  }
})();

(function for_in_non_enumerable2() {
  let prototype1 = {prop: 0};
  let derived1 = {prop1: 1};
  Object.setPrototypeOf(derived1, prototype1);
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop1', 'prop'], Accumulate(derived1));
  }

  let derived2 = Object.create(prototype1, {
    prop: {enumerable: false, configurable: true, value: 0},
    prop2: {enumerable: true, configurable: true, value: 2}
  });
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop2'], Accumulate(derived2));
  }
})();

(function for_in_same_key1() {
  let prototype1 = {prop: 0, prop1: 1};
  let derived1 = {prop: 0, prop2: 1};
  Object.setPrototypeOf(derived1, prototype1);
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop', 'prop2', 'prop1'], Accumulate(derived1));
  }

  let derived2 = {prop3: 3, prop4: 4};
  Object.setPrototypeOf(derived2, prototype1);
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop3', 'prop4', 'prop', 'prop1'], Accumulate(derived2));
  }
})();

(function for_in_same_key2() {
  let prototype1 = {prop: 0, prop1: 1};
  let derived1 = {prop2: 2, prop3: 3};
  Object.setPrototypeOf(derived1, prototype1);
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop2', 'prop3', 'prop', 'prop1'], Accumulate(derived1));
  }

  let derived2 = {prop: 0, prop4: 4};
  Object.setPrototypeOf(derived2, prototype1);
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop', 'prop4', 'prop1'], Accumulate(derived2));
  }
})();

(function for_in_redefine_property() {
  Object.prototype.prop = 0;
  let object1 = {prop1: 1, prop2: 2};
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop1', 'prop2', 'prop'], Accumulate(object1));
  }

  let object2 = {prop3: 3, prop4: 4};
  Object.defineProperty(object2,
    'prop', {enumerable: false, configurable: true, value: 0});
  for (let i = 0; i < 3; i++) {
    assertEquals(['prop3', 'prop4'], Accumulate(object2));
  }
})();

(function for_in_empty_property() {
  let prototype1 = {prop: 0};
  let derived1 = Object.create(prototype1, {
    prop: {enumerable: false, configurable: true, value: 0}
  });
  for (let i = 0; i < 3; i++) {
    assertEquals([], Accumulate(derived1));
  }
})();
