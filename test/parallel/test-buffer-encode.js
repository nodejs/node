'use strict';

require('../common');
const assert = require('assert');

assert.strictEqual(Buffer.encode('test', 'hex'), '74657374');

assert.strictEqual(Buffer.encode('test', 'base64'), 'dGVzdA==');

