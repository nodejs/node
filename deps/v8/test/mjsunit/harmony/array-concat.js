// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-arrays --harmony-classes

(function testArrayConcatArity() {
  "use strict";
  assertEquals(1, Array.prototype.concat.length);
})();


(function testArrayConcatNoPrototype() {
  "use strict";
  assertEquals(void 0, Array.prototype.concat.prototype);
})();


(function testArrayConcatDescriptor() {
  "use strict";
  var desc = Object.getOwnPropertyDescriptor(Array.prototype, 'concat');
  assertEquals(false, desc.enumerable);
})();


(function testConcatArrayLike() {
  "use strict";
  var obj = {
    "length": 6,
    "1": "A",
    "3": "B",
    "5": "C"
  };
  obj[Symbol.isConcatSpreadable] = true;
  var obj2 = { length: 3, "0": "0", "1": "1", "2": "2" };
  var arr = ["X", "Y", "Z"];
  assertEquals([void 0, "A", void 0, "B", void 0, "C",
               { "length": 3, "0": "0", "1": "1", "2": "2" },
               "X", "Y", "Z"], Array.prototype.concat.call(obj, obj2, arr));
})();


(function testConcatArrayLikeStringLength() {
  "use strict";
  var obj = {
    "length": "6",
    "1": "A",
    "3": "B",
    "5": "C"
  };
  obj[Symbol.isConcatSpreadable] = true;
  var obj2 = { length: 3, "0": "0", "1": "1", "2": "2" };
  var arr = ["X", "Y", "Z"];
  assertEquals([void 0, "A", void 0, "B", void 0, "C",
               { "length": 3, "0": "0", "1": "1", "2": "2" },
               "X", "Y", "Z"], Array.prototype.concat.call(obj, obj2, arr));
})();


(function testConcatArrayLikeNegativeLength() {
  "use strict";
  var obj = {
    "length": -6,
    "1": "A",
    "3": "B",
    "5": "C"
  };
  obj[Symbol.isConcatSpreadable] = true;
  assertEquals([], [].concat(obj));
  obj.length = -6.7;
  assertEquals([], [].concat(obj));
  obj.length = "-6";
  assertEquals([], [].concat(obj));
})();


(function testConcatArrayLikeToLengthThrows() {
  "use strict";
  var obj = {
    "length": {valueOf: null, toString: null},
    "1": "A",
    "3": "B",
    "5": "C"
  };
  obj[Symbol.isConcatSpreadable] = true;
  var obj2 = { length: 3, "0": "0", "1": "1", "2": "2" };
  var arr = ["X", "Y", "Z"];
  assertThrows(function() {
    Array.prototype.concat.call(obj, obj2, arr);
  }, TypeError);
})();


(function testConcatArrayLikePrimitiveNonNumberLength() {
  "use strict";
  var obj = {
    "1": "A",
    "3": "B",
    "5": "C"
  };
  obj[Symbol.isConcatSpreadable] = true;
  obj.length = {toString: function() { return "SIX"; }, valueOf: null };
  assertEquals([], [].concat(obj));
  obj.length = {toString: null, valueOf: function() { return "SIX"; } };
  assertEquals([], [].concat(obj));
})();


(function testConcatArrayLikeLengthToStringThrows() {
  "use strict";
  function MyError() {}
  var obj = {
    "length": { toString: function() {
        throw new MyError();
      }, valueOf: null
    },
    "1": "A",
    "3": "B",
    "5": "C"
  };
  obj[Symbol.isConcatSpreadable] = true;
  assertThrows(function() {
    [].concat(obj);
  }, MyError);
})();


(function testConcatArrayLikeLengthValueOfThrows() {
  "use strict";
  function MyError() {}
  var obj = {
    "length": { valueOf: function() {
      throw new MyError();
    }, toString: null
  },
  "1": "A",
  "3": "B",
  "5": "C"
};
obj[Symbol.isConcatSpreadable] = true;
assertThrows(function() {
  [].concat(obj);
}, MyError);
})();


(function testConcatHoleyArray() {
  "use strict";
  var arr = [];
  arr[4] = "Item 4";
  arr[8] = "Item 8";
  var arr2 = [".", "!", "?"];
  assertEquals([void 0, void 0, void 0, void 0, "Item 4", void 0, void 0,
                void 0, "Item 8", ".", "!", "?"], arr.concat(arr2));
})();


