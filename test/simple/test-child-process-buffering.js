var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;

var pwd_called = false;

function pwd(callback) {
  var output = '';
  var child = spawn('pwd');

  child.stdout.setEncoding('utf8');
  child.stdout.addListener('data', function(s) {
    console.log('stdout: ' + JSON.stringify(s));
    output += s;
  });

  child.addListener('exit', function(c) {
    console.log('exit: ' + c);
    assert.equal(0, c);
    callback(output);
    pwd_called = true;
  });
}


pwd(function(result) {
  console.dir(result);
  assert.equal(true, result.length > 1);
  assert.equal('\n', result[result.length - 1]);
});

process.addListener('exit', function() {
  assert.equal(true, pwd_called);
});
