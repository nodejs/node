// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/test-async.js');

function getBrokenPromise() {
  let brokenPromise = Promise.resolve(42);
  Object.defineProperty(brokenPromise, 'constructor', {
    get: function () {
      throw new Error('broken promise');
    }
  });
  return brokenPromise;
}

testAsync(test => {
  test.plan(1);

  let gen = (async function* () { })();

  gen.return(getBrokenPromise())
    .then(
      () => {
        test.unreachable();
      },
      (err) => {
        test.equals(err.message, 'broken promise');
      }
    );
}, "close suspendedStart async generator");

testAsync(test => {
  test.plan(1);

  let unblock;
  let blocking = new Promise(res => { unblock = res; });

  let gen = (async function* (){ await blocking; })();
  gen.next();

  gen.return(getBrokenPromise())
    .then(
      () => {
        test.unreachable();
      },
      (err) => {
        test.equals(err.message, 'broken promise');
      }
    );

  unblock();
}, "close blocked suspendedStart async generator");

testAsync(test => {
  test.plan(2);

  let gen = (async function* (){
    try {
      yield 1;
    } catch (err) {
      test.equals(err.message, 'broken promise');
      return 2;
    }
  })();
  gen.next()
    .then(() => {
      try {
        return gen.return(getBrokenPromise())
      } catch {
        test.unreachable();
      }
    })
    .then(
      val => {
        test.equals(val, {done: true, value: 2});
      },
      test.unexpectedRejection()
    );
}, "resume suspendedYield async generator with throw completion");
