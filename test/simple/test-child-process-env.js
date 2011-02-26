var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;

var env = {
  'HELLO': 'WORLD'
};
env.__proto__ = {
  'FOO': 'BAR'
}

var child = spawn('/usr/bin/env', [], {env: env});

var response = '';

child.stdout.setEncoding('utf8');

child.stdout.addListener('data', function(chunk) {
  console.log('stdout: ' + chunk);
  response += chunk;
});

process.addListener('exit', function() {
  assert.ok(response.indexOf('HELLO=WORLD') >= 0);
  assert.ok(response.indexOf('FOO=BAR') >= 0);
});