(function testIsConcatSpreadableGetterThrows() {
  "use strict";
  function MyError() {}
  var obj = {};
  Object.defineProperty(obj, Symbol.isConcatSpreadable, {
    get: function() { throw new MyError(); }
  });

  assertThrows(function() {
    [].concat(obj);
  }, MyError);

  assertThrows(function() {
    Array.prototype.concat.call(obj, 1, 2, 3);
  }, MyError);
})();


(function testConcatLengthThrows() {
  "use strict";
  function MyError() {}
  var obj = {};
  obj[Symbol.isConcatSpreadable] = true;
  Object.defineProperty(obj, "length", {
    get: function() { throw new MyError(); }
  });

  assertThrows(function() {
    [].concat(obj);
  }, MyError);

  assertThrows(function() {
    Array.prototype.concat.call(obj, 1, 2, 3);
  }, MyError);
})();


(function testConcatArraySubclass() {
  "use strict";
  // TODO(caitp): when concat is called on instances of classes which extend
  // Array, they should:
  //
  // - return an instance of the class, rather than an Array instance (if from
  //   same Realm)
  // - always treat such classes as concat-spreadable
})();


(function testConcatNonArray() {
  "use strict";
  class NonArray {
    constructor() { Array.apply(this, arguments); }
  };

  var obj = new NonArray(1,2,3);
  var result = Array.prototype.concat.call(obj, 4, 5, 6);
  assertEquals(Array, result.constructor);
  assertEquals([obj,4,5,6], result);
  assertFalse(result instanceof NonArray);
})();


function testConcatTypedArray(type, elems, modulo) {
  "use strict";
  var items = new Array(elems);
  var ta_by_len = new type(elems);
  for (var i = 0; i < elems; ++i) {
    ta_by_len[i] = items[i] = modulo === false ? i : elems % modulo;
  }
  var ta = new type(items);
  assertEquals([ta, ta], [].concat(ta, ta));
  ta[Symbol.isConcatSpreadable] = true;
  assertEquals(items, [].concat(ta));

  assertEquals([ta_by_len, ta_by_len], [].concat(ta_by_len, ta_by_len));
  ta_by_len[Symbol.isConcatSpreadable] = true;
  assertEquals(items, [].concat(ta_by_len));

  // TypedArray with fake `length`.
  ta = new type(1);
  var defValue = ta[0];
  var expected = new Array(4000);
  expected[0] = defValue;

  Object.defineProperty(ta, "length", { value: 4000 });
  ta[Symbol.isConcatSpreadable] = true;
  assertEquals(expected, [].concat(ta));
}

(function testConcatSmallTypedArray() {
  var max = [2^8, 2^16, 2^32, false, false];
  [
    Uint8Array,
    Uint16Array,
    Uint32Array,
    Float32Array,
    Float64Array
  ].forEach(function(ctor, i) {
    testConcatTypedArray(ctor, 1, max[i]);
  });
})();


(function testConcatLargeTypedArray() {
  var max = [2^8, 2^16, 2^32, false, false];
  [
    Uint8Array,
    Uint16Array,
    Uint32Array,
    Float32Array,
    Float64Array
  ].forEach(function(ctor, i) {
    testConcatTypedArray(ctor, 4000, max[i]);
  });
})();


(function testConcatStrictArguments() {
  var args = (function(a, b, c) { "use strict"; return arguments; })(1,2,3);
  args[Symbol.isConcatSpreadable] = true;
  assertEquals([1, 2, 3, 1, 2, 3], [].concat(args, args));

  Object.defineProperty(args, "length", { value: 6 });
  assertEquals([1, 2, 3, void 0, void 0, void 0], [].concat(args));
})();


(function testConcatSloppyArguments() {
  var args = (function(a, b, c) { return arguments; })(1,2,3);
  args[Symbol.isConcatSpreadable] = true;
  assertEquals([1, 2, 3, 1, 2, 3], [].concat(args, args));

  Object.defineProperty(args, "length", { value: 6 });
  assertEquals([1, 2, 3, void 0, void 0, void 0], [].concat(args));
})();


(function testConcatSloppyArgumentsWithDupes() {
  var args = (function(a, a, a) { return arguments; })(1,2,3);
  args[Symbol.isConcatSpreadable] = true;
  assertEquals([1, 2, 3, 1, 2, 3], [].concat(args, args));

  Object.defineProperty(args, "length", { value: 6 });
  assertEquals([1, 2, 3, void 0, void 0, void 0], [].concat(args));
})();


(function testConcatSloppyArgumentsThrows() {
  function MyError() {}
  var args = (function(a) { return arguments; })(1,2,3);
  Object.defineProperty(args, 0, {
    get: function() { throw new MyError(); }
  });
  args[Symbol.isConcatSpreadable] = true;
  assertThrows(function() {
    return [].concat(args, args);
  }, MyError);
})();


