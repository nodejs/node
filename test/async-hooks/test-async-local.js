'use strict';
const common = require('../common');

// This test verifys the AsyncLocal functionality.

const assert = require('assert');
const { AsyncLocal, AsyncResource } = require('async_hooks');

{
  common.expectsError(
    () => new AsyncLocal(15),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "options" argument must be of type Object. ' +
               'Received type number'
    }
  );

  common.expectsError(
    () => new AsyncLocal({ onChangedCb: {} }),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "options.onChangedCb" property must be of type ' +
               'function. Received type object'
    }
  );
}

{
  const asyncLocal1 = new AsyncLocal();
  const asyncLocal2 = new AsyncLocal();
  const asyncLocal3 = new AsyncLocal();

  assert.strictEqual(asyncLocal1.value, undefined);
  asyncLocal1.value = 'one';
  asyncLocal2.value = 'two';
  asyncLocal3.value = 'three';

  setImmediate(common.mustCall(() => {
    assert.strictEqual(asyncLocal1.value, 'one');
    assert.strictEqual(asyncLocal2.value, 'two');
    assert.strictEqual(asyncLocal3.value, 'three');
    setImmediate(common.mustCall(() => {
      assert.strictEqual(asyncLocal1.value, 'one');
      assert.strictEqual(asyncLocal2.value, 'two');
      assert.strictEqual(asyncLocal3.value, 'three');
    }));
    asyncLocal1.value = null;
    asyncLocal3.value = 'four';
    assert.strictEqual(asyncLocal1.value, undefined);
    assert.strictEqual(asyncLocal2.value, 'two');
    assert.strictEqual(asyncLocal3.value, 'four');
    setImmediate(common.mustCall(() => {
      assert.strictEqual(asyncLocal1.value, undefined);
      assert.strictEqual(asyncLocal2.value, 'two');
      assert.strictEqual(asyncLocal3.value, 'four');
    }));
  }));
}

{
  async function asyncFunc() {}

  const asyncLocal = new AsyncLocal();

  async function testAwait() {
    asyncLocal.value = 42;
    await asyncFunc();
    assert.strictEqual(asyncLocal.value, 42);
  }
  testAwait().then(common.mustCall(() =>
    assert.strictEqual(asyncLocal.value, 42)
  ));
}

{
  const asyncLocal = new AsyncLocal();
  const mutableObj = { a: 'b' };

  asyncLocal.value = mutableObj;
  process.nextTick(common.mustCall(() => {
    assert.deepStrictEqual(mutableObj, { a: 'b', b: 'a' });
  }));
  mutableObj.b = 'a';
}

{
  const exp = [
    [ undefined, 'foo', false ],
    [ 'foo', undefined, false ],
    [ undefined, 'bar', false ]
  ];

  const act = [];
  const asyncLocal = new AsyncLocal({
    onChangedCb: (p, c, t) => act.push([p, c, t])
  });

  asyncLocal.value = 'foo';
  assert.strictEqual(act.length, 1);

  asyncLocal.value = null;
  assert.strictEqual(act.length, 2);

  asyncLocal.value = 'bar';
  assert.strictEqual(act.length, 3);

  assert.deepStrictEqual(act, exp);
}

{
  const asyncLocal = new AsyncLocal();
  const asyncRes1 = new AsyncResource('Resource1');
  asyncLocal.value = 'R';
  const asyncRes2 = new AsyncResource('Resource2');

  asyncRes1.runInAsyncScope(common.mustCall(() => {
    assert.strictEqual(asyncLocal.value, undefined);
    asyncRes2.runInAsyncScope(common.mustCall(() =>
      assert.strictEqual(asyncLocal.value, 'R')
    ));
    assert.strictEqual(asyncLocal.value, undefined);
  }));
  assert.strictEqual(asyncLocal.value, 'R');
}

{
  const exp = [
    [ undefined, 'foo', false ],
    [ 'foo', 'bar', false ],
    [ 'bar', 'foo', true ],
    [ 'foo', 'bar', true ],
    [ 'bar', 'foo', true ],
    [ 'foo', undefined, true ],
    [ undefined, 'foo', true ],
    [ 'foo', undefined, true ],
    [ undefined, 'bar', true ],
  ];

  const act = [];
  const asyncLocal = new AsyncLocal({
    onChangedCb: (p, c, t) => act.push([p, c, t])
  });

  process.nextTick(common.mustCall(() => {
    asyncLocal.value = 'foo';
    assert.strictEqual(act.length, 1);

    const r1 = new AsyncResource('R1');
    const r2 = new AsyncResource('R2');

    r1.runInAsyncScope(common.mustCall(() => {
      asyncLocal.value = 'bar';
      assert.strictEqual(act.length, 2);

      r2.runInAsyncScope(common.mustCall(() => {
        assert.strictEqual(act.length, 3);

        setImmediate(common.mustCall(() => {
          assert.strictEqual(act.length, 7);
        }));
      }));

      setImmediate(common.mustCall(() => {
        assert.strictEqual(act.length, 9);
        assert.deepStrictEqual(act, exp);
      }));
      assert.strictEqual(act.length, 4);
    }));
  }));
}

{
  const exp = [
    [ undefined, 'A', false ],
    [ 'A', 'B', false ],
    [ 'B', 'A', true ],
    [ 'A', 'B', true ],
    [ 'B', 'A', true ],
  ];

  const act = [];
  const asyncLocal = new AsyncLocal({
    onChangedCb: (p, c, t) => act.push([p, c, t])
  });

  asyncLocal.value = 'A';
  const asyncRes1 = new AsyncResource('Resource1');
  const asyncRes2 = new AsyncResource('Resource2');
  assert.strictEqual(act.length, 1);

  asyncRes1.runInAsyncScope(common.mustCall(() => {
    assert.strictEqual(asyncLocal.value, 'A');
    asyncRes1.runInAsyncScope(common.mustCall(() => {
      assert.strictEqual(asyncLocal.value, 'A');
      asyncRes2.runInAsyncScope(common.mustCall(() => {
        assert.strictEqual(asyncLocal.value, 'A');
        asyncLocal.value = 'B';
        assert.strictEqual(act.length, 2);
        asyncRes1.runInAsyncScope(common.mustCall(() => {
          assert.strictEqual(asyncLocal.value, 'A');
          assert.strictEqual(act.length, 3);
        }));
        assert.strictEqual(act.length, 4);
        assert.strictEqual(asyncLocal.value, 'B');
      }));
      assert.strictEqual(act.length, 5);
      assert.strictEqual(asyncLocal.value, 'A');
    }));
    assert.strictEqual(asyncLocal.value, 'A');
  }));

  assert.strictEqual(act.length, 5);

  assert.deepStrictEqual(act, exp);
}
