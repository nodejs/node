// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function* g() { yield 42; return 88 };


// Return method is "undefined".
{
  g.prototype.return = null;


  assertEquals(undefined, (() => {
    for (var x of g()) { break; }
  })());

  assertEquals(undefined, (() => {
    for (let x of g()) { break; }
  })());

  assertEquals(undefined, (() => {
    for (const x of g()) { break; }
  })());

  assertEquals(undefined, (() => {
    for (x of g()) { break; }
  })());


  assertThrowsEquals(() => {
    for (var x of g()) { throw 42; }
  }, 42);

  assertThrowsEquals(() => {
    for (let x of g()) { throw 42; }
  }, 42);

  assertThrowsEquals(() => {
    for (const x of g()) { throw 42; }
  }, 42);

  assertThrowsEquals(() => {
    for (x of g()) { throw 42; }
  }, 42);


  assertEquals(42, (() => {
    for (var x of g()) { return 42; }
  })());

  assertEquals(42, (() => {
    for (let x of g()) { return 42; }
  })());

  assertEquals(42, (() => {
    for (const x of g()) { return 42; }
  })());

  assertEquals(42, (() => {
    for (x of g()) { return 42; }
  })());


  assertEquals(42, eval('for (var x of g()) { x; }'));

  assertEquals(42, eval('for (let x of g()) { x; }'));

  assertEquals(42, eval('for (const x of g()) { x; }'));

  assertEquals(42, eval('for (x of g()) { x; }'));


  assertEquals(42, (() => {
    var [x] = g(); return x;
  })());

  assertEquals(42, (() => {
    let [x] = g(); return x;
  })());

  assertEquals(42, (() => {
    const [x] = g(); return x;
  })());

  assertEquals(42, (() => {
    [x] = g(); return x;
  })());

  assertEquals(42,
    (([x]) => x)(g())
  );
}


// Return method is not callable.
{
  g.prototype.return = 666;


  assertThrows(() => {
    for (var x of g()) { break; }
  }, TypeError);

  assertThrows(() => {
    for (let x of g()) { break; }
  }, TypeError);

  assertThrows(() => {
    for (const x of g()) { break; }
  }, TypeError);

  assertThrows(() => {
    for (x of g()) { break; }
  }, TypeError);


  assertThrowsEquals(() => {
    for (var x of g()) { throw 666; }
  }, 666);

  assertThrowsEquals(() => {
    for (let x of g()) { throw 666; }
  }, 666);

  assertThrowsEquals(() => {
    for (const x of g()) { throw 666; }
  }, 666);

  assertThrowsEquals(() => {
    for (x of g()) { throw 666; }
  }, 666);


  assertThrows(() => {
    for (var x of g()) { return 666; }
  }, TypeError);

  assertThrows(() => {
    for (let x of g()) { return 666; }
  }, TypeError);

  assertThrows(() => {
    for (const x of g()) { return 666; }
  }, TypeError);

  assertThrows(() => {
    for (x of g()) { return 666; }
  }, TypeError);


  assertEquals(42, eval('for (var x of g()) { x; }'));

  assertEquals(42, eval('for (let x of g()) { x; }'));

  assertEquals(42, eval('for (const x of g()) { x; }'));

  assertEquals(42, eval('for (x of g()) { x; }'));


  assertThrows(() => {
    var [x] = g(); return x;
  }, TypeError);

  assertThrows(() => {
    let [x] = g(); return x;
  }, TypeError);

  assertThrows(() => {
    const [x] = g(); return x;
  }, TypeError);

  assertThrows(() => {
    [x] = g(); return x;
  }, TypeError);

  assertThrows(() => {
    (([x]) => x)(g());
  }, TypeError);
}


