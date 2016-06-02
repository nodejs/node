'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

if (process.argv[2] === 'child') {
  function serializer(data) {
    return JSON.stringify(data, (key, value) => {
      if (value === undefined)
        return '[undefined]';

      return value;
    });
  }

  process.send({foo: 42, bar: undefined}, null, {serializer});
} else {
  const child = cp.fork(process.argv[1], ['child']);

  child.on('message', common.mustCall(function(msg) {
    assert.deepStrictEqual(msg, {foo: 42, bar: '[undefined]'});
  }));
}
