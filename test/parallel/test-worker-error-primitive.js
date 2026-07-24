'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');

const cases = [
  { code: 'throw "boom";', value: 'boom' },
  { code: 'throw 42;', value: 42 },
  { code: 'throw 7n;', value: 7n },
  { code: 'throw true;', value: true },
  { code: 'throw null;', value: null },
  { code: 'throw undefined;', value: undefined },
  { code: 'throw Symbol.for("a");', value: Symbol.for('a') },
];

for (const { code, value } of cases) {
  const worker = new Worker(code, { eval: true });
  worker.on('message', common.mustNotCall());
  worker.on('error', common.mustCall((err) => {
    assert.strictEqual(err, value);
  }));
  worker.on('exit', common.mustCall((exitCode) => {
    assert.strictEqual(exitCode, 1);
  }));
}
