'use strict';
require('../common');
var assert = require('assert');
var cluster = require('cluster');

var OK = 2;

if (cluster.isMaster) {

  var worker = cluster.fork();

  worker.on('exit', function(code) {
    assert.equal(code, OK);
    process.exit(0);
  });

  const result = worker.send('SOME MESSAGE');
  assert.strictEqual(result, true);

  return;
}

// Messages sent to a worker will be emitted on both the process object and the
// process.worker object.

assert(cluster.isWorker);

var sawProcess;
var sawWorker;

process.on('message', function(m) {
  assert(!sawProcess);
  sawProcess = true;
  check(m);
});

cluster.worker.on('message', function(m) {
  assert(!sawWorker);
  sawWorker = true;
  check(m);
});

var messages = [];

function check(m) {
  messages.push(m);

  if (messages.length < 2) return;

  assert.deepStrictEqual(messages[0], messages[1]);

  cluster.worker.once('error', function(e) {
    assert.equal(e, 'HI');
    process.exit(OK);
  });

  process.emit('error', 'HI');
}
