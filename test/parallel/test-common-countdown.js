'use strict';

const common = require('../common');
const assert = require('assert');
const Countdown = require('../common/countdown');

let done = '';

const countdown = new Countdown(2, common.mustCall(() => done = true));
assert.strictEqual(countdown.remaining, 2);
countdown.dec();
assert.strictEqual(countdown.remaining, 1);
countdown.dec();
assert.strictEqual(countdown.remaining, 0);
assert.strictEqual(done, true);
