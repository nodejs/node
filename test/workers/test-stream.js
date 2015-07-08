// Flags: --experimental-workers
'use strict';

var assert = require('assert');
var util = require('util');
var Worker = require('worker');
var common = require('../common');
var checks = 0;

var tests = [
  'test/parallel/test-stream2-base64-single-char-read-end.js',
  'test/parallel/test-stream2-compatibility.js',
  'test/parallel/test-stream2-finish-pipe.js',
  'test/parallel/test-stream2-large-read-stall.js',
  'test/parallel/test-stream2-objects.js',
  'test/parallel/test-stream2-pipe-error-handling.js',
  'test/parallel/test-stream2-pipe-error-once-listener.js',
  'test/parallel/test-stream2-push.js',
  'test/parallel/test-stream2-readable-empty-buffer-no-eof.js',
  'test/parallel/test-stream2-readable-from-list.js',
  'test/parallel/test-stream2-readable-legacy-drain.js',
  'test/parallel/test-stream2-readable-non-empty-end.js',
  'test/parallel/test-stream2-readable-wrap-empty.js',
  'test/parallel/test-stream2-readable-wrap.js',
  'test/parallel/test-stream2-read-sync-stack.js',
  'test/parallel/test-stream2-set-encoding.js',
  'test/parallel/test-stream2-transform.js',
  'test/parallel/test-stream2-unpipe-drain.js',
  'test/parallel/test-stream2-unpipe-leak.js',
  'test/parallel/test-stream2-writable.js',
  'test/parallel/test-stream3-pause-then-read.js',
  'test/parallel/test-stream-big-packet.js',
  'test/parallel/test-stream-big-push.js',
  'test/parallel/test-stream-duplex.js',
  'test/parallel/test-stream-end-paused.js',
  'test/parallel/test-stream-ispaused.js',
  'test/parallel/test-stream-pipe-after-end.js',
  'test/parallel/test-stream-pipe-cleanup.js',
  'test/parallel/test-stream-pipe-error-handling.js',
  'test/parallel/test-stream-pipe-event.js',
  'test/parallel/test-stream-push-order.js',
  'test/parallel/test-stream-push-strings.js',
  'test/parallel/test-stream-readable-constructor-set-methods.js',
  'test/parallel/test-stream-readable-event.js',
  'test/parallel/test-stream-readable-flow-recursion.js',
  'test/parallel/test-stream-transform-constructor-set-methods.js',
  'test/parallel/test-stream-transform-objectmode-falsey-value.js',
  'test/parallel/test-stream-transform-split-objectmode.js',
  'test/parallel/test-stream-unshift-empty-chunk.js',
  'test/parallel/test-stream-unshift-read-race.js',
  'test/parallel/test-stream-writable-change-default-encoding.js',
  'test/parallel/test-stream-writable-constructor-set-methods.js',
  'test/parallel/test-stream-writable-decoded-encoding.js',
  'test/parallel/test-stream-writev.js'
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