// Return method does not return an object.
{
  g.prototype.return = () => 666;


  assertThrows(() => {
    for (var x of g()) { break; }
  }, TypeError);

  assertThrows(() => {
    for (let x of g()) { break; }
  }, TypeError);

  assertThrows(() => {
    for (const x of g()) { break; }
  }, TypeError);

  assertThrows(() => {
    for (x of g()) { break; }
  }, TypeError);


  // Throw from the body of a for loop 'wins' vs throw
  // originating from a bad 'return' value.

  assertThrowsEquals(() => {
    for (var x of g()) { throw 666; }
  }, 666);

  assertThrowsEquals(() => {
    for (let x of g()) { throw 666; }
  }, 666);

  assertThrowsEquals(() => {
    for (const x of g()) { throw 666; }
  }, 666);

  assertThrowsEquals(() => {
    for (x of g()) { throw 666; }
  }, 666);


  assertThrows(() => {
    for (var x of g()) { return 666; }
  }, TypeError);

  assertThrows(() => {
    for (let x of g()) { return 666; }
  }, TypeError);

  assertThrows(() => {
    for (const x of g()) { return 666; }
  }, TypeError);

  assertThrows(() => {
    for (x of g()) { return 666; }
  }, TypeError);


  assertEquals(42, eval('for (var x of g()) { x; }'));

  assertEquals(42, eval('for (let x of g()) { x; }'));

  assertEquals(42, eval('for (const x of g()) { x; }'));

  assertEquals(42, eval('for (x of g()) { x; }'));


  assertThrows(() => {
    var [x] = g(); return x;
  }, TypeError);

  assertThrows(() => {
    let [x] = g(); return x;
  }, TypeError);

  assertThrows(() => {
    const [x] = g(); return x;
  }, TypeError);

  assertThrows(() => {
    [x] = g(); return x;
  }, TypeError);

  assertThrows(() => {
    (([x]) => x)(g());
  }, TypeError);
}


