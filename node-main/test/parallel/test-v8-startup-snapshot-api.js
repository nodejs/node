'use strict';

require('../common');
const assert = require('assert');

const {
  isBuildingSnapshot,
  addSerializeCallback,
  addDeserializeCallback,
  setDeserializeMainFunction
} = require('v8').startupSnapshot;

// This test verifies that the v8.startupSnapshot APIs are not available when
// it is not building snapshot.

assert(!isBuildingSnapshot());

assert.throws(() => addSerializeCallback(() => {}), {
  code: 'ERR_NOT_BUILDING_SNAPSHOT',
});
assert.throws(() => addDeserializeCallback(() => {}), {
  code: 'ERR_NOT_BUILDING_SNAPSHOT',
});
assert.throws(() => setDeserializeMainFunction(() => {}), {
  code: 'ERR_NOT_BUILDING_SNAPSHOT',
});
