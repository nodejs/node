'use strict';

const assert = require('node:assert');

assert.strictEqual(process.env.BASIC, 'Original value');
process.loadEnvFile({ override: true });
assert.strictEqual(process.env.BASIC, 'basic');
