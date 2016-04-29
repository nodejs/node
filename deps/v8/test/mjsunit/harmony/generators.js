// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


{ // yield in try-catch

  let g = function*() {
    try {yield 1} catch (error) {assertEquals("caught", error)}
  };

  assertThrowsEquals(() => g().throw("not caught"), "not caught");

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next());
    assertEquals({value: undefined, done: true}, x.throw("caught"));
  }

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next());
    assertEquals({value: undefined, done: true}, x.next());
    assertThrowsEquals(() => x.throw("not caught"), "not caught");
  }
}


{ // return that doesn't close
  let g = function*() { try {return 42} finally {yield 43} };

  {
    let x = g();
    assertEquals({value: 43, done: false}, x.next());
    assertEquals({value: 42, done: true}, x.next());
  }
}


{ // return that doesn't close
  let x;
  let g = function*() { try {return 42} finally {x.throw(666)} };

  {
    x = g();
    assertThrows(() => x.next(), TypeError);  // still executing
  }
}


{ // yield in try-finally, finally clause performs return

  let g = function*() { try {yield 42} finally {return 13} };

  { // "return" closes at suspendedStart
    let x = g();
    assertEquals({value: 666, done: true}, x.return(666));
    assertEquals({value: undefined, done: true}, x.next(42));
    assertThrowsEquals(() => x.throw(43), 43);
    assertEquals({value: 42, done: true}, x.return(42));
  }

  { // "throw" closes at suspendedStart
    let x = g();
    assertThrowsEquals(() => x.throw(666), 666);
    assertEquals({value: undefined, done: true}, x.next(42));
    assertEquals({value: 43, done: true}, x.return(43));
    assertThrowsEquals(() => x.throw(44), 44);
  }

  { // "next" closes at suspendedYield
    let x = g();
    assertEquals({value: 42, done: false}, x.next());
    assertEquals({value: 13, done: true}, x.next(666));
    assertEquals({value: undefined, done: true}, x.next(666));
    assertThrowsEquals(() => x.throw(666), 666);
  }

  { // "return" closes at suspendedYield
    let x = g();
    assertEquals({value: 42, done: false}, x.next());
    assertEquals({value: 13, done: true}, x.return(666));
    assertEquals({value: undefined, done: true}, x.next(666));
    assertEquals({value: 666, done: true}, x.return(666));
  }

  { // "throw" closes at suspendedYield
    let x = g();
    assertEquals({value: 42, done: false}, x.next());
    assertEquals({value: 13, done: true}, x.throw(666));
    assertThrowsEquals(() => x.throw(666), 666);
    assertEquals({value: undefined, done: true}, x.next(666));
  }
}


{ // yield in try-finally, finally clause doesn't perform return

  let g = function*() { try {yield 42} finally {13} };

  { // "return" closes at suspendedStart
    let x = g();
    assertEquals({value: 666, done: true}, x.return(666));
    assertEquals({value: undefined, done: true}, x.next(42));
    assertThrowsEquals(() => x.throw(43), 43);
    assertEquals({value: 42, done: true}, x.return(42));
  }

  { // "throw" closes at suspendedStart
    let x = g();
    assertThrowsEquals(() => x.throw(666), 666);
    assertEquals({value: undefined, done: true}, x.next(42));
    assertEquals({value: 43, done: true}, x.return(43));
    assertThrowsEquals(() => x.throw(44), 44);
  }

  { // "next" closes at suspendedYield
    let x = g();
    assertEquals({value: 42, done: false}, x.next());
    assertEquals({value: undefined, done: true}, x.next(666));
    assertEquals({value: undefined, done: true}, x.next(666));
    assertThrowsEquals(() => x.throw(666), 666);
    assertEquals({value: 42, done: true}, x.return(42));
  }

  { // "return" closes at suspendedYield
    let x = g();
    assertEquals({value: 42, done: false}, x.next());
    assertEquals({value: 666, done: true}, x.return(666));
    assertEquals({value: undefined, done: true}, x.next(666));
    assertThrowsEquals(() => x.throw(44), 44);
    assertEquals({value: 42, done: true}, x.return(42));
  }

  { // "throw" closes at suspendedYield
    let x = g();
    assertEquals({value: 42, done: false}, x.next());
    assertThrowsEquals(() => x.throw(666), 666);
    assertEquals({value: undefined, done: true}, x.next(666));
    assertThrowsEquals(() => x.throw(666), 666);
    assertEquals({value: 42, done: true}, x.return(42));
  }
}


