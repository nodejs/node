'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const fs = require('node:fs');
const fsp = require('node:fs/promises');
const test = require('node:test');
const data = 'foo';
let cnt = 0;

function nextFile() {
  return tmpdir.resolve(`${cnt++}.out`);
}

tmpdir.refresh();

test('synchronous version', async (t) => {
  await t.test('validation', (t) => {
    for (const v of ['true', '', 0, 1, [], {}, Symbol()]) {
      assert.throws(() => {
        fs.writeFileSync(nextFile(), data, { flush: v });
      }, { code: 'ERR_INVALID_ARG_TYPE' });
    }
  });

  await t.test('performs flush', (t) => {
    const spy = t.mock.method(fs, 'fsyncSync');
    const file = nextFile();
    fs.writeFileSync(file, data, { flush: true });
    const calls = spy.mock.calls;
    assert.strictEqual(calls.length, 1);
    assert.strictEqual(calls[0].result, undefined);
    assert.strictEqual(calls[0].error, undefined);
    assert.strictEqual(calls[0].arguments.length, 1);
    assert.strictEqual(typeof calls[0].arguments[0], 'number');
    assert.strictEqual(fs.readFileSync(file, 'utf8'), data);
  });

  await t.test('does not perform flush', (t) => {
    const spy = t.mock.method(fs, 'fsyncSync');

    for (const v of [undefined, null, false]) {
      const file = nextFile();
      fs.writeFileSync(file, data, { flush: v });
      assert.strictEqual(fs.readFileSync(file, 'utf8'), data);
    }

    assert.strictEqual(spy.mock.calls.length, 0);
  });
});

test('callback version', async (t) => {
  await t.test('validation', (t) => {
    for (const v of ['true', '', 0, 1, [], {}, Symbol()]) {
      assert.throws(() => {
        fs.writeFileSync(nextFile(), data, { flush: v });
      }, { code: 'ERR_INVALID_ARG_TYPE' });
    }
  });

  await t.test('performs flush', (t, done) => {
    const spy = t.mock.method(fs, 'fsync');
    const file = nextFile();
    fs.writeFile(file, data, { flush: true }, common.mustSucceed(() => {
      const calls = spy.mock.calls;
      assert.strictEqual(calls.length, 1);
      assert.strictEqual(calls[0].result, undefined);
      assert.strictEqual(calls[0].error, undefined);
      assert.strictEqual(calls[0].arguments.length, 2);
      assert.strictEqual(typeof calls[0].arguments[0], 'number');
      assert.strictEqual(typeof calls[0].arguments[1], 'function');
      assert.strictEqual(fs.readFileSync(file, 'utf8'), data);
      done();
    }));
  });

  await t.test('does not perform flush', (t, done) => {
    const values = [undefined, null, false];
    const spy = t.mock.method(fs, 'fsync');
    let cnt = 0;

    for (const v of values) {
      const file = nextFile();

      fs.writeFile(file, data, { flush: v }, common.mustSucceed(() => {
        assert.strictEqual(fs.readFileSync(file, 'utf8'), data);
        cnt++;

        if (cnt === values.length) {
          assert.strictEqual(spy.mock.calls.length, 0);
          done();
        }
      }));
    }
  });
});

test('promise based version', async (t) => {
  await t.test('validation', async (t) => {
    for (const v of ['true', '', 0, 1, [], {}, Symbol()]) {
      await assert.rejects(() => {
        return fsp.writeFile(nextFile(), data, { flush: v });
      }, { code: 'ERR_INVALID_ARG_TYPE' });
    }
  });

  await t.test('success path', async (t) => {
    for (const v of [undefined, null, false, true]) {
      const file = nextFile();
      await fsp.writeFile(file, data, { flush: v });
      assert.strictEqual(await fsp.readFile(file, 'utf8'), data);
    }
  });
});
