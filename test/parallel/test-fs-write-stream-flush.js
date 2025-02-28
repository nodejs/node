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

test('validation', () => {
  for (const flush of ['true', '', 0, 1, [], {}, Symbol()]) {
    assert.throws(() => {
      fs.createWriteStream(nextFile(), { flush });
    }, { code: 'ERR_INVALID_ARG_TYPE' });
  }
});

test('performs flush', (t, done) => {
  const spy = t.mock.method(fs, 'fsync');
  const file = nextFile();
  const stream = fs.createWriteStream(file, { flush: true });

  stream.write(data, common.mustSucceed(() => {
    stream.close(common.mustSucceed(() => {
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
  }));
});

test('does not perform flush', (t, done) => {
  const values = [undefined, null, false];
  const spy = t.mock.method(fs, 'fsync');
  let cnt = 0;

  for (const flush of values) {
    const file = nextFile();
    const stream = fs.createWriteStream(file, { flush });

    stream.write(data, common.mustSucceed(() => {
      stream.close(common.mustSucceed(() => {
        assert.strictEqual(fs.readFileSync(file, 'utf8'), data);
        cnt++;

        if (cnt === values.length) {
          assert.strictEqual(spy.mock.calls.length, 0);
          done();
        }
      }));
    }));
  }
});

test('works with file handles', async () => {
  const file = nextFile();
  const handle = await fsp.open(file, 'w');
  const stream = handle.createWriteStream({ flush: true });

  return new Promise((resolve) => {
    stream.write(data, common.mustSucceed(() => {
      stream.close(common.mustSucceed(() => {
        assert.strictEqual(fs.readFileSync(file, 'utf8'), data);
        resolve();
      }));
    }));
  });
});
