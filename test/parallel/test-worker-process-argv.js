'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread, workerData } = require('worker_threads');

if (isMainThread) {
  assert.throws(() => {
    new Worker(__filename, { argv: 'foo' });
  }, {
    code: 'ERR_INVALID_ARG_TYPE'
  });

  [
    new Worker(__filename, {
      argv: [null, 'foo', 123, Symbol('bar')],
      // Asserts only if the worker is started by the test.
      workerData: 'assert-argv'
    }),
    new Worker(`
      const assert = require('assert');
      assert.deepStrictEqual(
        process.argv,
        [process.execPath, '[worker eval]']
      );
    `, {
      eval: true
    }),
    new Worker(`
      const assert = require('assert');
      assert.deepStrictEqual(
        process.argv,
        [process.execPath, '[worker eval]', 'null', 'foo', '123',
        String(Symbol('bar'))]
      );
    `, {
      argv: [null, 'foo', 123, Symbol('bar')],
      eval: true
    }),
  ].forEach((worker) => {
    worker.on('exit', common.mustCall((code) => {
      assert.strictEqual(code, 0);
    }));
  });
} else if (workerData === 'assert-argv') {
  assert.deepStrictEqual(
    process.argv,
    [process.execPath, __filename, 'null', 'foo', '123', String(Symbol('bar'))]
  );
}
