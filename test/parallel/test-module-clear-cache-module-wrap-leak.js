// Flags: --expose-internals
// Verifies that ModuleWrap instances created by import() are garbage-
// collectible after clearCache with caches: 'all'. This is the strongest
// guarantee that clearing works end-to-end: V8 itself can reclaim the
// underlying module.
'use strict';

const common = require('../common');

const { pathToFileURL } = require('node:url');
const { clearCache } = require('node:module');
const { internalBinding } = require('internal/test/binding');
const { ModuleWrap } = internalBinding('module_wrap');
const { checkIfCollectableByCounting } = require('../common/gc');

const fixtureURL = new URL(
  '../fixtures/module-cache/esm-counter.mjs',
  pathToFileURL(__filename),
);
const parentURL = pathToFileURL(__filename).href;

const outer = 8;
const inner = 4;

const runIteration = common.mustCallAtLeast(async (i) => {
  for (let j = 0; j < inner; j++) {
    const url = `${fixtureURL.href}?hmr=${i}-${j}`;
    await import(url);
    clearCache(url, {
      parentURL,
      resolver: 'import',
      caches: 'all',
    });
  }
  return inner;
});

checkIfCollectableByCounting(runIteration, ModuleWrap, outer)
  .then(common.mustCall(() => {
    delete globalThis.__module_cache_esm_counter;
  }));
