'use strict';
require('../common');
const assert = require('assert');

// Turn off no-restricted-properties because we are testing deepEqual!
/* eslint-disable no-restricted-properties */

// See https://github.com/nodejs/node/issues/10258
{
  const date = new Date('2016');
  function FakeDate() {}
  FakeDate.prototype = Date.prototype;
  const fake = new FakeDate();

  assert.deepEqual(date, fake);
  assert.deepEqual(fake, date);

  // For deepStrictEqual we check the runtime type,
  // then reveal the fakeness of the fake date
  assert.throws(
    () => assert.deepStrictEqual(date, fake),
    {
      message: 'Input A expected to strictly deep-equal input B:\n' +
               '+ expected - actual\n\n- 2016-01-01T00:00:00.000Z\n+ Date {}'
    }
  );
  assert.throws(
    () => assert.deepStrictEqual(fake, date),
    {
      message: 'Input A expected to strictly deep-equal input B:\n' +
               '+ expected - actual\n\n- Date {}\n+ 2016-01-01T00:00:00.000Z'
    }
  );
}

{  // At the moment global has its own type tag
  const fakeGlobal = {};
  Object.setPrototypeOf(fakeGlobal, Object.getPrototypeOf(global));
  for (const prop of Object.keys(global)) {
    fakeGlobal[prop] = global[prop];
  }
  assert.deepEqual(fakeGlobal, global);
  // Message will be truncated anyway, don't validate
  assert.throws(() => assert.deepStrictEqual(fakeGlobal, global),
                assert.AssertionError);
}

{ // At the moment process has its own type tag
  const fakeProcess = {};
  Object.setPrototypeOf(fakeProcess, Object.getPrototypeOf(process));
  for (const prop of Object.keys(process)) {
    fakeProcess[prop] = process[prop];
  }
  assert.deepEqual(fakeProcess, process);
  // Message will be truncated anyway, don't validate
  assert.throws(() => assert.deepStrictEqual(fakeProcess, process),
                assert.AssertionError);
}
/* eslint-enable */
