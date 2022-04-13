// Flags: --no-warnings
'use strict';
require('../common');
const test = require('node:test');

test('pass');
test('never resolving promise', () => new Promise(() => {}));
test('fail');
