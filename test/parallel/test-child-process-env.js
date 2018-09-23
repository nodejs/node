'use strict';
var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;

var env = {
  'HELLO': 'WORLD'
};
env.__proto__ = {
  'FOO': 'BAR'
};

if (common.isWindows) {
  var child = spawn('cmd.exe', ['/c', 'set'], {env: env});
} else {
  var child = spawn('/usr/bin/env', [], {env: env});
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
