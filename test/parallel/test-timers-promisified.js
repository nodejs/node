// Flags: --no-warnings
'use strict';
const common = require('../common');
const assert = require('assert');
const timers = require('timers');
const { promisify } = require('util');

/* eslint-disable no-restricted-syntax */

const setTimeout = promisify(timers.setTimeout);
const setImmediate = promisify(timers.setImmediate);

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
}
