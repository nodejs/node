'use strict';

const common = require('../common');
const assert = require('node:assert');
const dc = require('node:diagnostics_channel');

let last;
for (let i = 0; i < 1024 * 10 + 1; i++) {
  last = dc.channel(`test:many-channels:${i}`);
}

const onMessage = common.mustCall((message, name) => {
  assert.strictEqual(message, 'message');
  assert.strictEqual(name, last.name);
});

last.subscribe(onMessage);
last.publish('message');
assert.strictEqual(last.unsubscribe(onMessage), true);
