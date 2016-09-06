// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Largely ported from
// https://github.com/tc39/Array.prototype.includes/tree/master/test
// using https://www.npmjs.org/package/test262-to-mjsunit with further edits


// Array.prototype.includes sees a new element added by a getter that is hit
// during iteration
(function() {
  var arrayLike = {
    length: 5,
    0: "a",

    get 1() {
      this[2] = "c";
      return "b";
    }
  };

  assertTrue(Array.prototype.includes.call(arrayLike, "c"));
})();


// Array.prototype.includes works on array-like objects
(function() {
  var arrayLike1 = {
    length: 5,
    0: "a",
    1: "b"
  };

  assertTrue(Array.prototype.includes.call(arrayLike1, "a"));
  assertFalse(Array.prototype.includes.call(arrayLike1, "c"));

  var arrayLike2 = {
    length: 2,
    0: "a",
    1: "b",
    2: "c"
  };

  assertTrue(Array.prototype.includes.call(arrayLike2, "b"));
  assertFalse(Array.prototype.includes.call(arrayLike2, "c"));
})();


// Array.prototype.includes should fail if used on a null or undefined this
(function() {
  assertThrows(function() {
    Array.prototype.includes.call(null, "a");
  }, TypeError);

  assertThrows(function() {
    Array.prototype.includes.call(undefined, "a");
  }, TypeError);
})();


// Array.prototype.includes should terminate if getting an index throws an
// exception
(function() {
  function Test262Error() {}

  var trappedZero = {
    length: 2,

    get 0() {
      throw new Test262Error();
    },

    get 1() {
      assertUnreachable("Should not try to get the first element");
    }
  };

  assertThrows(function() {
    Array.prototype.includes.call(trappedZero, "a");
  }, Test262Error);
})();


// Array.prototype.includes should terminate if ToNumber ends up being called on
// a symbol fromIndex
(function() {
  var trappedZero = {
    length: 1,

    get 0() {
      assertUnreachable("Should not try to get the zeroth element");
    }
  };

  assertThrows(function() {
    Array.prototype.includes.call(trappedZero, "a", Symbol());
  }, TypeError);
})();


// Array.prototype.includes should terminate if an exception occurs converting
// the fromIndex to a number
(function() {
  function Test262Error() {}

  var fromIndex = {
    valueOf: function() {
      throw new Test262Error();
    }
  };

  var trappedZero = {
    length: 1,

    get 0() {
      assertUnreachable("Should not try to get the zeroth element");
    }
  };

  assertThrows(function() {
    Array.prototype.includes.call(trappedZero, "a", fromIndex);
  }, Test262Error);
})();


// Array.prototype.includes should terminate if an exception occurs getting the
// length
(function() {
  function Test262Error() {}

  var fromIndexTrap = {
    valueOf: function() {
      assertUnreachable("Should not try to call ToInteger on valueOf");
    }
  };

  var throwingLength = {
    get length() {
      throw new Test262Error();
    },

    get 0() {
      assertUnreachable("Should not try to get the zeroth element");
    }
  };

  assertThrows(function() {
    Array.prototype.includes.call(throwingLength, "a", fromIndexTrap);
  }, Test262Error);
})();


// Array.prototype.includes should terminate if ToLength ends up being called on
// a symbol length
(function() {
  var fromIndexTrap = {
    valueOf: function() {
      assertUnreachable("Should not try to call ToInteger on valueOf");
    }
  };

  var badLength = {
    length: Symbol(),

    get 0() {
      assertUnreachable("Should not try to get the zeroth element");
    }
  };

  assertThrows(function() {
    Array.prototype.includes.call(badLength, "a", fromIndexTrap);
  }, TypeError);
})();


// Array.prototype.includes should terminate if an exception occurs converting
// the length to a number
(function() {
  function Test262Error() {}

  var fromIndexTrap = {
    valueOf: function() {
      assertUnreachable("Should not try to call ToInteger on valueOf");
    }
  };

  var badLength = {
    length: {
      valueOf: function() {
        throw new Test262Error();
      }
    },

    get 0() {
      assertUnreachable("Should not try to get the zeroth element");
    }
  };

  assertThrows(function() {
    Array.prototype.includes.call(badLength, "a", fromIndexTrap);
  }, Test262Error);
})();


