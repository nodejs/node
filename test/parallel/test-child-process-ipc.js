'use strict';
var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;

var path = require('path');

var sub = path.join(common.fixturesDir, 'echo.js');

var gotHelloWorld = false;
var gotEcho = false;

var child = spawn(process.argv[0], [sub]);

child.stderr.on('data', function(data) {
  console.log('parent stderr: ' + data);
});

child.stdout.setEncoding('utf8');

child.stdout.on('data', function(data) {
  console.log('child said: ' + JSON.stringify(data));
  if (!gotHelloWorld) {
    console.error('testing for hello world');
    assert.equal('hello world\r\n', data);
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
