'use strict';

require('../common');
const assert = require('assert');
const { Agent } = require('http');

{
  const agent = new Agent({});
  assert.strictEqual(agent.keepAliveTimeoutBuffer, 1000);
}

{
  const buffer = 2500;
  const agent = new Agent({ keepAliveTimeoutBuffer: buffer });
  assert.strictEqual(agent.keepAliveTimeoutBuffer, buffer);
}

{
  assert.throws(() => {
    new Agent({ keepAliveTimeoutBuffer: -1 });
  }, { code: 'ERR_OUT_OF_RANGE' });

  assert.throws(() => {
    new Agent({ keepAliveTimeoutBuffer: 'foo' });
  }, { code: 'ERR_INVALID_ARG_TYPE' });
}
