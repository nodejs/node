'use strict';

const common = require('../common');
const assert = require('assert');
const Countdown = require('../common/countdown');
const fixtures = require('../common/fixtures');
const { execFile } = require('child_process');

let done = '';
const countdown = new Countdown(2, () => done = true);
assert.strictEqual(countdown.remaining, 2);
countdown.dec();
assert.strictEqual(countdown.remaining, 1);
countdown.dec();
assert.strictEqual(countdown.remaining, 0);
assert.strictEqual(done, true);

const failFixtures = [
  [
    fixtures.path('failcounter.js'),
    'Mismatched <anonymous> function calls. Expected exactly 1, actual 0.',
  ],
];

for (const p of failFixtures) {
  const [file, expected] = p;
  execFile(process.argv[0], [file], common.mustCall((ex, stdout, stderr) => {
    assert.ok(ex);
    assert.strictEqual(stderr, '');
    const firstLine = stdout.split('\n').shift();
    assert.strictEqual(firstLine, expected);
  }));
}
