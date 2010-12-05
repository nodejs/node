var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;

var path = require('path');

var sub = path.join(common.fixturesDir, 'echo.js');

var gotHelloWorld = false;
var gotEcho = false;

var child = spawn(process.argv[0], [sub]);

child.stderr.addListener('data', function(data) {
  console.log('parent stderr: ' + data);
});

child.stdout.setEncoding('utf8');

child.stdout.addListener('data', function(data) {
  console.log('child said: ' + JSON.stringify(data));
  if (!gotHelloWorld) {
    assert.equal('hello world\r\n', data);
    gotHelloWorld = true;
    child.stdin.write('echo me\r\n');
  } else {
    assert.equal('echo me\r\n', data);
    gotEcho = true;
    child.stdin.end();
  }
});

child.stdout.addListener('end', function(data) {
  console.log('child end');
});


process.addListener('exit', function() {
  assert.ok(gotHelloWorld);
  assert.ok(gotEcho);
});
