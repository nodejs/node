// Flags: --no-warnings
'use strict';

const common = require('../common');

if (!common.hasQuic)
  common.skip('quic support is not enabled');

const assert = require('assert');

const {
  StreamOptions,
  ResponseOptions,
} = require('net/quic');

const {
  inspect,
} = require('util');

{
  const opt = new StreamOptions();
  assert('unidirectional' in opt);
  assert('headers' in opt);
  assert('trailers' in opt);
  assert('body' in opt);
  assert.strictEqual(opt.unidirectional, false);
  assert.strictEqual(opt.headers, undefined);
  assert.strictEqual(opt.trailers, undefined);
  assert.strictEqual(opt.body, undefined);
  assert.match(inspect(opt), /StreamOptions {/);
}

{
  const opt = new ResponseOptions();
  assert('hints' in opt);
  assert('headers' in opt);
  assert('trailers' in opt);
  assert('body' in opt);
  assert.strictEqual(opt.hints, undefined);
  assert.strictEqual(opt.headers, undefined);
  assert.strictEqual(opt.trailers, undefined);
  assert.strictEqual(opt.body, undefined);
  assert.match(inspect(opt), /ResponseOptions {/);
}

assert.throws(
  () => Reflect.get(StreamOptions.prototype, 'unidirectional', {}), {
    code: 'ERR_INVALID_THIS',
  });

assert.throws(
  () => Reflect.get(StreamOptions.prototype, 'headers', {}), {
    code: 'ERR_INVALID_THIS',
  });

assert.throws(
  () => Reflect.get(StreamOptions.prototype, 'trailers', {}), {
    code: 'ERR_INVALID_THIS',
  });

assert.throws(
  () => Reflect.get(StreamOptions.prototype, 'body', {}), {
    code: 'ERR_INVALID_THIS',
  });

assert.throws(
  () => Reflect.get(ResponseOptions.prototype, 'hints', {}), {
    code: 'ERR_INVALID_THIS',
  });

assert.throws(
  () => Reflect.get(ResponseOptions.prototype, 'headers', {}), {
    code: 'ERR_INVALID_THIS',
  });

assert.throws(
  () => Reflect.get(ResponseOptions.prototype, 'trailers', {}), {
    code: 'ERR_INVALID_THIS',
  });

assert.throws(
  () => Reflect.get(ResponseOptions.prototype, 'body', {}), {
    code: 'ERR_INVALID_THIS',
  });

[1, 'hello', false, null].forEach((i) => {
  assert.throws(() => new StreamOptions(i), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => new ResponseOptions(i), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

[1, 'hello', {}, null].forEach((unidirectional) => {
  assert.throws(() => new StreamOptions({ unidirectional }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

// Anything will work for headers, trailers, and body here. They
// are validated when they are used, not when the StreamOptions
// is created.
[1, {}, 'hello', false, null].forEach((value) => {
  const so = new StreamOptions({
    headers: value,
    trailers: value,
    body: value,
  });
  assert.strictEqual(so.headers, value);
  assert.strictEqual(so.trailers, value);
  assert.strictEqual(so.body, value);

  const ro = new ResponseOptions({
    hints: value,
    headers: value,
    trailers: value,
    body: value,
  });
  assert.strictEqual(ro.hints, value);
  assert.strictEqual(ro.headers, value);
  assert.strictEqual(ro.trailers, value);
  assert.strictEqual(ro.body, value);
});
