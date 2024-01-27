'use strict';
const test = require('node:test');

test('a', (t, cb) => { globalThis.aCb = cb; });