// Return method returns an object.
{
  let log = [];
  g.prototype.return = (...args) => { log.push(args); return {} };


  log = [];
  for (var x of g()) { break; }
  assertEquals([[]], log);

  log = [];
  for (let x of g()) { break; }
  assertEquals([[]], log);

  log = [];
  for (const x of g()) { break; }
  assertEquals([[]], log);

  log = [];
  for (x of g()) { break; }
  assertEquals([[]], log);


  log = [];
  assertThrowsEquals(() => {
    for (var x of g()) { throw 42; }
  }, 42);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (let x of g()) { throw 42; }
  }, 42);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (const x of g()) { throw 42; }
  }, 42);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (x of g()) { throw 42; }
  }, 42);
  assertEquals([[]], log);


  log = [];
  assertEquals(42, (() => {
    for (var x of g()) { return 42; }
  })());
  assertEquals([[]], log);

  log = [];
  assertEquals(42, (() => {
    for (let x of g()) { return 42; }
  })());
  assertEquals([[]], log);

  log = [];
  assertEquals(42, (() => {
    for (const x of g()) { return 42; }
  })());
  assertEquals([[]], log);

  log = [];
  assertEquals(42, (() => {
    for (x of g()) { return 42; }
  })());
  assertEquals([[]], log);


  log = [];
  assertEquals(42, eval('for (var x of g()) { x; }'));
  assertEquals([], log);

  log = [];
  assertEquals(42, eval('for (let x of g()) { x; }'));
  assertEquals([], log);

  log = [];
  assertEquals(42, eval('for (const x of g()) { x; }'));
  assertEquals([], log);

  log = [];
  assertEquals(42, eval('for (x of g()) { x; }'));
  assertEquals([], log);


  // Even if doing the assignment throws, still call return
  log = [];
  x = { set attr(_) { throw 1234; } };
  assertThrowsEquals(() => {
    for (x.attr of g()) { throw 456; }
  }, 1234);
  assertEquals([[]], log);


  log = [];
  assertEquals(42, (() => {
    var [x] = g(); return x;
  })());
  assertEquals([[]], log);

  log = [];
  assertEquals(42, (() => {
    let [x] = g(); return x;
  })());
  assertEquals([[]], log);

  log = [];
  assertEquals(42, (() => {
    const [x] = g(); return x;
  })());
  assertEquals([[]], log);

  log = [];
  assertEquals(42, (() => {
    [x] = g(); return x;
  })());
  assertEquals([[]], log);

  log = []
  assertEquals(42,
    (([x]) => x)(g())
  );
  assertEquals([[]], log);


  log = [];
  assertEquals(42, (() => {
    var [x,] = g(); return x;
  })());
  assertEquals([[]], log);

  log = [];
  assertEquals(42, (() => {
    let [x,] = g(); return x;
  })());
  assertEquals([[]], log);

  log = [];
  assertEquals(42, (() => {
    const [x,] = g(); return x;
  })());
  assertEquals([[]], log);

  log = [];
  assertEquals(42, (() => {
    [x,] = g(); return x;
  })());
  assertEquals([[]], log);

  log = []
  assertEquals(42,
    (([x,]) => x)(g())
  );
  assertEquals([[]], log);


  log = [];
  assertEquals(42, (() => {
    var [x,,] = g(); return x;
  })());
  assertEquals([], log);

  log = [];
  assertEquals(42, (() => {
    let [x,,] = g(); return x;
  })());
  assertEquals([], log);

  log = [];
  assertEquals(42, (() => {
    const [x,,] = g(); return x;
  })());
  assertEquals([], log);

  log = [];
  assertEquals(42, (() => {
    [x,,] = g(); return x;
  })());
  assertEquals([], log);

  log = []
  assertEquals(42,
    (([x,,]) => x)(g())
  );
  assertEquals([], log);


  log = [];
  assertEquals([42, undefined], (() => {
    var [x, y] = g(); return [x, y];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42, undefined], (() => {
    let [x, y] = g(); return [x, y];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42, undefined], (() => {
    const [x, y] = g(); return [x, y];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42, undefined], (() => {
    [x, y] = g(); return [x, y];
  })());
  assertEquals([], log);

  log = []
  assertEquals([42, undefined],
    (([x, y]) => [x, y])(g())
  );
  assertEquals([], log);


  log = [];
  assertEquals([42], (() => {
    var [...x] = g(); return x;
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42], (() => {
    let [...x] = g(); return x;
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42], (() => {
    const [...x] = g(); return x;
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42], (() => {
    [...x] = g(); return x;
  })());
  assertEquals([], log);

  log = []
  assertEquals([42],
    (([...x]) => x)(g())
  );
  assertEquals([], log);


  log = [];
  assertEquals([42, []], (() => {
    var [x, ...y] = g(); return [x, y];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42, []], (() => {
    let [x, ...y] = g(); return [x, y];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42, []], (() => {
    const [x, ...y] = g(); return [x, y];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42, []], (() => {
    [x, ...y] = g(); return [x, y];
  })());
  assertEquals([], log);

  log = []
  assertEquals([42, []],
    (([x, ...y]) => [x, y])(g())
  );
  assertEquals([], log);


  log = [];
  assertEquals([], (() => {
    var [] = g(); return [];
  })());
  assertEquals([[]], log);

  log = [];
  assertEquals([], (() => {
    let [] = g(); return [];
  })());
  assertEquals([[]], log);

  log = [];
  assertEquals([], (() => {
    const [] = g(); return [];
  })());
  assertEquals([[]], log);

  log = [];
  assertEquals([], (() => {
    [] = g(); return [];
  })());
  assertEquals([[]], log);

  log = []
  assertEquals([],
    (([]) => [])(g())
  );
  assertEquals([[]], log);


  log = [];
  assertEquals([], (() => {
    var [...[]] = g(); return [];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([], (() => {
    let [...[]] = g(); return [];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([], (() => {
    const [...[]] = g(); return [];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([], (() => {
    [...[]] = g(); return [];
  })());
  assertEquals([], log);

  log = []
  assertEquals([],
    (([...[]]) => [])(g())
  );
  assertEquals([], log);


  log = [];
  assertEquals([42], (() => {
    var [...[x]] = g(); return [x];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42], (() => {
    let [...[x]] = g(); return [x];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42], (() => {
    const [...[x]] = g(); return [x];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42], (() => {
    [...[x]] = g(); return [x];
  })());
  assertEquals([], log);

  log = []
  assertEquals([42],
    (([...[x]]) => [x])(g())
  );
  assertEquals([], log);


  log = [];
  assertEquals([42, undefined], (() => {
    var [...[x, y]] = g(); return [x, y];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42, undefined], (() => {
    let [...[x, y]] = g(); return [x, y];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42, undefined], (() => {
    const [...[x, y]] = g(); return [x, y];
  })());
  assertEquals([], log);

  log = [];
  assertEquals([42, undefined], (() => {
    [...[x, y]] = g(); return [x, y];
  })());
  assertEquals([], log);

  log = []
  assertEquals([42, undefined],
    (([...[x, y]]) => [x, y])(g())
  );
  assertEquals([], log);


  log = []
  assertThrowsEquals(() => {
    let x = { set foo(_) { throw 666; } };
    [x.foo] = g();
  }, 666);
  assertEquals([[]], log);


  log = []
  assertThrows(() => {
    var [[]] = g();
  }, TypeError);
  assertEquals([[]], log);

  log = []
  assertThrows(() => {
    let [[]] = g();
  }, TypeError);
  assertEquals([[]], log);

  log = []
  assertThrows(() => {
    const [[]] = g();
  }, TypeError);
  assertEquals([[]], log);

  log = []
  assertThrows(() => {
    [[]] = g();
  }, TypeError);
  assertEquals([[]], log);

  log = []
  assertThrows(() => {
    (([[]]) => 0)(g());
  }, TypeError);
  assertEquals([[]], log);


  log = []
  assertThrows(() => {
    var [...[[]]] = g();
  }, TypeError);
  assertEquals([], log);

  log = []
  assertThrows(() => {
    let [...[[]]] = g();
  }, TypeError);
  assertEquals([], log);

  log = []
  assertThrows(() => {
    const [...[[]]] = g();
  }, TypeError);
  assertEquals([], log);

  log = []
  assertThrows(() => {
    [...[[]]] = g();
  }, TypeError);
  assertEquals([], log);

  log = []
  assertThrows(() => {
    (([...[[]]]) => 0)(g());
  }, TypeError);
  assertEquals([], log);


  {
    let backup = Array.prototype[Symbol.iterator];
    Array.prototype[Symbol.iterator] = () => g();


    log = [];
    assertDoesNotThrow(() => {
      var [x, ...[y]] = [1, 2, 3]
    });
    assertEquals(log, [[]]);

    log = [];
    assertDoesNotThrow(() => {
      let [x, ...[y]] = [1, 2, 3];
    });
    assertEquals(log, [[]]);

    log = [];
    assertDoesNotThrow(() => {
      const [x, ...[y]] = [1, 2, 3];
    });
    assertEquals(log, [[]]);

    log = [];
    assertDoesNotThrow(() => {
      (([x, ...[y]]) => {})([1, 2, 3]);
    });
    assertEquals(log, [[]]);


    log = [];
    assertThrows(() => {
      var [x, ...[[]]] = [1, 2, 3];
    }, TypeError);
    assertEquals(log, [[]]);

    log = [];
    assertThrows(() => {
      let [x, ...[[]]] = [1, 2, 3];
    }, TypeError);
    assertEquals(log, [[]]);

    log = [];
    assertThrows(() => {
      const [x, ...[[]]] = [1, 2, 3];
    }, TypeError);
    assertEquals(log, [[]]);

    log = [];
    assertThrows(() => {
      (([x, ...[[]]]) => {})([1, 2, 3]);
    }, TypeError);
    assertEquals(log, [[]]);


    log = [];
    assertDoesNotThrow(() => {
      var [x, ...[...y]] = [1, 2, 3];
    });
    assertEquals(log, []);

    log = [];
    assertDoesNotThrow(() => {
      let [x, ...[...y]] = [1, 2, 3];
    });
    assertEquals(log, []);

    log = [];
    assertDoesNotThrow(() => {
      const [x, ...[...y]] = [1, 2, 3];
    });
    assertEquals(log, []);

    log = [];
    assertDoesNotThrow(() => {
      (([x, ...[...y]]) => {})([1, 2, 3]);
    });
    assertEquals(log, []);


    Array.prototype[Symbol.iterator] = backup;
  }
}


// Return method throws.
{
  let log = [];
  g.prototype.return = (...args) => { log.push(args); throw 23 };


  log = [];
  assertThrowsEquals(() => {
    for (var x of g()) { break; }
  }, 23);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (let x of g()) { break; }
  }, 23);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (const x of g()) { break; }
  }, 23);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (x of g()) { break; }
  }, 23);
  assertEquals([[]], log);


  log = [];
  assertThrowsEquals(() => {
    for (var x of g()) { throw 42; }
  }, 42);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (let x of g()) { throw 42; }
  }, 42);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (const x of g()) { throw 42; }
  }, 42);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (x of g()) { throw 42; }
  }, 42);
  assertEquals([[]], log);


  log = [];
  assertThrowsEquals(() => {
    for (var x of g()) { return 42; }
  }, 23);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (let x of g()) { return 42; }
  }, 23);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (const x of g()) { return 42; }
  }, 23);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    for (x of g()) { return 42; }
  }, 23);
  assertEquals([[]], log);


  log = [];
  assertEquals(42, eval('for (var x of g()) { x; }'));
  assertEquals([], log);

  log = [];
  assertEquals(42, eval('for (let x of g()) { x; }'));
  assertEquals([], log);

  log = [];
  assertEquals(42, eval('for (const x of g()) { x; }'));
  assertEquals([], log);

  log = [];
  assertEquals(42, eval('for (x of g()) { x; }'));
  assertEquals([], log);


  log = [];
  assertThrowsEquals(() => {
    var [x] = g(); return x;
  }, 23);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    let [x] = g(); return x;
  }, 23);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    const [x] = g(); return x;
  }, 23);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    [x] = g(); return x;
  }, 23);
  assertEquals([[]], log);

  log = [];
  assertThrowsEquals(() => {
    (([x]) => x)(g())
  }, 23);
  assertEquals([[]], log);
}