// Array.prototype.includes should search the whole array, as the optional
// second argument fromIndex defaults to 0
(function() {
  assertTrue([10, 11].includes(10));
  assertTrue([10, 11].includes(11));

  var arrayLike = {
    length: 2,

    get 0() {
      return "1";
    },

    get 1() {
      return "2";
    }
  };

  assertTrue(Array.prototype.includes.call(arrayLike, "1"));
  assertTrue(Array.prototype.includes.call(arrayLike, "2"));
})();


// Array.prototype.includes returns false if fromIndex is greater or equal to
// the length of the array
(function() {
  assertFalse([1, 2].includes(2, 3));
  assertFalse([1, 2].includes(2, 2));

  var arrayLikeWithTrap = {
    length: 2,

    get 0() {
      assertUnreachable("Getter for 0 was called");
    },

    get 1() {
      assertUnreachable("Getter for 1 was called");
    }
  };

  assertFalse(Array.prototype.includes.call(arrayLikeWithTrap, "c", 2));
  assertFalse(Array.prototype.includes.call(arrayLikeWithTrap, "c", 3));
})();


// Array.prototype.includes searches the whole array if the computed index from
// the given negative fromIndex argument is less than 0
(function() {
  assertTrue([1, 3].includes(1, -4));
  assertTrue([1, 3].includes(3, -4));

  var arrayLike = {
    length: 2,
    0: "a",

    get 1() {
      return "b";
    },

    get "-1"() {
      assertUnreachable("Should not try to get the element at index -1");
    }
  };

  assertTrue(Array.prototype.includes.call(arrayLike, "a", -4));
  assertTrue(Array.prototype.includes.call(arrayLike, "b", -4));
})();


// Array.prototype.includes should use a negative value as the offset from the
// end of the array to compute fromIndex
(function() {
  assertTrue([12, 13].includes(13, -1));
  assertFalse([12, 13].includes(12, -1));
  assertTrue([12, 13].includes(12, -2));

  var arrayLike = {
    length: 2,

    get 0() {
      return "a";
    },

    get 1() {
      return "b";
    }
  };

  assertTrue(Array.prototype.includes.call(arrayLike, "b", -1));
  assertFalse(Array.prototype.includes.call(arrayLike, "a", -1));
  assertTrue(Array.prototype.includes.call(arrayLike, "a", -2));
})();


// Array.prototype.includes converts its fromIndex parameter to an integer
(function() {
  assertFalse(["a", "b"].includes("a", 2.3));

  var arrayLikeWithTraps = {
    length: 2,

    get 0() {
      assertUnreachable("Getter for 0 was called");
    },

    get 1() {
      assertUnreachable("Getter for 1 was called");
    }
  };

  assertFalse(Array.prototype.includes.call(arrayLikeWithTraps, "c", 2.1));
  assertFalse(Array.prototype.includes.call(arrayLikeWithTraps, "c", +Infinity));
  assertTrue(["a", "b", "c"].includes("a", -Infinity));
  assertTrue(["a", "b", "c"].includes("c", 2.9));
  assertTrue(["a", "b", "c"].includes("c", NaN));

  var arrayLikeWithTrapAfterZero = {
    length: 2,

    get 0() {
      return "a";
    },

    get 1() {
      assertUnreachable("Getter for 1 was called");
    }
  };

  assertTrue(Array.prototype.includes.call(arrayLikeWithTrapAfterZero, "a", NaN));

  var numberLike = {
    valueOf: function() {
      return 2;
    }
  };

  assertFalse(["a", "b", "c"].includes("a", numberLike));
  assertFalse(["a", "b", "c"].includes("a", "2"));
  assertTrue(["a", "b", "c"].includes("c", numberLike));
  assertTrue(["a", "b", "c"].includes("c", "2"));
})();


