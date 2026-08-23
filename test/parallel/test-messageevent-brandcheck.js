'use strict';

require('../common');
const assert = require('assert');

for (const i of ['data', 'origin', 'lastEventId', 'source', 'ports']) {
  assert.throws(() => Reflect.get(MessageEvent.prototype, i, {}), TypeError);
}