// Next method throws.
{
  let closed = false;
  g.prototype.next = () => { throw 666; };
  g.prototype.return = () => { closed = true; };


  assertThrowsEquals(() => {
    for (var x of g()) {}
  }, 666);

  assertThrowsEquals(() => {
    for (let x of g()) {}
  }, 666);

  assertThrowsEquals(() => {
    for (const x of g()) {}
  }, 666);

  assertThrowsEquals(() => {
    for (x of g()) {}
  }, 666);

  assertThrowsEquals(() => {
    var [x] = g();
  }, 666);

  assertThrowsEquals(() => {
    let [x] = g();
  }, 666);

  assertThrowsEquals(() => {
    const [x] = g();
  }, 666);

  assertThrowsEquals(() => {
    [x] = g();
  }, 666);

  assertThrowsEquals(() => {
    (([x]) => x)(g());
  }, 666);

  assertThrowsEquals(() => {
    var [...x] = g();
  }, 666);

  assertThrowsEquals(() => {
    let [...x] = g();
  }, 666);

  assertThrowsEquals(() => {
    const [...x] = g();
  }, 666);

  assertThrowsEquals(() => {
    [...x] = g();
  }, 666);

  assertThrowsEquals(() => {
    (([...x]) => x)(g());
  }, 666);


  assertFalse(closed);
}


