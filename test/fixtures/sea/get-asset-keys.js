'use strict';

const { isSea, getAssetKeys } = require('node:sea');
const assert = require('node:assert');

assert(isSea());

const keys = getAssetKeys();
console.log('Asset keys:', JSON.stringify(keys.sort()));
