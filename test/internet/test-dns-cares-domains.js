'use strict';
require('../common');
const assert = require('assert');
const dns = require('dns');
const domain = require('domain');

const methods = [
  'resolve4',
  'resolve6',
  'resolveCname',
  'resolveMx',
  'resolveNs',
  'resolveTxt',
  'resolveSrv',
  'resolvePtr',
  'resolveNaptr',
  'resolveSoa'
];

methods.forEach(function(method) {
  const d = domain.create();
  d.run(function() {
    dns[method]('google.com', function() {
      assert.strictEqual(process.domain, d, `${method} retains domain`);
    });
  });
});
