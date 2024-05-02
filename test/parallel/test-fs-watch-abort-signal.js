// Flags: --expose-internals
'use strict';

// Verify that AbortSignal integration works for fs.watch

const common = require('../common');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

const fs = require('fs');
const fixtures = require('../common/fixtures');


{
  // Signal aborted after creating the watcher
  const file = fixtures.path('empty.js');
  const ac = new AbortController();
  const { signal } = ac;
  const watcher = fs.watch(file, { signal });
  watcher.once('close', common.mustCall());
  setImmediate(() => ac.abort());
}
{
  // Signal aborted before creating the watcher
  const file = fixtures.path('empty.js');
  const signal = AbortSignal.abort();
  const watcher = fs.watch(file, { signal });
  watcher.once('close', common.mustCall());
}
