'use strict';
const common = require('../common');
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

  child.once('message', common.mustCall(function(data) {
    assert.deepStrictEqual(data, normal);
  }));

  child.once('internalMessage', common.mustCall(function(data) {
    assert.deepStrictEqual(data, internal);
  }));
}
