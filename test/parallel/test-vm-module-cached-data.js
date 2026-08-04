'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');

const assert = require('assert');
const { SourceTextModule } = require('vm');

{
  const m = new SourceTextModule('const a = 1');
  const cachedData = m.createCachedData();

  new SourceTextModule('const a = 1', { cachedData });

  assert.throws(() => {
    new SourceTextModule('differentSource', { cachedData });
  }, {
    code: 'ERR_VM_MODULE_CACHED_DATA_REJECTED',
  });
}

assert.rejects(async () => {
  const m = new SourceTextModule('const a = 1');
  await m.link(() => {});
  m.evaluate();
  m.createCachedData();
}, {
  code: 'ERR_VM_MODULE_CANNOT_CREATE_CACHED_DATA',
}).then(common.mustCall());
