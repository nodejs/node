'use strict';

const common = require('../common');
const assert = require('assert');

const spawn = require('child_process').spawn;

const path = require('path');

const sub = path.join(common.fixturesDir, 'echo.js');

let gotHelloWorld = false;
let gotEcho = false;

const child = spawn(process.argv[0], [sub]);

child.stderr.on('data', function(data) {
  console.log('parent stderr: ' + data);
});

child.stdout.setEncoding('utf8');

child.stdout.on('data', function(data) {
  console.log('child said: ' + JSON.stringify(data));
  if (!gotHelloWorld) {
    console.error('testing for hello world');
    assert.strictEqual('hello world\r\n', data);
    gotHelloWorld = true;
    console.error('writing echo me');
    child.stdin.write('echo me\r\n');
  } else {
    console.error('testing for echo me');
    assert.equal('echo me\r\n', data);
    gotEcho = true;
    child.stdin.end();
  }
});

child.stdout.on('end', function(data) {
  console.log('child end');
});


process.on('exit', function() {
  assert.ok(gotHelloWorld);
  assert.ok(gotEcho);
});
