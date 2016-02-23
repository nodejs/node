'use strict';
var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;

var env = {
  'HELLO': 'WORLD'
};
Object.setPrototypeOf(env, {
  'FOO': 'BAR'
});

var child;
if (common.isWindows) {
  child = spawn('cmd.exe', ['/c', 'set'], {env: env});
} else {
  child = spawn('/usr/bin/env', [], {env: env});
}


var response = '';

child.stdout.setEncoding('utf8');

child.stdout.on('data', function(chunk) {
  console.log('stdout: ' + chunk);
  response += chunk;
});

process.on('exit', function() {
  assert.ok(response.indexOf('HELLO=WORLD') >= 0);
  assert.ok(response.indexOf('FOO=BAR') >= 0);
});
