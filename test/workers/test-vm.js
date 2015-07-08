// Flags: --experimental-workers --harmony_proxies
'use strict';

var assert = require('assert');
var util = require('util');
var Worker = require('worker');
var common = require('../common');
var checks = 0;

var tests = [
  'test/parallel/test-vm-basic.js',
  'test/parallel/test-vm-context-async-script.js',
  'test/parallel/test-vm-context.js',
  'test/parallel/test-vm-context-property-forwarding.js',
  'test/parallel/test-vm-create-and-run-in-context.js',
  'test/parallel/test-vm-create-context-accessors.js',
  'test/parallel/test-vm-create-context-arg.js',
  'test/parallel/test-vm-create-context-circular-reference.js',
  'test/parallel/test-vm-cross-context.js',
  'test/parallel/test-vm-debug-context.js',
  'test/parallel/test-vm-function-declaration.js',
  'test/parallel/test-vm-global-define-property.js',
  'test/parallel/test-vm-global-identity.js',
  'test/parallel/test-vm-harmony-proxies.js',
  'test/parallel/test-vm-harmony-symbols.js',
  'test/parallel/test-vm-is-context.js',
  'test/parallel/test-vm-new-script-new-context.js',
  'test/parallel/test-vm-new-script-this-context.js',
  // Needs --expose-gc but exposing it for the process causes other tests
  // to fail
  // 'test/parallel/test-vm-run-in-new-context.js',
  'test/parallel/test-vm-static-this.js',
  'test/parallel/test-vm-timeout.js'
];

var parallelism = 4;
var testsPerThread = Math.ceil(tests.length / parallelism);
for (var i = 0; i < parallelism; ++i) {
  var shareOfTests = tests.slice(i * testsPerThread, (i + 1) * testsPerThread);
  var cur = Promise.resolve();
  shareOfTests.forEach(function(testFile) {
    cur = cur.then(function() {
      return common.runTestInsideWorker(testFile);
    });
  });
}