// Array.prototype.includes should have length 1
(function() {
  assertEquals(1, Array.prototype.includes.length);
})();


// Array.prototype.includes should have name property with value 'includes'
(function() {
  assertEquals("includes", Array.prototype.includes.name);
})();


// !!! Test failed to convert:
// Cannot convert tests with includes.
// !!!


// Array.prototype.includes does not skip holes; if the array has a prototype it
// gets from that
(function() {
  var holesEverywhere = [,,,];

  holesEverywhere.__proto__ = {
    1: "a"
  };

  holesEverywhere.__proto__.__proto__ = Array.prototype;
  assertTrue(holesEverywhere.includes("a"));
  var oneHole = ["a", "b",, "d"];

  oneHole.__proto__ = {
    get 2() {
      return "c";
    }
  };

  assertTrue(Array.prototype.includes.call(oneHole, "c"));
})();


// Array.prototype.includes does not skip holes; instead it treates them as
// undefined
(function() {
  assertTrue([,,,].includes(undefined));
  assertTrue(["a", "b",, "d"].includes(undefined));
})();


// Array.prototype.includes gets length property from the prototype if it's
// available
(function() {
  var proto = {
    length: 1
  };

  var arrayLike = Object.create(proto);
  arrayLike[0] = "a";

  Object.defineProperty(arrayLike, "1", {
    get: function() {
      assertUnreachable("Getter for 1 was called");
    }
  });

  assertTrue(Array.prototype.includes.call(arrayLike, "a"));
})();


// Array.prototype.includes treats a missing length property as zero
(function() {
  var arrayLikeWithTraps = {
    get 0() {
      assertUnreachable("Getter for 0 was called");
    },

    get 1() {
      assertUnreachable("Getter for 1 was called");
    }
  };

  assertFalse(Array.prototype.includes.call(arrayLikeWithTraps, "a"));
})();


// Array.prototype.includes should always return false on negative-length
// objects
(function() {
  assertFalse(Array.prototype.includes.call({
    length: -1
  }, 2));

  assertFalse(Array.prototype.includes.call({
    length: -2
  }));

  assertFalse(Array.prototype.includes.call({
    length: -Infinity
  }, undefined));

  assertFalse(Array.prototype.includes.call({
    length: -Math.pow(2, 53)
  }, NaN));

  assertFalse(Array.prototype.includes.call({
    length: -1,
    "-1": 2
  }, 2));

  assertFalse(Array.prototype.includes.call({
    length: -3,
    "-1": 2
  }, 2));

  assertFalse(Array.prototype.includes.call({
    length: -Infinity,
    "-1": 2
  }, 2));

  var arrayLikeWithTrap = {
    length: -1,

    get 0() {
      assertUnreachable("Getter for 0 was called");
    }
  };

  assertFalse(Array.prototype.includes.call(arrayLikeWithTrap, 2));
})();


// Array.prototype.includes should clamp positive lengths to 2^53 - 1
(function() {
  var fromIndexForLargeIndexTests = 9007199254740990;

  assertFalse(Array.prototype.includes.call({
    length: 1
  }, 2));

  assertTrue(Array.prototype.includes.call({
    length: 1,
    0: "a"
  }, "a"));

  assertTrue(Array.prototype.includes.call({
    length: +Infinity,
    0: "a"
  }, "a"));

  assertFalse(Array.prototype.includes.call({
    length: +Infinity
  }, "a", fromIndexForLargeIndexTests));

  var arrayLikeWithTrap = {
    length: +Infinity,

    get 9007199254740992() {
      assertUnreachable("Getter for 9007199254740992 (i.e. 2^53) was called");
    },

    "9007199254740993": "a"
  };

  assertFalse(
    Array.prototype.includes.call(arrayLikeWithTrap, "a", fromIndexForLargeIndexTests)
  );

  var arrayLikeWithTooBigLength = {
    length: 9007199254740996,
    "9007199254740992": "a"
  };

  assertFalse(
    Array.prototype.includes.call(arrayLikeWithTooBigLength, "a", fromIndexForLargeIndexTests)
  );
})();


