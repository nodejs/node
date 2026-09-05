// Flags: --no-warnings
'use strict';

const common = require('../common');
const { completeSample } = require('../common/bench');
const assert = require('assert');
const { Writable } = require('stream');
const { finished } = require('stream/promises');
const { bench, run } = require('node:bench');

bench('completed', { samples: 1 }, (b) => {
  completeSample(b);
});
bench.skip('skipped', { samples: 1 }, common.mustNotCall());

async function* customReporter(source) {
  for await (const { type, data } of source) {
    if (type !== 'bench:complete') continue;
    yield `${data.name}:${data.skip === undefined ? 'completed' : 'skipped'}\n`;
  }
}

let output = '';
const destination = new Writable({
  write(chunk, _encoding, callback) {
    output += chunk;
    callback();
  },
});

run().compose(customReporter).pipe(destination);

(async () => {
  await finished(destination);
  assert.strictEqual(output, 'completed:completed\nskipped:skipped\n');
})().then(common.mustCall());
