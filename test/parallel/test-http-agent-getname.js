'use strict';

require('../common');
const assert = require('assert');
const http = require('http');

const agent = new http.Agent();

// default to localhost
assert.strictEqual(
  agent.getName({
    port: 80,
    localAddress: '192.168.1.1'
  }),
  'localhost:80:192.168.1.1'
);

// empty
assert.strictEqual(
  agent.getName({}),
  'localhost::'
);

// pass all arguments
assert.strictEqual(
  agent.getName({
    host: '0.0.0.0',
    port: 80,
    localAddress: '192.168.1.1'
  }),
  '0.0.0.0:80:192.168.1.1'
);

for (const family of [0, null, undefined, 'bogus'])
  assert.strictEqual(agent.getName({ family }), 'localhost::');

for (const family of [4, 6])
  assert.strictEqual(agent.getName({ family }), `localhost:::${family}`);
