// Copyright 2013 the V8 project authors. All rights reserved.
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

// Test for-of semantics.

"use strict";


// First, some helpers.

function* values() {
  for (var i = 0; i < arguments.length; i++) {
    yield arguments[i];
  }
}

function wrap_iterator(iterator) {
    var iterable = {};
    iterable[Symbol.iterator] = function() { return iterator; };
    return iterable;
}

function integers_until(max) {
  function next() {
    var ret = { value: this.n, done: this.n == max };
    this.n++;
    return ret;
  }
  return wrap_iterator({ next: next, n: 0 });
}

function results(results) {
  var i = 0;
  function next() {
    return results[i++];
  }
  return wrap_iterator({ next: next });
}

function* integers_from(n) {
  while (1) yield n++;
}

// A destructive append.
function append(x, tail) {
  tail[tail.length] = x;
  return tail;
}

function sum(x, tail) {
  return x + tail;
}

function fold(cons, seed, iterable) {
  for (var x of iterable) {
    seed = cons(x, seed);
  }
  return seed;
}

function* take(iterable, n) {
  if (n == 0) return;
  for (let x of iterable) {
    yield x;
    if (--n == 0) break;
  }
}

function nth(iterable, n) {
  for (let x of iterable) {
    if (n-- == 0) return x;
  }
  throw "unreachable";
}

function* skip_every(iterable, n) {
  var i = 0;
  for (let x of iterable) {
    if (++i % n == 0) continue;
    yield x;
  }
}

function* iter_map(iterable, f) {
  for (var x of iterable) {
    yield f(x);
  }
}
function nested_fold(cons, seed, iterable) {
  var visited = []
  for (let x of iterable) {
    for (let y of x) {
      seed = cons(y, seed);
    }
  }
  return seed;
}

function* unreachable(iterable) {
  for (let x of iterable) {
    throw "not reached";
  }
}

function one_time_getter(o, prop, val) {
  function set_never() { throw "unreachable"; }
  var gotten = false;
  function get_once() {
    if (gotten) throw "got twice";
    gotten = true;
    return val;
  }
  Object.defineProperty(o, prop, {get: get_once, set: set_never})
  return o;
}

function never_getter(o, prop) {
  function never() { throw "unreachable"; }
  Object.defineProperty(o, prop, {get: never, set: never})
  return o;
}

function remove_next_after(iterable, n) {
  var iterator = iterable[Symbol.iterator]();
  function next() {
    if (n-- == 0) delete this.next;
    return iterator.next();
  }
  return wrap_iterator({ next: next });
}

function poison_next_after(iterable, n) {
  var iterator = iterable[Symbol.iterator]();
  function next() {
    return iterator.next();
  }
  function next_getter() {
    if (n-- < 0)
      throw "poisoned";
    return next;
  }
  var o = {};
  Object.defineProperty(o, 'next', { get: next_getter });
  return wrap_iterator(o);
}

// Now, the tests.

// Non-generator iterators.
assertEquals(45, fold(sum, 0, integers_until(10)));
// Generator iterators.
assertEquals([1, 2, 3], fold(append, [], values(1, 2, 3)));
// Break.
assertEquals(45, fold(sum, 0, take(integers_from(0), 10)));
// Continue.
assertEquals(90, fold(sum, 0, take(skip_every(integers_from(0), 2), 10)));
// Return.
assertEquals(10, nth(integers_from(0), 10));
// Nested for-of.
assertEquals([0, 0, 1, 0, 1, 2, 0, 1, 2, 3],
             nested_fold(append,
                         [],
                         iter_map(integers_until(5), integers_until)));
// Result objects with sparse fields.
assertEquals([undefined, 1, 2, 3],
             fold(append, [],
                  results([{ done: false },
                           { value: 1, done: false },
                           // A missing "done" is the same as undefined, which
                           // is false.
                           { value: 2 },
                           // Not done.
                           { value: 3, done: 0 },
                           // Done.
                           { value: 4, done: 42 }])));
// Results that are not objects.
assertThrows(function() {
  assertEquals([undefined, undefined, undefined],
               fold(append, [],
                    results([10, "foo", /qux/, { value: 37, done: true }])));
}, TypeError);
// Getters (shudder).
assertEquals([1, 2],
             fold(append, [],
                  results([one_time_getter({ value: 1 }, 'done', false),
                           one_time_getter({ done: false }, 'value', 2),
                           { value: 37, done: true },
                           never_getter(never_getter({}, 'done'), 'value')])));

