'use strict';
require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;
var child = spawn(process.execPath, [], {
  env: Object.assign(process.env, {
    NODE_DEBUG: process.argv[2]
  })
});
var wanted = child.pid + '\n';
var found = '';

child.stdout.setEncoding('utf8');
child.stdout.on('data', function(c) {
  found += c;
});

child.stderr.setEncoding('utf8');
child.stderr.on('data', function(c) {
  console.error('> ' + c.trim().split(/\n/).join('\n> '));
});

child.on('close', function(c) {
  assert(!c);
  assert.equal(found, wanted);
  console.log('ok');
});

setTimeout(function() {
  child.stdin.end('console.log(process.pid)');
});
