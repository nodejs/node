'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

// Verify which store values are visible after asyncLocalStorage.run() returns.

{
  const als1 = new AsyncLocalStorage();
  const als2 = new AsyncLocalStorage();

  als1.run('outer', common.mustCall(() => {
    als1.run('inner', common.mustCall(() => {
      als2.enterWith('entered');
      als1.enterWith('replaced');
      assert.strictEqual(als1.getStore(), 'replaced');
      assert.strictEqual(als2.getStore(), 'entered');
    }));
    assert.strictEqual(als1.getStore(), 'outer');
    assert.strictEqual(als2.getStore(), 'entered');
  }));
  assert.strictEqual(als1.getStore(), undefined);
  assert.strictEqual(als2.getStore(), 'entered');
  als2.disable();
}

{
  // enterWith() inside a run() with an unchanged value.
  const als1 = new AsyncLocalStorage();
  const als2 = new AsyncLocalStorage();

  als1.run(NaN, common.mustCall(() => {
    als1.run(NaN, common.mustCall(() => {
      als2.enterWith('entered');
    }));
    assert.strictEqual(als2.getStore(), 'entered');
  }));
  assert.strictEqual(als1.getStore(), undefined);
  assert.strictEqual(als2.getStore(), 'entered');
  als2.disable();
}

{
  const als1 = new AsyncLocalStorage();
  const als2 = new AsyncLocalStorage();
  const als3 = new AsyncLocalStorage();

  als2.enterWith('store2');
  als1.run('store1', common.mustCall(() => {
    als3.disable();
    als2.disable();
    assert.strictEqual(als1.getStore(), 'store1');
    assert.strictEqual(als2.getStore(), undefined);
  }));
  assert.strictEqual(als1.getStore(), undefined);
  assert.strictEqual(als2.getStore(), undefined);
  assert.strictEqual(als3.getStore(), undefined);

  als1.run('outer', common.mustCall(() => {
    als1.run('inner', common.mustCall(() => {
      als1.disable();
      assert.strictEqual(als1.getStore(), undefined);
    }));
    assert.strictEqual(als1.getStore(), 'outer');
  }));
  assert.strictEqual(als1.getStore(), undefined);

  als2.enterWith('store2');
  als1.run('store1', common.mustCall(() => {
    als3.disable();
  }));
  assert.strictEqual(als1.getStore(), undefined);
  assert.strictEqual(als2.getStore(), 'store2');
  als2.disable();
}

{
  // A store holding undefined does not fall back to defaultValue.
  const als = new AsyncLocalStorage({ defaultValue: 'default' });
  const other = new AsyncLocalStorage();

  assert.strictEqual(als.getStore(), 'default');
  als.run(undefined, common.mustCall(() => {
    assert.strictEqual(als.getStore(), undefined);
  }));
  assert.strictEqual(als.getStore(), 'default');

  als.run('value', common.mustCall(() => {
    als.exit(common.mustCall(() => {
      assert.strictEqual(als.getStore(), undefined);
    }));
    als.run(undefined, common.mustCall(() => {
      assert.strictEqual(als.getStore(), undefined);
    }));
    assert.strictEqual(als.getStore(), 'value');
  }));
  assert.strictEqual(als.getStore(), 'default');

  other.run('other', common.mustCall(() => {
    assert.strictEqual(als.getStore(), 'default');
    als.run(undefined, common.mustCall(() => {
      assert.strictEqual(als.getStore(), undefined);
      assert.strictEqual(other.getStore(), 'other');
    }));
    assert.strictEqual(als.getStore(), 'default');
  }));
  assert.strictEqual(als.getStore(), 'default');
  assert.strictEqual(other.getStore(), undefined);

  als.run('value', common.mustCall(() => {
    als.disable();
    assert.strictEqual(als.getStore(), 'default');
  }));
  assert.strictEqual(als.getStore(), 'default');
}

{
  const als1 = new AsyncLocalStorage();
  const als2 = new AsyncLocalStorage();

  als1.run('outer', common.mustCall(() => {
    als2.run('store2', common.mustCall(() => {
      assert.throws(() => {
        als1.run('inner', common.mustCall(() => {
          assert.strictEqual(als1.getStore(), 'inner');
          throw new Error('boom');
        }));
      }, /boom/);
      assert.strictEqual(als1.getStore(), 'outer');
      assert.strictEqual(als2.getStore(), 'store2');
    }));
  }));

  assert.throws(() => {
    als1.run('value', common.mustCall(() => {
      throw new Error('boom');
    }));
  }, /boom/);
  assert.strictEqual(als1.getStore(), undefined);
  assert.strictEqual(als2.getStore(), undefined);
}

{
  const als1 = new AsyncLocalStorage();
  const als2 = new AsyncLocalStorage();

  als1.run('a1', common.mustCall(() => {
    als2.run('b1', common.mustCall(() => {
      als1.run('a2', common.mustCall(() => {
        assert.strictEqual(als1.getStore(), 'a2');
        assert.strictEqual(als2.getStore(), 'b1');
        setImmediate(common.mustCall(() => {
          assert.strictEqual(als1.getStore(), 'a2');
          assert.strictEqual(als2.getStore(), 'b1');
        }));
      }));
      assert.strictEqual(als1.getStore(), 'a1');
      assert.strictEqual(als2.getStore(), 'b1');
      als2.run('b2', common.mustCall(() => {
        assert.strictEqual(als1.getStore(), 'a1');
        assert.strictEqual(als2.getStore(), 'b2');
      }));
      assert.strictEqual(als2.getStore(), 'b1');
    }));
    assert.strictEqual(als1.getStore(), 'a1');
    assert.strictEqual(als2.getStore(), undefined);
  }));
  assert.strictEqual(als1.getStore(), undefined);
  assert.strictEqual(als2.getStore(), undefined);

  const promise = als1.run('async', common.mustCall(async () => {
    await 1;
    assert.strictEqual(als1.getStore(), 'async');
    await als2.run('nested', common.mustCall(async () => {
      await 1;
      assert.strictEqual(als1.getStore(), 'async');
      assert.strictEqual(als2.getStore(), 'nested');
    }));
    assert.strictEqual(als2.getStore(), undefined);
    return als1.getStore();
  }));
  assert.strictEqual(als1.getStore(), undefined);
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, 'async');
    assert.strictEqual(als1.getStore(), undefined);
  }));
}
