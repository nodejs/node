'use strict';

const common = require('../common');
const assert = require('node:assert');
const { time, encode, encodeSync } = require('node:perf_hooks/pprof');

for (const options of [null, 1, 'string', true]) {
  assert.throws(() => time.start(options), { code: 'ERR_INVALID_ARG_TYPE' });
}

for (const intervalMicros of ['1', true]) {
  assert.throws(() => time.start({ intervalMicros }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}
for (const intervalMicros of [1.5, 0, -1, 2 ** 31]) {
  assert.throws(() => time.start({ intervalMicros }), {
    code: 'ERR_OUT_OF_RANGE',
  });
}

for (const durationMillis of ['1', true]) {
  assert.throws(() => time.start({ durationMillis }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}
for (const durationMillis of [1.5, 0, -1, 2 ** 31]) {
  assert.throws(() => time.start({ durationMillis }), {
    code: 'ERR_OUT_OF_RANGE',
  });
}
// The duration must not be shorter than the interval.
assert.throws(() => time.start({ intervalMicros: 5000, durationMillis: 4 }), {
  code: 'ERR_OUT_OF_RANGE',
});

for (const name of ['lineNumbers', 'collectCpuTime', 'workaroundV8Bug']) {
  assert.throws(() => time.start({ [name]: 1 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

if (common.isWindows) {
  assert.throws(() => time.start({ collectCpuTime: true }), {
    code: 'ERR_FEATURE_UNAVAILABLE_ON_PLATFORM',
  });
} else {
  assert.throws(() => time.start({ collectCpuTime: true, lineNumbers: true }), {
    code: 'ERR_INCOMPATIBLE_OPTION_PAIR',
  });
}

assert.strictEqual(time.isStarted(), false);

assert.throws(() => time.stop(), { code: 'ERR_INVALID_STATE' });
assert.throws(() => time.stop(true), { code: 'ERR_INVALID_STATE' });

time.start();
assert.strictEqual(time.isStarted(), true);
assert.throws(() => time.start(), { code: 'ERR_INVALID_STATE' });
assert.throws(() => time.stop('true'), { code: 'ERR_INVALID_ARG_TYPE' });
time.stop();
assert.strictEqual(time.isStarted(), false);

for (const profile of [undefined, null, {}, Buffer.alloc(0)]) {
  assert.throws(() => encodeSync(profile), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.rejects(encode(profile), { code: 'ERR_INVALID_ARG_TYPE' })
    .then(common.mustCall());
}

assert.rejects(time.profile({ signal: 'signal' }), {
  code: 'ERR_INVALID_ARG_TYPE',
}).then(common.mustCall());
