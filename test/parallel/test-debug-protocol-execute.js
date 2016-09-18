'use strict';

require('../common');
const assert = require('assert');
const debug = require('_debugger');

const protocol = new debug.Protocol();

assert.strictEqual(protocol.state, 'headers');

protocol.execute('Content-Length: 10\r\n\r\nfhqwhgads');

assert.strictEqual(protocol.state, 'body');
assert.strictEqual(protocol.res.body, undefined);

protocol.state = 'sterrance';
assert.throws(
  () => { protocol.execute('grumblecakes'); },
  /^Error: Unknown state$/
);