(function testConcatHoleySloppyArguments() {
  var args = (function(a) { return arguments; })(1,2,3);
  delete args[1];
  args[Symbol.isConcatSpreadable] = true;
  assertEquals([1, void 0, 3, 1, void 0, 3], [].concat(args, args));
})();


(function testConcatSpreadableStringWrapper() {
  "use strict";
  var str1 = new String("yuck\uD83D\uDCA9")
  // String wrapper objects are not concat-spreadable by default
  assertEquals([str1], [].concat(str1));

  // String wrapper objects may be individually concat-spreadable
  str1[Symbol.isConcatSpreadable] = true;
  assertEquals(["y", "u", "c", "k", "\uD83D", "\uDCA9"],
               [].concat(str1));

  String.prototype[Symbol.isConcatSpreadable] = true;
  // String wrapper objects may be concat-spreadable
  assertEquals(["y", "u", "c", "k", "\uD83D", "\uDCA9"],
               [].concat(new String("yuck\uD83D\uDCA9")));

  // String values are never concat-spreadable
  assertEquals(["yuck\uD83D\uDCA9"], [].concat("yuck\uD83D\uDCA9"));
  delete String.prototype[Symbol.isConcatSpreadable];
})();


(function testConcatSpreadableBooleanWrapper() {
  "use strict";
  var bool = new Boolean(true)
  // Boolean wrapper objects are not concat-spreadable by default
  assertEquals([bool], [].concat(bool));

  // Boolean wrapper objects may be individually concat-spreadable
  bool[Symbol.isConcatSpreadable] = true;
  bool.length = 3;
  bool[0] = 1, bool[1] = 2, bool[2] = 3;
  assertEquals([1, 2, 3], [].concat(bool));

  Boolean.prototype[Symbol.isConcatSpreadable] = true;
  // Boolean wrapper objects may be concat-spreadable
  assertEquals([], [].concat(new Boolean(true)));
  Boolean.prototype[0] = 1;
  Boolean.prototype[1] = 2;
  Boolean.prototype[2] = 3;
  Boolean.prototype.length = 3;
  assertEquals([1,2,3], [].concat(new Boolean(true)));

  // Boolean values are never concat-spreadable
  assertEquals([true], [].concat(true));
  delete Boolean.prototype[Symbol.isConcatSpreadable];
  delete Boolean.prototype[0];
  delete Boolean.prototype[1];
  delete Boolean.prototype[2];
  delete Boolean.prototype.length;
})();


(function testConcatSpreadableNumberWrapper() {
  "use strict";
  var num = new Number(true)
  // Number wrapper objects are not concat-spreadable by default
  assertEquals([num], [].concat(num));

  // Number wrapper objects may be individually concat-spreadable
  num[Symbol.isConcatSpreadable] = true;
  num.length = 3;
  num[0] = 1, num[1] = 2, num[2] = 3;
  assertEquals([1, 2, 3], [].concat(num));

  Number.prototype[Symbol.isConcatSpreadable] = true;
  // Number wrapper objects may be concat-spreadable
  assertEquals([], [].concat(new Number(123)));
  Number.prototype[0] = 1;
  Number.prototype[1] = 2;
  Number.prototype[2] = 3;
  Number.prototype.length = 3;
  assertEquals([1,2,3], [].concat(new Number(123)));

  // Number values are never concat-spreadable
  assertEquals([true], [].concat(true));
  delete Number.prototype[Symbol.isConcatSpreadable];
  delete Number.prototype[0];
  delete Number.prototype[1];
  delete Number.prototype[2];
  delete Number.prototype.length;
})();


(function testConcatSpreadableFunction() {
  "use strict";
  var fn = function(a, b, c) {}
  // Functions are not concat-spreadable by default
  assertEquals([fn], [].concat(fn));

  // Functions may be individually concat-spreadable
  fn[Symbol.isConcatSpreadable] = true;
  fn[0] = 1, fn[1] = 2, fn[2] = 3;
  assertEquals([1, 2, 3], [].concat(fn));

  Function.prototype[Symbol.isConcatSpreadable] = true;
  // Functions may be concat-spreadable
  assertEquals([void 0, void 0, void 0], [].concat(function(a,b,c) {}));
  Function.prototype[0] = 1;
  Function.prototype[1] = 2;
  Function.prototype[2] = 3;
  assertEquals([1,2,3], [].concat(function(a, b, c) {}));

  delete Function.prototype[Symbol.isConcatSpreadable];
  delete Function.prototype[0];
  delete Function.prototype[1];
  delete Function.prototype[2];
})();


