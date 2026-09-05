// Flags: --no-warnings
'use strict';

const common = require('../common');
const { completeSample } = require('../common/bench');
const assert = require('assert');
const { setImmediate } = require('timers/promises');
const {
  after,
  afterEach,
  before,
  beforeEach,
  bench,
  run,
  suite,
} = require('node:bench');

suite('before failure', () => {
  before(() => { throw new Error('before failure'); });
  after(common.mustCall());
  bench('blocked by before', { samples: 1 }, common.mustNotCall());
});

suite('beforeEach failure', () => {
  beforeEach(() => { throw new Error('beforeEach failure'); });
  afterEach(common.mustCall());
  bench('blocked by beforeEach', { samples: 1 }, common.mustNotCall());
});

suite('after failure', () => {
  after(() => { throw new Error('after failure'); });
  bench('completes before after', { samples: 1 }, completeSample);
});

suite('build failure', async () => {
  await setImmediate();
  throw new Error('build failure');
});

bench('continues after suite failures', { samples: 1 }, completeSample);

const completions = [];
const diagnostics = [];
let summary;
const stream = run();
stream.on('bench:complete', (result) => completions.push(result));
stream.on('bench:diagnostic', (diagnostic) => {
  diagnostics.push(diagnostic);
});
stream.on('bench:summary', (value) => { summary = value; });
stream.on('end', common.mustCall(() => {
  assert.deepStrictEqual(summary.counts, {
    __proto__: null,
    completed: 2,
    failed: 2,
    skipped: 0,
    total: 4,
  });
  assert.strictEqual(summary.success, false);

  const byName = new Map(completions.map((result) => [result.name, result]));
  assert.strictEqual(byName.get('blocked by before').error.message,
                     'before failure');
  assert.strictEqual(byName.get('blocked by beforeEach').error.message,
                     'beforeEach failure');
  assert.strictEqual(byName.get('completes before after').error, undefined);
  assert.strictEqual(
    byName.get('continues after suite failures').error, undefined);

  assert.deepStrictEqual(
    diagnostics.map(({ message }) => message).sort(),
    ['after failure', 'before failure', 'build failure'],
  );
  for (const diagnostic of diagnostics) {
    assert.strictEqual(typeof diagnostic.file, 'string');
    assert.strictEqual(typeof diagnostic.line, 'number');
    assert.strictEqual(typeof diagnostic.column, 'number');
  }
}));
stream.resume();
