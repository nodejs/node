'use strict';

const assert = require('node:assert');

assert.strictEqual(process.env.BASIC, 'Original value');
process.loadEnvFile(process.argv[2]);
assert.strictEqual(process.env.BASIC, 'Original value');
