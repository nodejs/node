'use strict';
const common = require('../common');
const assert = require('assert');

//messages
const PREFIX = 'NODE_';
const normal = {cmd: `foo${PREFIX}`};
const internal = {cmd: `${PREFIX}bar`};

if (process.argv[2] === 'child') {
  //send non-internal message containing PREFIX at a non prefix position
  process.send(normal);

  //send internal message
  process.send(internal);

  process.exit(0);

} else {

  const fork = require('child_process').fork;
  const child = fork(process.argv[1], ['child']);

  child.once('message', common.mustCall(function(data) {
    assert.deepStrictEqual(data, normal);
  }));

  child.once('internalMessage', common.mustCall(function(data) {
    assert.deepStrictEqual(data, internal);
  }));
}
