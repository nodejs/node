// Flags: --test --test-reporter=junit
'use strict';
const test = require('node:test');

test('quote"only', () => {});
test('quote&quot;only', () => {});
test('amp&and"quote"', () => {});
test('lt<and"quote', () => {});
test('line\n"break', () => {});
