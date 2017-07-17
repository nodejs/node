// Flags: --expose-http2
'use strict';

require('../common');
const assert = require('assert');

assert.doesNotThrow(() => require('http2'));
