'use strict';
const common = require('../common');
const assert = require('assert');

const spawn = require('child_process').spawn;

process.env.HELLO = 'WORLD';

let child;
if (common.isWindows) {
  child = spawn('cmd.exe', ['/c', 'set'], {});
} else {
  child = spawn('/usr/bin/env', [], {});
}

let response = '';

child.stdout.setEncoding('utf8');

child.stdout.on('data', function(chunk) {
  console.log('stdout: ' + chunk);
  response += chunk;
});

process.on('exit', function() {
  assert.ok(response.indexOf('HELLO=WORLD') >= 0,
            'spawn did not use process.env as default');
});
