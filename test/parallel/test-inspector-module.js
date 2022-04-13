'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { Session } = require('inspector');

const session = new Session();

assert.throws(
  () => session.post('Runtime.evaluate', { expression: '2 + 2' }),
  {
    code: 'ERR_INSPECTOR_NOT_CONNECTED',
    name: 'Error',
    message: 'Session is not connected'
  }
);

session.connect();
session.post('Runtime.evaluate', { expression: '2 + 2' });

[1, {}, [], true, Infinity, undefined].forEach((i) => {
  assert.throws(
    () => session.post(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message:
        'The "method" argument must be of type string.' +
        common.invalidArgTypeHelper(i)
    }
  );
});

[1, true, Infinity].forEach((i) => {
  assert.throws(
    () => session.post('test', i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message:
        'The "params" argument must be of type object.' +
        common.invalidArgTypeHelper(i)
    }
  );
});

[1, 'a', {}, [], true, Infinity].forEach((i) => {
  assert.throws(
    () => session.post('test', {}, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );
});

assert.throws(
  () => session.connect(),
  {
    code: 'ERR_INSPECTOR_ALREADY_CONNECTED',
    name: 'Error',
    message: 'The inspector session is already connected'
  }
);

session.disconnect();
// Calling disconnect twice should not throw.
session.disconnect();
