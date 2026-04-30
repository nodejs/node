'use strict';
const common = require('../common');
const { addresses } = require('../common/internet');
const assert = require('assert');
const dns = require('dns');
const domain = require('domain');

[
  'resolve4',
  'resolve6',
  'resolveCname',
  'resolveMx',
  'resolveNs',
  'resolveTlsa',
  'resolveTxt',
  'resolveSrv',
  'resolvePtr',
  'resolveNaptr',
  'resolveSoa',
].forEach(function(method) {
  const d = domain.create();
  d.run(common.mustCall(() => {
    dns[method](addresses.INET_HOST, common.mustCall(() => {
      assert.strictEqual(process.domain, d, `${method} retains domain`);
    }));
  }));
});
