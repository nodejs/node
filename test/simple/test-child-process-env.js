var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;
var child = spawn('/usr/bin/env', [], {env: {'HELLO': 'WORLD'}});

var response = '';

child.stdout.setEncoding('utf8');

child.stdout.addListener('data', function(chunk) {
  console.log('stdout: ' + chunk);
  response += chunk;
});

process.addListener('exit', function() {
  assert.ok(response.indexOf('HELLO=WORLD') >= 0);
});
