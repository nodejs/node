'use strict';

const common = require('../common');

const { strictEqual, ok } = require('assert');

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
