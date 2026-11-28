'use strict';

const common = require('../common');

const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { clearCache } = require('node:module');
const Module = require('node:module');
const { checkIfCollectableByCounting } = require('../common/gc');

// require(esm) creates a CJS cache entry that wraps the ES module. After
// clearCache, that wrapper (and the loaded module) must be collectible.
const fixture = path.join(__dirname, '..', 'fixtures', 'module-cache', 'esm-counter.mjs');

const outer = 8;
const inner = 4;

checkIfCollectableByCounting(() => {
  for (let i = 0; i < inner; i++) {
    require(fixture);
    clearCache(fixture, {
      parentURL: pathToFileURL(__filename),
      resolver: 'require',
    });
  }
  delete globalThis.__module_cache_esm_counter;
  return inner;
}, Module, outer).then(common.mustCall());
