// https://github.com/nodejs/node/issues/39447
'use strict';
const common = require('../common');
const {
  Transform,
  pipeline,
} = require('stream');
const assert = require('assert');

function createTransformStream () {
  return async function*(stream) {
      yield 'test';
      throw new Error('Artificial Error');
  };
}

const ts = createTransformStream();

pipeline(ts, process.stdout, common.mustCall((err) => {
  assert.ok(err, 'should have an error');
}));
