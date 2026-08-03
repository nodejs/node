'use strict';

require('../common');
const { strictEqual } = require('assert');
const util = require('util');

strictEqual(TextDecoder, util.TextDecoder);
strictEqual(TextEncoder, util.TextEncoder);
