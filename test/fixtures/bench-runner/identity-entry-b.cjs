'use strict';

const { suite } = require('node:bench');
const { setImmediate } = require('timers/promises');
const registerSharedIdentity = require('./identity-shared.cjs');

suite('shared suite', async () => {
  await setImmediate();
  registerSharedIdentity();
});
