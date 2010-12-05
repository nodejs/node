var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;

var cat = spawn('cat');
cat.stdin.write('hello');
cat.stdin.write(' ');
cat.stdin.write('world');
cat.stdin.end();

var response = '';
var exitStatus = -1;

var gotStdoutEOF = false;

cat.stdout.setEncoding('utf8');
cat.stdout.addListener('data', function(chunk) {
  console.log('stdout: ' + chunk);
  response += chunk;
});

cat.stdout.addListener('end', function() {
  gotStdoutEOF = true;
});


var gotStderrEOF = false;

cat.stderr.addListener('data', function(chunk) {
  // shouldn't get any stderr output
  assert.ok(false);
});

cat.stderr.addListener('end', function(chunk) {
  gotStderrEOF = true;
});


cat.addListener('exit', function(status) {
  console.log('exit event');
  exitStatus = status;
  assert.equal('hello world', response);
});

process.addListener('exit', function() {
  assert.equal(0, exitStatus);
  assert.equal('hello world', response);
});
