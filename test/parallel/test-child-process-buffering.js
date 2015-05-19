'use strict';
var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;

var pwd_called = false;
var childClosed = false;
var childExited = false;

function pwd(callback) {
  var output = '';
  var child = common.spawnPwd();

  child.stdout.setEncoding('utf8');
  child.stdout.on('data', function(s) {
    console.log('stdout: ' + JSON.stringify(s));
    output += s;
  });

  child.on('exit', function(c) {
    console.log('exit: ' + c);
    assert.equal(0, c);
    childExited = true;
  });

  child.on('close', function() {
    callback(output);
    pwd_called = true;
    childClosed = true;
  });
}


pwd(function(result) {
  console.dir(result);
  assert.equal(true, result.length > 1);
  assert.equal('\n', result[result.length - 1]);
});

process.on('exit', function() {
  assert.equal(true, pwd_called);
  assert.equal(true, childExited);
  assert.equal(true, childClosed);
});
