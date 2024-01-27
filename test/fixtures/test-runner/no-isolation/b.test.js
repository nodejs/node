'use strict';
const test = require('node:test');

test('b', async () => {
  globalThis.aCb();
  delete globalThis.aCb;
});
