// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-iterator-helpers

function* gen() {
  yield 42;
  yield 43;
}

function* longerGen() {
  yield 42;
  yield 43;
  yield 44;
  yield 45;
}

function TestHelperPrototypeSurface(helper) {
  const proto = Object.getPrototypeOf(helper);
  assertEquals('Iterator Helper', proto[Symbol.toStringTag]);
  assertTrue(Object.hasOwn(proto, 'next'));
  assertTrue(Object.hasOwn(proto, 'return'));
  assertEquals('function', typeof proto.next);
  assertEquals('function', typeof proto.return);
  assertEquals(0, proto.next.length);
  assertEquals(0, proto.return.length);
  assertEquals('next', proto.next.name);
  assertEquals('return', proto.return.name);
}

// --- Test Map helper

(function TestMap() {
  const iter = gen();
  assertEquals('function', typeof iter.map);
  assertEquals(1, iter.map.length);
  assertEquals('map', iter.map.name);
  let counters = [];
  const mapIter = iter.map((x, i) => {
    counters.push(i);
    return x*2;
  });
  TestHelperPrototypeSurface(mapIter);
  assertEquals({value: 84, done: false }, mapIter.next());
  assertEquals({value: 86, done: false }, mapIter.next());
  assertEquals([0,1], counters);
  assertEquals({value: undefined, done: true }, mapIter.next());
})();

// --- Test Filter helper

(function TestFilter() {
  const iter = gen();
  assertEquals('function', typeof iter.filter);
  assertEquals(1, iter.filter.length);
  assertEquals('filter', iter.filter.name);
  const filterIter = iter.filter((x, i) => {
    return x%2 == 0;
  });
  TestHelperPrototypeSurface(filterIter);
  assertEquals({value: 42, done: false }, filterIter.next());
  assertEquals({value: undefined, done: true }, filterIter.next());
})();

(function TestFilterLastElement() {
  const iter = gen();
  const filterIter = iter.filter((x, i) => {
    return x == 43;
  });
  TestHelperPrototypeSurface(filterIter);
  assertEquals({value: 43, done: false }, filterIter.next());
  assertEquals({value: undefined, done: true }, filterIter.next());
})();

(function TestFilterAllElement() {
  const iter = gen();
  const filterIter = iter.filter((x, i) => {
    return x == x;
  });
  TestHelperPrototypeSurface(filterIter);
  assertEquals({value: 42, done: false }, filterIter.next());
  assertEquals({value: 43, done: false }, filterIter.next());
  assertEquals({value: undefined, done: true }, filterIter.next());
})();

(function TestFilterNoElement() {
  const iter = gen();
  const filterIter = iter.filter((x, i) => {
    return x == 0;
  });
  TestHelperPrototypeSurface(filterIter);
  assertEquals({value: undefined, done: true }, filterIter.next());
})();

// --- Test Take helper

(function TestTake() {
  const iter = gen();
  assertEquals('function', typeof iter.take);
  assertEquals(1, iter.take.length);
  assertEquals('take', iter.take.name);
  const takeIter = iter.take(1);
  TestHelperPrototypeSurface(takeIter);
  assertEquals({value: 42, done: false }, takeIter.next());
  assertEquals({value: undefined, done: true }, takeIter.next());
})();

(function TestTakeAllElements() {
  const iter = gen();
  const takeIter = iter.take(2);
  TestHelperPrototypeSurface(takeIter);
  assertEquals({value: 42, done: false }, takeIter.next());
  assertEquals({value: 43, done: false }, takeIter.next());
  assertEquals({value: undefined, done: true }, takeIter.next());
})();

(function TestTakeNoElements() {
  const iter = gen();
  const takeIter = iter.take(0);
  TestHelperPrototypeSurface(takeIter);
  assertEquals({value: undefined, done: true }, takeIter.next());
})();

(function TestTakeMoreElements() {
  const iter = gen();
  const takeIter = iter.take(4);
  TestHelperPrototypeSurface(takeIter);
  assertEquals({value: 42, done: false }, takeIter.next());
  assertEquals({value: 43, done: false }, takeIter.next());
  assertEquals({value: undefined, done: true }, takeIter.next());
})();

(function TestTakeNegativeLimit() {
  const iter = gen();
  assertThrows(() => {iter.take(-3);});
})();

(function TestTakeInfinityLimit() {
  const iter = gen();
  const takeIter = iter.take(Number.POSITIVE_INFINITY);
  TestHelperPrototypeSurface(takeIter);
  assertEquals({value: 42, done: false }, takeIter.next());
  assertEquals({value: 43, done: false }, takeIter.next());
  assertEquals({value: undefined, done: true }, takeIter.next());
})();