// Unlike the case with for-in, null and undefined cause an error.
assertThrows('fold(sum, 0, unreachable(null))', TypeError);
assertThrows('fold(sum, 0, unreachable(undefined))', TypeError);

// Other non-iterators do cause an error.
assertThrows('fold(sum, 0, unreachable({}))', TypeError);
assertThrows('fold(sum, 0, unreachable(false))', TypeError);
assertThrows('fold(sum, 0, unreachable(37))', TypeError);

// "next" is looked up each time.
assertThrows('fold(sum, 0, remove_next_after(integers_until(10), 5))',
             TypeError);
// It is not called at any other time.
assertEquals(45,
             fold(sum, 0, remove_next_after(integers_until(10), 10)));
// It is not looked up too many times.
assertEquals(45,
             fold(sum, 0, poison_next_after(integers_until(10), 10)));

function labelled_continue(iterable) {
  var n = 0;
outer:
  while (true) {
    n++;
    for (var x of iterable) continue outer;
    break;
  }
  return n;
}
assertEquals(11, labelled_continue(integers_until(10)));

function labelled_break(iterable) {
  var n = 0;
outer:
  while (true) {
    n++;
    for (var x of iterable) break outer;
  }
  return n;
}
assertEquals(1, labelled_break(integers_until(10)));

// Test continue/break in catch.
function catch_control(iterable, k) {
  var n = 0;
  for (var x of iterable) {
    try {
      return k(x);
    } catch (e) {
      if (e == "continue") continue;
      else if (e == "break") break;
      else throw e;
    }
  } while (false);
  return false;
}
assertEquals(false,
             catch_control(integers_until(10),
                           function() { throw "break" }));
assertEquals(false,
             catch_control(integers_until(10),
                           function() { throw "continue" }));
assertEquals(5,
             catch_control(integers_until(10),
                           function(x) {
                             if (x == 5) return x;
                             throw "continue";
                           }));

// Test continue/break in try.
function try_control(iterable, k) {
  var n = 0;
  for (var x of iterable) {
    try {
      var e = k(x);
      if (e == "continue") continue;
      else if (e == "break") break;
      return e;
    } catch (e) {
      throw e;
    }
  } while (false);
  return false;
}
assertEquals(false,
             try_control(integers_until(10),
                         function() { return "break" }));
assertEquals(false,
             try_control(integers_until(10),
                         function() { return "continue" }));
assertEquals(5,
             try_control(integers_until(10),
                         function(x) { return (x == 5) ? x : "continue" }));

// TODO(neis,cbruni): Enable once the corresponding traps work again.
// Proxy results, with getters.
// function transparent_proxy(x) {
//   return new Proxy({}, {
//     get: function(receiver, name) { return x[name]; }
//   });
// }
// assertEquals([1, 2],
//              fold(append, [],
//                   results([one_time_getter({ value: 1 }, 'done', false),
//                            one_time_getter({ done: false }, 'value', 2),
//                            { value: 37, done: true },
//                            never_getter(never_getter({}, 'done'), 'value')]
//                           .map(transparent_proxy))));

// Proxy iterators.
// function poison_proxy_after(iterable, n) {
//   var iterator = iterable[Symbol.iterator]();
//   return wrap_iterator(new Proxy({}, {
//     get: function(receiver, name) {
//       if (name == 'next' && n-- < 0) throw "unreachable";
//       return iterator[name];
//     },
//     // Needed for integers_until(10)'s this.n++.
//     set: function(receiver, name, val) {
//       return iterator[name] = val;
//     }
//   }));
// }
// assertEquals(45, fold(sum, 0, poison_proxy_after(integers_until(10), 10)));


function test_iterator_result_object_non_object(value, descr) {
  var arr = [];
  var ex;
  var message = 'Iterator result ' + (descr || value) + ' is not an object';
  try {
    fold(append, arr,
         results([{value: 1}, {}, value, {value: 2}, {done: true}]));
  } catch (e) {
    ex = e;
  }
  assertInstanceof(ex, TypeError);
  assertEquals(message, ex.message);
  assertArrayEquals([1, undefined], arr);
}
test_iterator_result_object_non_object(null);
test_iterator_result_object_non_object(undefined);
test_iterator_result_object_non_object(42);
test_iterator_result_object_non_object('abc');
test_iterator_result_object_non_object(false);
test_iterator_result_object_non_object(Symbol('x'), 'Symbol(x)');
