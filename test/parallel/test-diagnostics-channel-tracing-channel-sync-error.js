'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.tracingChannel('test');

const expectedError = new Error('test');
const input = { foo: 'bar' };
const thisArg = { baz: 'buz' };

function check(found) {
  assert.deepStrictEqual(found, input);
}

const handlers = {
  start: common.mustCall(check),
  end: common.mustCall(check),
  asyncStart: common.mustNotCall(),
  asyncEnd: common.mustNotCall(),
  error: common.mustCall((found) => {
    check(found);
    assert.deepStrictEqual(found.error, expectedError);
  })
};

channel.subscribe(handlers);
try {
  channel.traceSync(function(err) {
    assert.deepStrictEqual(this, thisArg);
    assert.strictEqual(err, expectedError);
    throw err;
  }, input, thisArg, expectedError);

  throw new Error('It should not reach this error');
} catch (error) {
  assert.deepStrictEqual(error, expectedError);
}
