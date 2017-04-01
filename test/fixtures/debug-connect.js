'use strict';

const assert = require('assert');
const debug = require('_debugger');
var pid;

setTimeout(function() {
  if (pid) process.kill(pid, 'SIGTERM');
  throw new Error('timeout');
}, 5000).unref();

function connectToDebug() {
  var c = new debug.Client();
  c.connect(5858);
  c.on('break', function(brk) {
    c.reqContinue(function() {});
  });
}

process.on('message', function(m) {
  pid = m.pid;
  connectToDebug();
  process.send({ exit: 'exit' });
});
