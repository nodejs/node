'use strict';
var common = require('../common');
var assert = require('assert');

//messages
var PREFIX = 'NODE_';
var normal = {cmd: 'foo' + PREFIX};
var internal = {cmd: PREFIX + 'bar'};

if (process.argv[2] === 'child') {
  //send non-internal message containing PREFIX at a non prefix position
  process.send(normal);

  //send inernal message
  process.send(internal);

  process.exit(0);

} else {

  var fork = require('child_process').fork;
  var child = fork(process.argv[1], ['child']);

  var gotNormal;
  child.once('message', function(data) {
    gotNormal = data;
  });

  var gotInternal;
  child.once('internalMessage', function(data) {
    gotInternal = data;
  });

  process.on('exit', function() {
    assert.deepEqual(gotNormal, normal);
    assert.deepEqual(gotInternal, internal);
  });
}
