// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');

const {
  MessageEvent,
} = require('internal/worker/io');

[
  'data',
  'origin',
  'lastEventId',
  'source',
  'ports',
].forEach((i) => {
  assert.throws(() => Reflect.get(MessageEvent.prototype, i, {}), {
    code: 'ERR_INVALID_THIS',
  });
});
