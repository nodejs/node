// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-iterator-close

function* g() { yield 42; return 88 };


// Return method is "undefined".
{
  g.prototype.return = null;

  assertEquals(undefined, (() => {
    for (let x of g()) { break; }
  })());

  assertEquals(undefined, (() => {
    for (x of g()) { break; }
  })());

  assertThrowsEquals(() => {
    for (let x of g()) { throw 42; }
  }, 42);

  assertThrowsEquals(() => {
    for (x of g()) { throw 42; }
  }, 42);

  assertEquals(42, (() => {
    for (let x of g()) { return 42; }
  })());

  assertEquals(42, (() => {
    for (x of g()) { return 42; }
  })());

  assertEquals(42, eval('for (let x of g()) { x; }'));

  assertEquals(42, eval('for (let x of g()) { x; }'));
}


// Return method is not callable.
{
  g.prototype.return = 666;

  assertThrows(() => {
    for (let x of g()) { break; }
  }, TypeError);

  assertThrows(() => {
    for (x of g()) { break; }
  }, TypeError);

  assertThrows(() => {
    for (let x of g()) { throw 666; }
  }, TypeError);

  assertThrows(() => {
    for (x of g()) { throw 666; }
  }, TypeError);

  assertThrows(() => {
    for (let x of g()) { return 666; }
  }, TypeError);

  assertThrows(() => {
    for (x of g()) { return 666; }
  }, TypeError);

  assertEquals(42, eval('for (let x of g()) { x; }'));

  assertEquals(42, eval('for (let x of g()) { x; }'));
}


// Return method does not return an object.
{
  g.prototype.return = () => 666;

  assertThrows(() => {
    for (let x of g()) { break; }
  }, TypeError);

  assertThrows(() => {
    for (x of g()) { break; }
  }, TypeError);

  assertThrows(() => {
    for (let x of g()) { throw 666; }
  }, TypeError);

  assertThrows(() => {
    for (x of g()) { throw 666; }
  }, TypeError);

  assertThrows(() => {
    for (let x of g()) { return 666; }
  }, TypeError);

  assertThrows(() => {
    for (x of g()) { return 666; }
  }, TypeError);

  assertEquals(42, eval('for (let x of g()) { x; }'));

  assertEquals(42, eval('for (x of g()) { x; }'));
}


// Return method returns an object.
{
  let log = [];
  g.prototype.return = (...args) => { log.push(args); return {} };

  log = [];
  for (let x of g()) { break; }
  assertEquals([[]], log);

  log = [];
  for (x of g()) { break; }
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (let x of g()) { throw 42; }
  }, 42);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (x of g()) { throw 42; }
  }, 42);
  assertEquals([[]], log);

  log = [];
  assertEquals(42, (() => {
    for (let x of g()) { return 42; }
  })());
  assertEquals([[]], log);

  log = [];
  assertEquals(42, (() => {
    for (x of g()) { return 42; }
  })());
  assertEquals([[]], log);

  log = [];
  assertEquals(42, eval('for (let x of g()) { x; }'));
  assertEquals([], log);

  log = [];
  assertEquals(42, eval('for (x of g()) { x; }'));
  assertEquals([], log);
}


// Return method throws.
{
  let log = [];
  g.prototype.return = (...args) => { log.push(args); throw 23 };

  log = [];
  assertThrowsEquals(() => {
    for (let x of g()) { break; }
  }, 23);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (x of g()) { break; }
  }, 23);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (let x of g()) { throw 42; }
  }, 42);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (x of g()) { throw 42; }
  }, 42);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (let x of g()) { return 42; }
  }, 23);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (x of g()) { return 42; }
  }, 23);
  assertEquals([[]], log);

  log = [];
  assertEquals(42, eval('for (let x of g()) { x; }'));
  assertEquals([], log);

  log = [];
  assertEquals(42, eval('for (x of g()) { x; }'));
  assertEquals([], log);
}


// Next method throws.
{
  g.prototype.next = () => { throw 666; };
  g.prototype.return = () => { assertUnreachable() };

  assertThrowsEquals(() => {
    for (let x of g()) {}
  }, 666);

  assertThrowsEquals(() => {
    for (x of g()) {}
  }, 666);
}


// Nested loops.
{
  function* g1() { yield 1; yield 2; throw 3; }
  function* g2() { yield -1; yield -2; throw -3; }

  assertDoesNotThrow(() => {
    for (let x of g1()) {
      for (let y of g2()) {
        if (y == -2) break;
      }
      if (x == 2) break;
    }
  }, -3);

  assertThrowsEquals(() => {
    for (let x of g1()) {
      for (let y of g2()) {
      }
    }
  }, -3);

  assertThrowsEquals(() => {
    for (let x of g1()) {
      for (let y of g2()) {
        if (y == -2) break;
      }
    }
  }, 3);

  assertDoesNotThrow(() => {
    l: for (let x of g1()) {
      for (let y of g2()) {
        if (y == -2) break l;
      }
    }
  });

  assertThrowsEquals(() => {
    for (let x of g1()) {
      for (let y of g2()) {
        throw 4;
      }
    }
  }, 4);

  assertThrowsEquals(() => {
    for (let x of g1()) {
      for (let y of g2()) {
        if (y == -2) throw 4;
      }
    }
  }, 4);

  let log = [];
  g1.prototype.return = () => { log.push(1); throw 5 };
  g2.prototype.return = () => { log.push(2); throw -5 };

  log = [];
  assertThrowsEquals(() => {
    for (let x of g1()) {
      for (let y of g2()) {
        if (y == -2) break;
      }
      if (x == 2) break;
    }
  }, -5);
  assertEquals([2, 1], log);

  log = [];
  assertThrowsEquals(() => {
    for (let x of g1()) {
      for (let y of g2()) {
      }
    }
  }, -3);
  assertEquals([1], log);

  log = [];
  assertThrowsEquals(() => {
    for (let x of g1()) {
      for (let y of g2()) {
        if (y == -2) break;
      }
    }
  }, -5);
  assertEquals([2, 1], log);

  log = [];
  assertThrowsEquals(() => {
    l: for (let x of g1()) {
      for (let y of g2()) {
        if (y == -2) break l;
      }
    }
  }, -5);
  assertEquals([2, 1], log);

  log = [];
  assertThrowsEquals(() => {
    for (let x of g1()) {
      for (let y of g2()) {
        throw 4;
      }
    }
  }, 4);
  assertEquals([2, 1], log);

  log = [];
  assertThrowsEquals(() => {
    for (let x of g1()) {
      for (let y of g2()) {
        if (y == -2) throw 4;
      }
    }
  }, 4);
  assertEquals([2, 1], log);

  log = [];
  assertThrowsEquals(() => {
    for (let x of g1()) {
      try {
        for (let y of g2()) {
        }
      } catch (_) {}
    }
  }, 3);
  assertEquals([], log);

  log = [];
  assertThrowsEquals(() => {
    for (let x of g1()) {
      try {
        for (let y of g2()) {
        }
      } catch (_) {}
      if (x == 2) break;
    }
  }, 5);
  assertEquals([1], log);
}
