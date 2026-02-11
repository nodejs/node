'use strict';

const common = require('../common');

const path = require('node:path');
const Module = require('node:module');
const { clearCache } = require('node:module');
const { checkIfCollectableByCounting } = require('../common/gc');

const fixture = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-counter.js');

const outer = 16;
const inner = 64;

checkIfCollectableByCounting(() => {
  for (let i = 0; i < inner; i++) {
    require(fixture);
    clearCache(fixture, { mode: 'commonjs' });
  }
  return inner;
}, Module, outer).then(common.mustCall(() => {
  delete globalThis.__module_cache_cjs_counter;
}));
