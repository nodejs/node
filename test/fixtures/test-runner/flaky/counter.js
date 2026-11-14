'use strict';
const { test } = require('node:test');
test('flaky but passes first try', { flaky: 3 }, () => {});
test('plain not flaky', () => {});
test('flaky:true passes first try', { flaky: true }, () => {});
