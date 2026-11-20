'use strict';
const assert = require('node:assert');
const { test } = require('node:test');

test('diff', () => assert.deepStrictEqual({ a: 1 }, { a: 2 }));
