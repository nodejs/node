'use strict';
require('../common');
const assert = require('assert');
const url = require('url');

assert.strictEqual(url.parse('x://0.0,1.1/').host, '0.0,1.1');