// Array.prototype.includes should always return false on zero-length objects
(function() {
  assertFalse([].includes(2));
  assertFalse([].includes());
  assertFalse([].includes(undefined));
  assertFalse([].includes(NaN));

  assertFalse(Array.prototype.includes.call({
    length: 0
  }, 2));

  assertFalse(Array.prototype.includes.call({
    length: 0
  }));

  assertFalse(Array.prototype.includes.call({
    length: 0
  }, undefined));

  assertFalse(Array.prototype.includes.call({
    length: 0
  }, NaN));

  assertFalse(Array.prototype.includes.call({
    length: 0,
    0: 2
  }, 2));

  assertFalse(Array.prototype.includes.call({
    length: 0,
    0: undefined
  }));

  assertFalse(Array.prototype.includes.call({
    length: 0,
    0: undefined
  }, undefined));

  assertFalse(Array.prototype.includes.call({
    length: 0,
    0: NaN
  }, NaN));

  var arrayLikeWithTrap = {
    length: 0,

    get 0() {
      assertUnreachable("Getter for 0 was called");
    }
  };

  Array.prototype.includes.call(arrayLikeWithTrap);

  var trappedFromIndex = {
    valueOf: function() {
      assertUnreachable("Should not try to convert fromIndex to a number on a zero-length array");
    }
  };

  [].includes("a", trappedFromIndex);

  Array.prototype.includes.call({
    length: 0
  }, trappedFromIndex);
})();


// Array.prototype.includes works on objects
(function() {
  assertFalse(["a", "b", "c"].includes({}));
  assertFalse([{}, {}].includes({}));
  var obj = {};
  assertTrue([obj].includes(obj));
  assertFalse([obj].includes(obj, 1));
  assertTrue([obj, obj].includes(obj, 1));

  var stringyObject = {
    toString: function() {
      return "a";
    }
  };

  assertFalse(["a", "b", obj].includes(stringyObject));
})();


// Array.prototype.includes does not see an element removed by a getter that is
// hit during iteration
(function() {
  var arrayLike = {
    length: 5,
    0: "a",

    get 1() {
      delete this[2];
      return "b";
    },

    2: "c"
  };

  assertFalse(Array.prototype.includes.call(arrayLike, "c"));
})();


// Array.prototype.includes should use the SameValueZero algorithm to compare
(function() {
  assertTrue([1, 2, 3].includes(2));
  assertFalse([1, 2, 3].includes(4));
  assertTrue([1, 2, NaN].includes(NaN));
  assertTrue([1, 2, -0].includes(+0));
  assertTrue([1, 2, -0].includes(-0));
  assertTrue([1, 2, +0].includes(-0));
  assertTrue([1, 2, +0].includes(+0));
  assertFalse([1, 2, -Infinity].includes(+Infinity));
  assertTrue([1, 2, -Infinity].includes(-Infinity));
  assertFalse([1, 2, +Infinity].includes(-Infinity));
  assertTrue([1, 2, +Infinity].includes(+Infinity));
})();


// Array.prototype.includes stops once it hits the length of an array-like, even
// if there are more after
(function() {
  var arrayLike = {
    length: 2,
    0: "a",
    1: "b",

    get 2() {
      assertUnreachable("Should not try to get the second element");
    }
  };

  assertFalse(Array.prototype.includes.call(arrayLike, "c"));
})();


// Array.prototype.includes works on typed arrays
(function() {
  assertTrue(Array.prototype.includes.call(new Uint8Array([1, 2, 3]), 2));

  assertTrue(
    Array.prototype.includes.call(new Float32Array([2.5, 3.14, Math.PI]), 3.1415927410125732)
  );

  assertFalse(Array.prototype.includes.call(new Uint8Array([1, 2, 3]), 4));
  assertFalse(Array.prototype.includes.call(new Uint8Array([1, 2, 3]), 2, 2));
})();


(function testUnscopable() {
  assertTrue(Array.prototype[Symbol.unscopables].includes);
})();
