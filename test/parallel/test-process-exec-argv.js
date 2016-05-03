'use strict';
require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  process.stdout.write(JSON.stringify(process.execArgv));
} else {
  var execArgv = ['--harmony_proxies', '--stack-size=256'];
  var args = [__filename, 'child', 'arg0'];
  var child = spawn(process.execPath, execArgv.concat(args));
  var out = '';

  child.stdout.on('data', function(chunk) {
    out += chunk;
  });

  child.on('close', function() {
    assert.deepStrictEqual(JSON.parse(out), execArgv);
  });
}