(function TestTakeReturnInNormalIterator() {
  const NormalIterator = {
    i: 1,
    next() {
      if (this.i <= 3) {
                return {value: this.i++, done: false};
              } else {
                return {value: undefined, done: true};
              }
    },
    return() {return {value: undefined, done: true};},
  };

  Object.setPrototypeOf(NormalIterator, Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]())));
  const takeIter = NormalIterator.take(1);
  TestHelperPrototypeSurface(takeIter);
  assertEquals({value: 1, done: false }, takeIter.next());
  assertEquals({value: undefined, done: true }, takeIter.next());
})();

(function TestTakeNoReturnInIterator() {
  const IteratorNoReturn = {
    i: 1,
    next() {
      if (this.i <= 3) {
        return {value: this.i++, done: false};
      } else {
        return {value: undefined, done: true};
      }
    },
  };

  Object.setPrototypeOf(
      IteratorNoReturn,
      Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]())));
  assertThrows(() => {iter.take(1);});
})();

(function TestTakeNonObjectReturnInIterator() {
  const IteratorThrowReturn = {
    i: 1,
    next() {
      if (this.i <= 3) {
        return {value: this.i++, done: false};
      } else {
        return {value: undefined, done: true};
      }
    },
    return () {
      throw new Error('Non-object return');
    },
  };

  Object.setPrototypeOf(
      IteratorThrowReturn,
      Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]())));
  assertThrows(() => {iter.take(1);});
})();

// --- Test Drop helper

(function TestDrop() {
  const iter = longerGen();
  assertEquals('function', typeof iter.drop);
  assertEquals(1, iter.drop.length);
  assertEquals('drop', iter.drop.name);
  const dropIter = iter.drop(1);
  TestHelperPrototypeSurface(dropIter);
  assertEquals({value: 43, done: false}, dropIter.next());
  assertEquals({value: 44, done: false}, dropIter.next());
  assertEquals({value: 45, done: false}, dropIter.next());
  assertEquals({value: undefined, done: true}, dropIter.next());
})();

(function TestDropAllElements() {
  const iter = longerGen();
  const dropIter = iter.drop(4);
  TestHelperPrototypeSurface(dropIter);
  assertEquals({value: undefined, done: true}, dropIter.next());
})();

(function TestDropNoElements() {
  const iter = longerGen();
  const dropIter = iter.drop(0);
  TestHelperPrototypeSurface(dropIter);
  assertEquals({value: 42, done: false}, dropIter.next());
  assertEquals({value: 43, done: false}, dropIter.next());
  assertEquals({value: 44, done: false}, dropIter.next());
  assertEquals({value: 45, done: false}, dropIter.next());
  assertEquals({value: undefined, done: true}, dropIter.next());
})();

(function TestDropNegativeLimit() {
  const iter = longerGen();
  assertThrows(() => {
    iter.drop(-3);
  });
})();

(function TestDropInfinityLimit() {
  const iter = longerGen();
  const dropIter = iter.drop(Number.POSITIVE_INFINITY);
  TestHelperPrototypeSurface(dropIter);
  assertEquals({value: undefined, done: true}, dropIter.next());
})();

(function TestDropReturnInNormalIterator() {
  const NormalIterator = {
    i: 1,
    next() {
      if (this.i <= 3) {
        return {value: this.i++, done: false};
      } else {
        return {value: undefined, done: true};
      }
    },
    return () {
      return {value: undefined, done: true};
    },
  };

  Object.setPrototypeOf(
      NormalIterator,
      Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]())));
  const dropIter = NormalIterator.drop(1);
  TestHelperPrototypeSurface(dropIter);
  assertEquals({value: 2, done: false}, dropIter.next());
  assertEquals({value: 3, done: false}, dropIter.next());
  assertEquals({value: undefined, done: true}, dropIter.next());
})();

(function TestDropNoReturnInIterator() {
  const IteratorNoReturn = {
    i: 1,
    next() {
      if (this.i <= 3) {
        return {value: this.i++, done: false};
      } else {
        return {value: undefined, done: true};
      }
    },
  };

  Object.setPrototypeOf(
      IteratorNoReturn,
      Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]())));
  assertThrows(() => {
    iter.drop(4);
  });
})();

(function TestDropNonObjectReturnInIterator() {
  const IteratorThrowReturn = {
    i: 1,
    next() {
      if (this.i <= 3) {
        return {value: this.i++, done: false};
      } else {
        return {value: undefined, done: true};
      }
    },
    return () {
      throw new Error('Non-object return');
    },
  };

  Object.setPrototypeOf(
      IteratorThrowReturn,
      Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]())));
  assertThrows(() => {
    iter.drop(4);
  });
})();
