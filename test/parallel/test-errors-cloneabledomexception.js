// Flags: --expose-internals
'use strict';

const common = require('../common');

const {
  strictEqual,
  notStrictEqual,
  ok,
} = require('assert');
const { AbortError } = require('internal/errors');

const exception = new DOMException('foo', 'AbortError');
strictEqual(exception.name, 'AbortError');
strictEqual(exception.message, 'foo');
strictEqual(exception.code, 20);

const mc = new MessageChannel();
mc.port1.onmessage = common.mustCall(({ data }) => {
  ok(data instanceof DOMException);
  strictEqual(data.name, 'AbortError');
  strictEqual(data.message, 'foo');
  strictEqual(data.code, 20);
  strictEqual(data.stack, exception.stack);
  mc.port1.close();
});
mc.port2.postMessage(exception);

// Let's make sure AbortError is cloneable also
const abort = new AbortError();
const mc2 = new MessageChannel();
mc2.port1.onmessage = common.mustCall(({ data }) => {
  ok(data instanceof AbortError);
  ok(data instanceof Error);
  notStrictEqual(data, abort);
  strictEqual(data.name, abort.name);
  strictEqual(data.message, abort.message);
  strictEqual(data.code, abort.code);
  mc2.port1.close();
});
mc2.port2.postMessage(abort);
