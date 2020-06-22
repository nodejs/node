// Flags: --no-warnings
'use strict';
const common = require('../common');
const assert = require('assert');
const timers = require('timers');
const { promisify } = require('util');
const child_process = require('child_process');

const timerPromises = require('timers/promises');

/* eslint-disable no-restricted-syntax */

const setTimeout = promisify(timers.setTimeout);
const setImmediate = promisify(timers.setImmediate);
const exec = promisify(child_process.exec);

assert.strictEqual(setTimeout, timerPromises.setTimeout);
assert.strictEqual(setImmediate, timerPromises.setImmediate);

process.on('multipleResolves', common.mustNotCall());

{
  const promise = setTimeout(1);
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, undefined);
  }));
}

{
  const promise = setTimeout(1, 'foobar');
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, 'foobar');
  }));
}

{
  const promise = setImmediate();
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, undefined);
  }));
}

{
  const promise = setImmediate('foobar');
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, 'foobar');
  }));
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  assert.rejects(setTimeout(10, undefined, { signal }), /AbortError/);
  ac.abort();
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  ac.abort(); // Abort in advance
  assert.rejects(setTimeout(10, undefined, { signal }), /AbortError/);
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  assert.rejects(setImmediate(10, { signal }), /AbortError/);
  ac.abort();
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  ac.abort(); // Abort in advance
  assert.rejects(setImmediate(10, { signal }), /AbortError/);
}

{
  // Check that aborting after resolve will not reject.
  const ac = new AbortController();
  const signal = ac.signal;
  setTimeout(10, undefined, { signal }).then(() => {
    ac.abort();
  });
}
{
  // Check that aborting after resolve will not reject.
  const ac = new AbortController();
  const signal = ac.signal;
  setImmediate(10, { signal }).then(() => {
    ac.abort();
  });
}

{
  Promise.all(
    [1, '', false, Infinity].map((i) => assert.rejects(setImmediate(10, i)), {
      code: 'ERR_INVALID_ARG_TYPE'
    })).then(common.mustCall());

  Promise.all(
    [1, '', false, Infinity, null, {}].map(
      (signal) => assert.rejects(setImmediate(10, { signal })), {
        code: 'ERR_INVALID_ARG_TYPE'
      })).then(common.mustCall());

  Promise.all(
    [1, '', false, Infinity].map(
      (i) => assert.rejects(setTimeout(10, null, i)), {
        code: 'ERR_INVALID_ARG_TYPE'
      })).then(common.mustCall());

  Promise.all(
    [1, '', false, Infinity, null, {}].map(
      (signal) => assert.rejects(setTimeout(10, null, { signal })), {
        code: 'ERR_INVALID_ARG_TYPE'
      })).then(common.mustCall());

  Promise.all(
    [1, '', Infinity, null, {}].map(
      (ref) => assert.rejects(setTimeout(10, null, { ref })), {
        code: 'ERR_INVALID_ARG_TYPE'
      })).then(common.mustCall());
}

{
  exec(`${process.execPath} -pe "const assert = require('assert');` +
       'require(\'timers/promises\').setTimeout(1000, null, { ref: false }).' +
       'then(assert.fail)"').then(common.mustCall(({ stderr }) => {
    assert.strictEqual(stderr, '');
  }));
}

{
  exec(`${process.execPath} -pe "const assert = require('assert');` +
       'require(\'timers/promises\').setImmediate(null, { ref: false }).' +
       'then(assert.fail)"').then(common.mustCall(({ stderr }) => {
    assert.strictEqual(stderr, '');
  }));
}