// Value throws.
{
  let closed = false;
  g.prototype.next = () => ({get value() {throw 666}});
  g.prototype.return = () => { closed = true; };


  assertThrowsEquals(() => {
    for (var x of g()) {}
  }, 666);

  assertThrowsEquals(() => {
    for (let x of g()) {}
  }, 666);

  assertThrowsEquals(() => {
    for (const x of g()) {}
  }, 666);

  assertThrowsEquals(() => {
    for (x of g()) {}
  }, 666);

  assertThrowsEquals(() => {
    var [x] = g();
  }, 666);

  assertThrowsEquals(() => {
    let [x] = g();
  }, 666);

  assertThrowsEquals(() => {
    const [x] = g();
  }, 666);

  assertThrowsEquals(() => {
    [x] = g();
  }, 666);

  assertThrowsEquals(() => {
    (([x]) => x)(g());
  }, 666);

  assertThrowsEquals(() => {
    var [...x] = g();
  }, 666);

  assertThrowsEquals(() => {
    let [...x] = g();
  }, 666);

  assertThrowsEquals(() => {
    const [...x] = g();
  }, 666);

  assertThrowsEquals(() => {
    [...x] = g();
  }, 666);

  assertThrowsEquals(() => {
    (([...x]) => x)(g());
  }, 666);


  assertFalse(closed);
}


