'use strict';

const assert = require('node:assert');

assert.strictEqual(process.entrypoint.href, process.env.NODE_TEST_ENTRYPOINT);
