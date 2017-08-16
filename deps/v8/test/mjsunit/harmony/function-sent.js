// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-function-sent


{
  function* g() { return function.sent }
  assertEquals({value: 42, done: true}, g().next(42));
}


{
  function* g() {
    try {
      yield function.sent;
    } finally {
      yield function.sent;
      return function.sent;
    }
  }

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next(1));
    assertEquals({value: 2, done: false}, x.next(2));
    assertEquals({value: 3, done: true}, x.next(3));
  }

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next(1));
    assertEquals({value: 2, done: false}, x.throw(2));
    assertEquals({value: 3, done: true}, x.next(3));
  }

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next(1));
    assertEquals({value: 2, done: false}, x.return(2));
    assertEquals({value: 3, done: true}, x.next(3));
  }
}


{
  function* inner() {
    try {
      yield function.sent;
    } finally {
      return 23;
    }
  }

  function* g() {
    yield function.sent;
    yield* inner();
    return function.sent;
  }

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next(1));
    assertEquals({value: undefined, done: false}, x.next(2));
    assertEquals({value: 3, done: true}, x.next(3));
  }

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next(1));
    assertEquals({value: undefined, done: false}, x.next(2));
    assertEquals({value: 42, done: true}, x.throw(42));
  }

  {
    let x = g();
    assertEquals({value: 1, done: false}, x.next(1));
    assertEquals({value: undefined, done: false}, x.next(2));
    assertEquals({value: 23, done: true}, x.return(42));
  }
}


assertThrows("function f() { return function.sent }", SyntaxError);
assertThrows("() => { return function.sent }", SyntaxError);
assertThrows("() => { function.sent }", SyntaxError);
assertThrows("() => function.sent", SyntaxError);
assertThrows("({*f() { function.sent }})", SyntaxError);
assertDoesNotThrow("({*f() { return function.sent }})");