// Done throws.
{
  let closed = false;
  g.prototype.next = () => ({get done() {throw 666}});
  g.prototype.return = () => { closed = true; };


  assertThrowsEquals(() => {
    for (var x of g()) {}
  }, 666);

  assertThrowsEquals(() => {
    for (let x of g()) {}
  }, 666);

  assertThrowsEquals(() => {
    for (const x of g()) {}
  }, 666);

  assertThrowsEquals(() => {
    for (x of g()) {}
  }, 666);

  assertThrowsEquals(() => {
    var [x] = g();
  }, 666);

  assertThrowsEquals(() => {
    let [x] = g();
  }, 666);

  assertThrowsEquals(() => {
    const [x] = g();
  }, 666);

  assertThrowsEquals(() => {
    [x] = g();
  }, 666);

  assertThrowsEquals(() => {
    (([x]) => x)(g());
  }, 666);

  assertThrowsEquals(() => {
    var [...x] = g();
  }, 666);

  assertThrowsEquals(() => {
    let [...x] = g();
  }, 666);

  assertThrowsEquals(() => {
    const [...x] = g();
  }, 666);

  assertThrowsEquals(() => {
    [...x] = g();
  }, 666);

  assertThrowsEquals(() => {
    (([...x]) => x)(g());
  }, 666);


  assertFalse(closed);
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


// yield*, argument's return method is "undefined".
function TestYieldStarWithoutReturn(get_iterable) {
  assertTrue(get_iterable().return == undefined);

  function* g() { yield* get_iterable() }

  {
    let gen = g();
    assertEquals({value: 1, done: false}, gen.next());
    assertEquals({value: undefined, done: true}, gen.return());
  }

  assertEquals(42, (() => {
    for (let x of g()) break;
    return 42;
  })());

  assertEquals(42, (() => {
    for (let x of g()) return 42;
  })());

  assertThrowsEquals(() => {
    for (let x of g()) throw 42;
  }, 42);
}
{
  let get_iterable1 = () => [1, 2];
  let get_iterable2 = function*() { yield 1; yield 2 };
  get_iterable2.prototype.return = null;
  TestYieldStarWithoutReturn(get_iterable1);
  TestYieldStarWithoutReturn(get_iterable2);
}


// yield*, argument's return method is defined.
{
  let get_iterable = function*() { yield 1; yield 2 };
  const obj = {};
  get_iterable.prototype.return = (...args) => obj;

  function* g() { yield* get_iterable() }

  {
    let gen = g();
    assertEquals({value: 1, done: false}, gen.next());
    assertSame(obj, gen.return());
    assertSame(obj, gen.return());
    assertSame(obj, gen.return());
    assertEquals({value: 2, done: false}, gen.next());
    assertSame(obj, gen.return());
    assertSame(obj, gen.return());
    assertSame(obj, gen.return());
    assertEquals({value: undefined, done: true}, gen.next());
    assertEquals({value: undefined, done: true}, gen.return());
    assertEquals({value: undefined, done: true}, gen.return());
  }

  assertEquals(42, (() => {
    for (let x of g()) break;
    return 42;
  })());

  assertEquals(42, (() => {
    for (let x of g()) return 42;
  })());

  assertThrowsEquals(() => {
    for (let x of g()) throw 42;
  }, 42);
}
