'use strict';
require('../common');
const assert = require('assert');

// https://github.com/nodejs/node/issues/11480
// This code should not throw a SyntaxError.

const a = 1;
const b = 1;
let c;

((a + 1), {c} = b);

assert.strictEqual(c, undefined);