(function testConcatSpreadableRegExp() {
  "use strict";
  var re = /abc/;
  // RegExps are not concat-spreadable by default
  assertEquals([re], [].concat(re));

  // RegExps may be individually concat-spreadable
  re[Symbol.isConcatSpreadable] = true;
  re[0] = 1, re[1] = 2, re[2] = 3, re.length = 3;
  assertEquals([1, 2, 3], [].concat(re));

  // RegExps may be concat-spreadable
  RegExp.prototype[Symbol.isConcatSpreadable] = true;
  RegExp.prototype.length = 3;

  assertEquals([void 0, void 0, void 0], [].concat(/abc/));
  RegExp.prototype[0] = 1;
  RegExp.prototype[1] = 2;
  RegExp.prototype[2] = 3;
  assertEquals([1,2,3], [].concat(/abc/));

  delete RegExp.prototype[Symbol.isConcatSpreadable];
  delete RegExp.prototype[0];
  delete RegExp.prototype[1];
  delete RegExp.prototype[2];
  delete RegExp.prototype.length;
})();


(function testArrayConcatSpreadableSparseObject() {
  "use strict";
  var obj = { length: 5 };
  obj[Symbol.isConcatSpreadable] = true;
  assertEquals([void 0, void 0, void 0, void 0, void 0], [].concat(obj));

  obj.length = 4000;
  assertEquals(new Array(4000), [].concat(obj));
})();


