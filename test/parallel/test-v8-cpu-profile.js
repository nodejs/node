'use strict';

const common = require('../common');
const assert = require('assert');
const v8 = require('v8');

{
  const handle = v8.startCpuProfile();
  const profile = handle.stop();
  assert.ok(typeof profile === 'string');
  assert.ok(profile.length > 0);
  // Call stop() again
  assert.ok(handle.stop() === undefined);
}
{
  assert.throws(
    () => v8.startCpuProfile({ sampleInterval: -1 }),
    common.expectsError({ code: 'ERR_OUT_OF_RANGE' }));

  assert.throws(
    () => v8.startCpuProfile({ maxBufferSize: 0 }),
    common.expectsError({ code: 'ERR_OUT_OF_RANGE' }));
}
