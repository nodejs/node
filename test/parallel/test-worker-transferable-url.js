'use strict';

const common = require('../common');
const assert = require('assert');

const mc = new MessageChannel();
const source = new URL('https://abc:123@example.org:80/a/b/c?d=e#f');

mc.port2.onmessage = common.mustCall(({ data }) => {
  assert(data instanceof URL);
  assert.strictEqual(data.href, source.href);
  mc.port2.close();
});

mc.port1.unref();
mc.port1.postMessage(source);