// ES5 tests
(function testArrayConcatES5() {
  "use strict";
  var poses;
  var pos;

  poses = [140, 4000000000];
  while (pos = poses.shift()) {
    var a = new Array(pos);
    var array_proto = [];
    a.__proto__ = array_proto;
    assertEquals(pos, a.length);
    a.push('foo');
    assertEquals(pos + 1, a.length);
    var b = ['bar'];
    var c = a.concat(b);
    assertEquals(pos + 2, c.length);
    assertEquals("undefined", typeof(c[pos - 1]));
    assertEquals("foo", c[pos]);
    assertEquals("bar", c[pos + 1]);

    // Can we fool the system by putting a number in a string?
    var onetwofour = "124";
    a[onetwofour] = 'doo';
    assertEquals(a[124], 'doo');
    c = a.concat(b);
    assertEquals(c[124], 'doo');

    // If we put a number in the prototype, then the spec says it should be
    // copied on concat.
    array_proto["123"] = 'baz';
    assertEquals(a[123], 'baz');

    c = a.concat(b);
    assertEquals(pos + 2, c.length);
    assertEquals("baz", c[123]);
    assertEquals("undefined", typeof(c[pos - 1]));
    assertEquals("foo", c[pos]);
    assertEquals("bar", c[pos + 1]);

    // When we take the number off the prototype it disappears from a, but
    // the concat put it in c itself.
    array_proto["123"] = undefined;
    assertEquals("undefined", typeof(a[123]));
    assertEquals("baz", c[123]);

    // If the element of prototype is shadowed, the element on the instance
    // should be copied, but not the one on the prototype.
    array_proto[123] = 'baz';
    a[123] = 'xyz';
    assertEquals('xyz', a[123]);
    c = a.concat(b);
    assertEquals('xyz', c[123]);

    // Non-numeric properties on the prototype or the array shouldn't get
    // copied.
    array_proto.moe = 'joe';
    a.ben = 'jerry';
    assertEquals(a["moe"], 'joe');
    assertEquals(a["ben"], 'jerry');
    c = a.concat(b);
    // ben was not copied
    assertEquals("undefined", typeof(c.ben));

    // When we take moe off the prototype it disappears from all arrays.
    array_proto.moe = undefined;
    assertEquals("undefined", typeof(c.moe));

    // Negative indices don't get concated.
    a[-1] = 'minus1';
    assertEquals("minus1", a[-1]);
    assertEquals("undefined", typeof(a[0xffffffff]));
    c = a.concat(b);
    assertEquals("undefined", typeof(c[-1]));
    assertEquals("undefined", typeof(c[0xffffffff]));
    assertEquals(c.length, a.length + 1);
  }

  poses = [140, 4000000000];
  while (pos = poses.shift()) {
    var a = new Array(pos);
    assertEquals(pos, a.length);
    a.push('foo');
    assertEquals(pos + 1, a.length);
    var b = ['bar'];
    var c = a.concat(b);
    assertEquals(pos + 2, c.length);
    assertEquals("undefined", typeof(c[pos - 1]));
    assertEquals("foo", c[pos]);
    assertEquals("bar", c[pos + 1]);

    // Can we fool the system by putting a number in a string?
    var onetwofour = "124";
    a[onetwofour] = 'doo';
    assertEquals(a[124], 'doo');
    c = a.concat(b);
    assertEquals(c[124], 'doo');

    // If we put a number in the prototype, then the spec says it should be
    // copied on concat.
    Array.prototype["123"] = 'baz';
    assertEquals(a[123], 'baz');

    c = a.concat(b);
    assertEquals(pos + 2, c.length);
    assertEquals("baz", c[123]);
    assertEquals("undefined", typeof(c[pos - 1]));
    assertEquals("foo", c[pos]);
    assertEquals("bar", c[pos + 1]);

    // When we take the number off the prototype it disappears from a, but
    // the concat put it in c itself.
    Array.prototype["123"] = undefined;
    assertEquals("undefined", typeof(a[123]));
    assertEquals("baz", c[123]);

    // If the element of prototype is shadowed, the element on the instance
    // should be copied, but not the one on the prototype.
    Array.prototype[123] = 'baz';
    a[123] = 'xyz';
    assertEquals('xyz', a[123]);
    c = a.concat(b);
    assertEquals('xyz', c[123]);

    // Non-numeric properties on the prototype or the array shouldn't get
    // copied.
    Array.prototype.moe = 'joe';
    a.ben = 'jerry';
    assertEquals(a["moe"], 'joe');
    assertEquals(a["ben"], 'jerry');
    c = a.concat(b);
    // ben was not copied
    assertEquals("undefined", typeof(c.ben));
    // moe was not copied, but we can see it through the prototype
    assertEquals("joe", c.moe);

    // When we take moe off the prototype it disappears from all arrays.
    Array.prototype.moe = undefined;
    assertEquals("undefined", typeof(c.moe));

    // Negative indices don't get concated.
    a[-1] = 'minus1';
    assertEquals("minus1", a[-1]);
    assertEquals("undefined", typeof(a[0xffffffff]));
    c = a.concat(b);
    assertEquals("undefined", typeof(c[-1]));
    assertEquals("undefined", typeof(c[0xffffffff]));
    assertEquals(c.length, a.length + 1);

  }

  a = [];
  c = a.concat('Hello');
  assertEquals(1, c.length);
  assertEquals("Hello", c[0]);
  assertEquals("Hello", c.toString());

  // Check that concat preserves holes.
  var holey = [void 0,'a',,'c'].concat(['d',,'f',[0,,2],void 0])
  assertEquals(9, holey.length);  // hole in embedded array is ignored
  for (var i = 0; i < holey.length; i++) {
    if (i == 2 || i == 5) {
      assertFalse(i in holey);
    } else {
      assertTrue(i in holey);
    }
  }

  // Polluted prototype from prior tests.
  delete Array.prototype[123];

  // Check that concat reads getters in the correct order.
  var arr1 = [,2];
  var arr2 = [1,3];
  var r1 = [].concat(arr1, arr2);  // [,2,1,3]
  assertEquals([,2,1,3], r1);

  // Make first array change length of second array.
  Object.defineProperty(arr1, 0, {get: function() {
        arr2.push("X");
        return undefined;
      }, configurable: true})
  var r2 = [].concat(arr1, arr2);  // [undefined,2,1,3,"X"]
  assertEquals([undefined,2,1,3,"X"], r2);

  // Make first array change length of second array massively.
  arr2.length = 2;
  Object.defineProperty(arr1, 0, {get: function() {
        arr2[500000] = "X";
        return undefined;
      }, configurable: true})
  var r3 = [].concat(arr1, arr2);  // [undefined,2,1,3,"X"]
  var expected = [undefined,2,1,3];
  expected[500000 + 2] = "X";

  assertEquals(expected, r3);

  var arr3 = [];
  var trace = [];
  var expectedTrace = []
  function mkGetter(i) { return function() { trace.push(i); }; }
  arr3.length = 10000;
  for (var i = 0; i < 100; i++) {
    Object.defineProperty(arr3, i * i, {get: mkGetter(i)});
    expectedTrace[i] = i;
    expectedTrace[100 + i] = i;
  }
  var r4 = [0].concat(arr3, arr3);
  assertEquals(1 + arr3.length * 2, r4.length);
  assertEquals(expectedTrace, trace);
})();