{ // yield in try-finally, finally clause yields and performs return

  let g = function*() { try {yield 42} finally {yield 43; return 13} };

  {
    let x = g();
    assertEquals({value: 42, done: false}, x.next());
    assertEquals({value: 43, done: false}, x.return(666));
    assertEquals({value: 13, done: true}, x.next());
    assertEquals({value: 666, done: true}, x.return(666));
  }

  {
    let x = g();
    assertEquals({value: 666, done: true}, x.return(666));
    assertEquals({value: undefined, done: true}, x.next());
    assertEquals({value: 666, done: true}, x.return(666));
  }
}


{ // yield in try-finally, finally clause yields and doesn't perform return

  let g = function*() { try {yield 42} finally {yield 43; 13} };

  {
    let x = g();
    assertEquals({value: 42, done: false}, x.next());
    assertEquals({value: 43, done: false}, x.return(666));
    assertEquals({value: 666, done: true}, x.next());
    assertEquals({value: 5, done: true}, x.return(5));
  }

  {
    let x = g();
    assertEquals({value: 666, done: true}, x.return(666));
    assertEquals({value: undefined, done: true}, x.next());
    assertEquals({value: 666, done: true}, x.return(666));
  }
}


{ // yield*, finally clause performs return

  let h = function*() { try {yield 42} finally {yield 43; return 13} };
  let g = function*() { yield 1; yield yield* h(); };

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next());
    assertEquals({value: 42, done: false}, x.next());
    assertEquals({value: 43, done: false}, x.next(666));
    assertEquals({value: 13, done: false}, x.next());
    assertEquals({value: undefined, done: true}, x.next());
  }

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next());
    assertEquals({value: 42, done: false}, x.next());
    assertEquals({value: 43, done: false}, x.return(666));
    assertEquals({value: 13, done: false}, x.next());
    assertEquals({value: undefined, done: true}, x.next());
  }

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next());
    assertEquals({value: 42, done: false}, x.next());
    assertEquals({value: 43, done: false}, x.throw(666));
    assertEquals({value: 13, done: false}, x.next());
    assertEquals({value: undefined, done: true}, x.next());
  }
}


{ // yield*, finally clause does not perform return

  let h = function*() { try {yield 42} finally {yield 43; 13} };
  let g = function*() { yield 1; yield yield* h(); };

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next());
    assertEquals({value: 42, done: false}, x.next());
    assertEquals({value: 43, done: false}, x.next(666));
    assertEquals({value: undefined, done: false}, x.next());
    assertEquals({value: undefined, done: true}, x.next());
  }

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next());
    assertEquals({value: 42, done: false}, x.next());
    assertEquals({value: 43, done: false}, x.return(44));
    assertEquals({value: 44, done: false}, x.next());
    assertEquals({value: undefined, done: true}, x.next());
  }

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next());
    assertEquals({value: 42, done: false}, x.next());
    assertEquals({value: 43, done: false}, x.throw(666));
    assertThrowsEquals(() => x.next(), 666);
  }
}


{ // yield*, .return argument is final result

  function* inner() {
    yield 2;
  }

  function* g() {
    yield 1;
    return yield* inner();
  }

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next());
    assertEquals({value: 2, done: false}, x.next());
    assertEquals({value: 42, done: true}, x.return(42));
  }
}
