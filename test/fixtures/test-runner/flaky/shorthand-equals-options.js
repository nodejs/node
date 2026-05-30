'use strict';
const { it } = require('node:test');
const assert = require('node:assert');
let a = 0;
let b = 0;
it.flaky('shorthand form', () => { a++; assert.strictEqual(a, 2); });
it('options form', { flaky: true }, () => { b++; assert.strictEqual(b, 2); });
