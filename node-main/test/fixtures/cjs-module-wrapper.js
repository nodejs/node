'use strict';
const assert = require('assert');
const m = require('module');

global.mwc = 0;

const originalWrapper = m.wrapper;
const patchedWrapper = {...m.wrapper};

patchedWrapper[0] += 'global.mwc = (global.mwc || 0 ) + 1';

// Storing original version of wrapper function
m.wrapper = patchedWrapper;

require('./not-main-module.js');

assert.strictEqual(mwc, 1);

// Restoring original wrapper function
m.wrapper = originalWrapper;
// Cleaning require cache
delete require.cache[require.resolve('./not-main-module.js')];
delete global.mwc;
