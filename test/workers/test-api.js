// Flags: --experimental-workers
'use strict';

var common = require('../common');
common.use();
var assert = require('assert');
var Worker = require('worker');
var checks = 0;

if (process.isMainThread) {
  try {
    Worker();
  } catch (e) {checks++;}

  try {
    Worker(1);
  } catch (e) {checks++;}

  try {
    Worker('str');
  } catch (e) {checks++;}

  try {
    new Worker();
  } catch (e) {checks++;}

  try {
    new Worker(1);
  } catch (e) {checks++;}

  var aWorker = new Worker(__filename, {keepAlive: false});

  aWorker.postMessage({hello: 'world'});
  aWorker.on('message', function(data) {
    assert.deepEqual({hello: 'world'}, data);
    checks++;
  });
  aWorker.on('exit', function() {
    checks++;
    try {
      aWorker.postMessage([]);
    } catch (e) {checks++;}
    try {
      aWorker.terminate();
    } catch (e) {checks++;}
  });
  process.on('beforeExit', function() {
    assert.equal(9, checks);
  });
} else {
  Worker.on('message', function(data) {
    assert.deepEqual({hello: 'world'}, data);
    Worker.postMessage({hello: 'world'});
  });
}
