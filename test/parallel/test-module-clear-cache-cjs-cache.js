// Flags: --expose-internals
// Verifies that CJS Module instances created when a CommonJS file is loaded
// via import() are garbage-collectible after clearCache. Uses
// checkIfCollectableByCounting for a robust GC-level check rather than
// asserting on internal cache state.
'use strict';

const common = require('../common');

const { pathToFileURL } = require('node:url');
const { clearCache } = require('node:module');
const { Module } = require('internal/modules/cjs/loader');
const { checkIfCollectableByCounting } = require('../common/gc');

const fixtureURL = new URL(
  '../fixtures/module-cache/cjs-counter.js',
  pathToFileURL(__filename),
);
const parentURL = pathToFileURL(__filename).href;

const outer = 8;
const inner = 4;

const runIteration = common.mustCallAtLeast(async (i) => {
  for (let j = 0; j < inner; j++) {
    const url = `${fixtureURL.href}?v=${i}-${j}`;
    await import(url);
    clearCache(url, {
      parentURL,
      resolver: 'import',
    });
  }
  return inner;
});

checkIfCollectableByCounting(runIteration, Module, outer)
  .then(common.mustCall(() => {
    delete globalThis.__module_cache_cjs_counter;
  }));
