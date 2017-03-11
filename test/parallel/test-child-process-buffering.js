'use strict';
const common = require('../common');
const assert = require('assert');

function pwd(callback) {
  let output = '';
  const child = common.spawnPwd();

  child.stdout.setEncoding('utf8');
  child.stdout.on('data', function(s) {
    console.log('stdout: ' + JSON.stringify(s));
    output += s;
  });

  child.on('exit', common.mustCall(function(c) {
    console.log('exit: ' + c);
    assert.equal(0, c);
  }));

  child.on('close', common.mustCall(function() {
    callback(output);
  }));
}


pwd(function(result) {
  console.dir(result);
  assert.equal(true, result.length > 1);
  assert.equal('\n', result[result.length - 1]);
});
