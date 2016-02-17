'use strict';
require('../common');
var assert = require('assert');
var dns = require('dns');
var domain = require('domain');

var methods = [
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
  var d = domain.create();
  d.run(function() {
    dns[method]('google.com', function() {
      assert.strictEqual(process.domain, d, method + ' retains domain');
    });
  });
});
