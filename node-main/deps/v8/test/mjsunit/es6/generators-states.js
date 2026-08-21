// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test generator states.

function Foo() {}
function Bar() {}

function assertIteratorResult(value, done, result) {
  assertEquals({ value: value, done: done}, result);
}

function assertIteratorIsClosed(iter) {
  assertIteratorResult(undefined, true, iter.next());
  // Next and throw on a closed iterator.
  assertDoesNotThrow(function() { iter.next(); });
  assertThrows(function() { iter.throw(new Bar); }, Bar);
}

var iter;
function* nextGenerator() { yield iter.next(); }
function* throwGenerator() { yield iter.throw(new Bar); }

// Throw on a suspendedStart iterator.
iter = nextGenerator();
assertThrows(function() { iter.throw(new Foo) }, Foo)
assertIteratorIsClosed(iter);
assertThrows(function() { iter.throw(new Foo) }, Foo)
assertIteratorIsClosed(iter);

// The same.
iter = throwGenerator();
assertThrows(function() { iter.throw(new Foo) }, Foo)
assertThrows(function() { iter.throw(new Foo) }, Foo)
assertIteratorIsClosed(iter);

// Next on an executing iterator raises a TypeError.
iter = nextGenerator();
assertThrows(function() { iter.next() }, TypeError)
assertIteratorIsClosed(iter);

// Throw on an executing iterator raises a TypeError.
iter = throwGenerator();
assertThrows(function() { iter.next() }, TypeError)
assertIteratorIsClosed(iter);

// Next on an executing iterator doesn't change the state of the
// generator.
iter = (function* () {
  try {
    iter.next();
    yield 1;
  } catch (e) {
    try {
      // This next() should raise the same exception, because the first
      // next() left the iter in the executing state.
      iter.next();
      yield 2;
    } catch (e) {
      yield 3;
    }
  }
  yield 4;
})();
assertIteratorResult(3, false, iter.next());
assertIteratorResult(4, false, iter.next());
assertIteratorIsClosed(iter);


// A return that doesn't close.
{
  let g = function*() { try {return 42} finally {yield 43} };

  let x = g();
  assertEquals({value: 43, done: false}, x.next());
  assertEquals({value: 42, done: true}, x.next());
}
{
  let x;
  let g = function*() { try {return 42} finally {x.throw(666)} };

  x = g();
  assertThrows(() => x.next(), TypeError);  // Still executing.
}
{
  let x;
  let g = function*() {
    try {return 42} finally {try {x.throw(666)} catch(e) {}}
  };

  x = g();
  assertEquals({value: 42, done: true}, x.next());
}
