'use strict';
const common = require('../common');
const assert = require('assert');

const spawn = require('child_process').spawn;
const child = spawn(process.execPath, [], {
  env: Object.assign(process.env, {
    NODE_DEBUG: process.argv[2]
  })
});
const wanted = child.pid + '\n';
var found = '';

child.stdout.setEncoding('utf8');
child.stdout.on('data', function(c) {
  found += c;
});

child.stderr.setEncoding('utf8');
child.stderr.on('data', function(c) {
  console.error('> ' + c.trim().split(/\n/).join('\n> '));
});

child.on('close', common.mustCall(function(c) {
  assert.strictEqual(c, 0);
  assert.strictEqual(found, wanted);
  console.log('ok');
}));

setTimeout(function() {
  child.stdin.end('console.log(process.pid)');
}, 1);
